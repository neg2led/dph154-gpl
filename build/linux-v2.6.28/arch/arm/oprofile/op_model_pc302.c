/*
 * FILE NAME op_model_pc302.c
 *
 * Copyright (c) 2008 ip.access Ltd.
 *
 * BRIEF MODULE DESCRIPTION
 *  Support for async hardware timer for oprofile
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */


/****************************************************************************
 * Standard Library Includes
 ****************************************************************************/
#include <linux/types.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <linux/oprofile.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/smp.h>

/****************************************************************************
 * Kernel Includes
 ****************************************************************************/
#include "op_counter.h"
#include "op_arm_model.h"

/****************************************************************************
  Private Definitions
 ****************************************************************************/
/* Converts a physical address number into a pointer to the virtual location */
#define _ioa(n)     __io(IO_ADDRESS(n))

/****************************************************************************
  Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static irqreturn_t pc302_oprof_interrupt(int irq, void *dev_id);
static int         pc302_oprof_start(void);
static void        pc302_oprof_stop(void);
static int         pc302_oprof_dummy(void);

/****************************************************************************
 * Private Constants
 ****************************************************************************/
/* Use a relatively prime divisor to avoid syncing up to the tick timer */
#define OPROF_RT_LATCH ((11*19*19*53) - 1)  /* Divisor of 210463 vs 200000 for tick */

/* Use timer 2 for the oprofile sample clock */
#define OPROF_TICK_TIMER 2
#define OPROF_TICK_TIMER_IRQ IRQ_TIMER2

/****************************************************************************
 * Exported Variables
 ****************************************************************************/
struct op_arm_model_spec op_pc302_spec = {
    .init           = pc302_oprof_dummy,
    .num_counters   = 1,
    .setup_ctrs     = pc302_oprof_dummy,
    .start          = pc302_oprof_start,
    .stop           = pc302_oprof_stop,
    .name           = "timer",  /* The name must be timer to allow oprofile to analyse results */
};

/****************************************************************************
 * Private Variables (Must be declared static)
 ****************************************************************************/



/****************************************************************************
 * Function Name  : pc302_oprof_interrupt
 * Description    : IRQ handler for the timer.
 *                  Clears interrupt and takes a sample
 ****************************************************************************/
static irqreturn_t pc302_oprof_interrupt(int irq, void *dev_id)
{
    struct pt_regs *regs = get_irq_regs();

    // ...clear the interrupt
    ioread32(_ioa(PC302_TIMER_BASE + TIMERNEOIREGOFFSET(OPROF_TICK_TIMER)));

    oprofile_add_sample(regs, 0);

    return IRQ_HANDLED;
}

/****************************************************************************
 * Function Name  : pc302_oprof_start
 * Description    : Set up the timer and interrupt.
 ****************************************************************************/
static int pc302_oprof_start(void)
{
    int            ret = 0;

    /* Start with the timer disabled */
    iowrite32(0,
            _ioa(PC302_TIMER_BASE + TIMERNCONTROLREGOFFSET(OPROF_TICK_TIMER)));

    /* Set mode  */
    iowrite32(TIMERMODE,
            _ioa(PC302_TIMER_BASE + TIMERNCONTROLREGOFFSET(OPROF_TICK_TIMER)));

    /* Set the reload count that gives us the desired interrupt frequency */
    iowrite32(OPROF_RT_LATCH,
            _ioa(PC302_TIMER_BASE + TIMERNLOADCOUNTREGOFFSET(OPROF_TICK_TIMER)));

    /* Register interrupt handler */
    ret = request_irq(OPROF_TICK_TIMER_IRQ, pc302_oprof_interrupt, IRQF_DISABLED, "OProf Tick", NULL);

    if (ret == 0)
    {
        /* Set user defined count mode, unmask interrupt and enable the timer */
        iowrite32((TIMERENABLE | TIMERMODE),
                _ioa(PC302_TIMER_BASE + TIMERNCONTROLREGOFFSET(OPROF_TICK_TIMER)));
    }

    return ret;
}

/****************************************************************************
 * Function Name  : pc302_oprof_stop
 * Description    : Stop the timer and interrupt.
 ****************************************************************************/
static void pc302_oprof_stop(void)
{
    /* Disable the timer */
    iowrite32(0,
            _ioa(PC302_TIMER_BASE + TIMERNCONTROLREGOFFSET(OPROF_TICK_TIMER)));

    free_irq(OPROF_TICK_TIMER_IRQ, NULL);
}

/****************************************************************************
 * Function Name  : pc302_oprof_dummy
 * Description    : Dummy init and setup function.
 ****************************************************************************/
static int pc302_oprof_dummy(void)
{
    return 0;
}
