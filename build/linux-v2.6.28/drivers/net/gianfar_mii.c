/*
 * drivers/net/gianfar_mii.c
 *
 * Gianfar Ethernet Driver -- MIIM bus implementation
 * Provides Bus interface for MIIM regs
 *
 * Author: Andy Fleming
 * Maintainer: Kumar Gala
 *
 * Copyright (c) 2002-2004 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/phy.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include "gianfar.h"
#include "gianfar_mii.h"

/*
 * Write value to the PHY at mii_id at register regnum,
 * on the bus attached to the local interface, which may be different from the
 * generic mdio bus (tied to a single interface), waiting until the write is
 * done before returning. This is helpful in programming interfaces like
 * the TBI which control interfaces like onchip SERDES and are always tied to
 * the local mdio pins, which may not be the same as system mdio bus, used for
 * controlling the external PHYs, for example.
 */
int gfar_local_mdio_write(struct gfar_mii __iomem *regs, int mii_id,
			  int regnum, u16 value)
{
	/* Set the PHY address and the register address we want to write */
	gfar_write(&regs->miimadd, (mii_id << 8) | regnum);

	/* Write out the value we want */
	gfar_write(&regs->miimcon, value);

	/* Wait for the transaction to finish */
	while (gfar_read(&regs->miimind) & MIIMIND_BUSY)
		cpu_relax();

	return 0;
}

/*
 * Read the bus for PHY at addr mii_id, register regnum, and
 * return the value.  Clears miimcom first.  All PHY operation
 * done on the bus attached to the local interface,
 * which may be different from the generic mdio bus
 * This is helpful in programming interfaces like
 * the TBI which, inturn, control interfaces like onchip SERDES
 * and are always tied to the local mdio pins, which may not be the
 * same as system mdio bus, used for controlling the external PHYs, for eg.
 */
int gfar_local_mdio_read(struct gfar_mii __iomem *regs, int mii_id, int regnum)
{
	u16 value;

	/* Set the PHY address and the register address we want to read */
	gfar_write(&regs->miimadd, (mii_id << 8) | regnum);

	/* Clear miimcom, and then initiate a read */
	gfar_write(&regs->miimcom, 0);
	gfar_write(&regs->miimcom, MII_READ_COMMAND);

	/* Wait for the transaction to finish */
	while (gfar_read(&regs->miimind) & (MIIMIND_NOTVALID | MIIMIND_BUSY))
		cpu_relax();

	/* Grab the value of the register from miimstat */
	value = gfar_read(&regs->miimstat);

	return value;
}

/* Write value to the PHY at mii_id at register regnum,
 * on the bus, waiting until the write is done before returning.
 * All PHY configuration is done through the TSEC1 MIIM regs */
int gfar_mdio_write(struct mii_bus *bus, int mii_id, int regnum, u16 value)
{
	struct gfar_mii __iomem *regs = (void __iomem *)bus->priv;

	/* Write to the local MII regs */
	return(gfar_local_mdio_write(regs, mii_id, regnum, value));
}

/* Read the bus for PHY at addr mii_id, register regnum, and
 * return the value.  Clears miimcom first.  All PHY
 * configuration has to be done through the TSEC1 MIIM regs */
int gfar_mdio_read(struct mii_bus *bus, int mii_id, int regnum)
{
	struct gfar_mii __iomem *regs = (void __iomem *)bus->priv;

	/* Read the local MII regs */
	return(gfar_local_mdio_read(regs, mii_id, regnum));
}

/* Reset the MIIM registers, and wait for the bus to free */
static int gfar_mdio_reset(struct mii_bus *bus)
{
	struct gfar_mii __iomem *regs = (void __iomem *)bus->priv;
	unsigned int timeout = PHY_INIT_TIMEOUT;

	mutex_lock(&bus->mdio_lock);

	/* Reset the management interface */
	gfar_write(&regs->miimcfg, MIIMCFG_RESET);

	/* Setup the MII Mgmt clock speed */
	gfar_write(&regs->miimcfg, MIIMCFG_INIT_VALUE);

	/* Wait until the bus is free */
	while ((gfar_read(&regs->miimind) & MIIMIND_BUSY) &&
			--timeout)
		cpu_relax();

	mutex_unlock(&bus->mdio_lock);

	if(timeout == 0) {
		printk(KERN_ERR "%s: The MII Bus is stuck!\n",
				bus->name);
		return -EBUSY;
	}

	return 0;
}


static int gfar_mdio_probe(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gianfar_mdio_data *pdata;
	struct gfar_mii __iomem *regs;
	struct gfar __iomem *enet_regs;
	struct mii_bus *new_bus;
	struct resource *r;
	int i, err = 0;

	if (NULL == dev)
		return -EINVAL;

	new_bus = mdiobus_alloc();
	if (NULL == new_bus)
		return -ENOMEM;

	new_bus->name = "Gianfar MII Bus",
	new_bus->read = &gfar_mdio_read,
	new_bus->write = &gfar_mdio_write,
	new_bus->reset = &gfar_mdio_reset,
	snprintf(new_bus->id, MII_BUS_ID_SIZE, "%x", pdev->id);

	pdata = (struct gianfar_mdio_data *)pdev->dev.platform_data;

	if (NULL == pdata) {
		printk(KERN_ERR "gfar mdio %d: Missing platform data!\n", pdev->id);
		return -ENODEV;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	/* Set the PHY base address */
	regs = ioremap(r->start, sizeof (struct gfar_mii));

	if (NULL == regs) {
		err = -ENOMEM;
		goto reg_map_fail;
	}

	new_bus->priv = (void __force *)regs;

	new_bus->irq = pdata->irq;

	new_bus->parent = dev;
	dev_set_drvdata(dev, new_bus);

	/*
	 * This is mildly evil, but so is our hardware for doing this.
	 * Also, we have to cast back to struct gfar_mii because of
	 * definition weirdness done in gianfar.h.
	 */
	enet_regs = (struct gfar __iomem *)
		((char *)regs - offsetof(struct gfar, gfar_mii_regs));

	/* Scan the bus, looking for an empty spot for TBIPA */
	gfar_write(&enet_regs->tbipa, 0);
	for (i = PHY_MAX_ADDR; i > 0; i--) {
		u32 phy_id;

		err = get_phy_id(new_bus, i, &phy_id);
		if (err)
			goto bus_register_fail;

		if (phy_id == 0xffffffff)
			break;
	}

	/* The bus is full.  We don't support using 31 PHYs, sorry */
	if (i == 0) {
		err = -EBUSY;

		goto bus_register_fail;
	}

	gfar_write(&enet_regs->tbipa, i);

	err = mdiobus_register(new_bus);

	if (0 != err) {
		printk (KERN_ERR "%s: Cannot register as MDIO bus\n",
				new_bus->name);
		goto bus_register_fail;
	}

	return 0;

bus_register_fail:
	iounmap(regs);
reg_map_fail:
	mdiobus_free(new_bus);

	return err;
}


static int gfar_mdio_remove(struct device *dev)
{
	struct mii_bus *bus = dev_get_drvdata(dev);

	mdiobus_unregister(bus);

	dev_set_drvdata(dev, NULL);

	iounmap((void __iomem *)bus->priv);
	bus->priv = NULL;
	mdiobus_free(bus);

	return 0;
}

static struct device_driver gianfar_mdio_driver = {
	.name = "fsl-gianfar_mdio",
	.bus = &platform_bus_type,
	.probe = gfar_mdio_probe,
	.remove = gfar_mdio_remove,
};

static int match_mdio_bus(struct device *dev, void *data)
{
	const struct gfar_private *priv = data;
	const struct platform_device *pdev = to_platform_device(dev);

	return !strcmp(pdev->name, gianfar_mdio_driver.name) &&
		pdev->id == priv->einfo->mdio_bus;
}

/* Given a gfar_priv structure, find the mii_bus controlled by this device (not
 * necessarily the same as the bus the gfar's PHY is on), if one exists.
 * Normally only the first gianfar controls a mii_bus.  */
struct mii_bus *gfar_get_miibus(const struct gfar_private *priv)
{
	/*const*/ struct device *d;

	d = bus_find_device(gianfar_mdio_driver.bus, NULL, (void *)priv,
			    match_mdio_bus);
	return d ? dev_get_drvdata(d) : NULL;
}

int __init gfar_mdio_init(void)
{
	return driver_register(&gianfar_mdio_driver);
}

void gfar_mdio_exit(void)
{
	driver_unregister(&gianfar_mdio_driver);
}
