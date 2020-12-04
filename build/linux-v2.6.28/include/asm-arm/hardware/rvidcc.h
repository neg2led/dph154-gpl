/* 
   Copyright (C) 2004-2007 ARM Limited.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License version 2
   as published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   As a special exception, if other files instantiate templates or use macros
   or inline functions from this file, or you compile this file and link it
   with other works to produce a work based on this file, this file does not
   by itself cause the resulting work to be covered by the GNU General Public
   License. However the source code for this file must still be made available
   in accordance with section (3) of the GNU General Public License.

   This exception does not invalidate any other reasons why a work based on
   this file might be covered by the GNU General Public License.
*/

#ifndef __RVIDCC_H
#define __RVIDCC_H

/* Ioctl defines */
#define DCC_SETPOLLPERIOD  _IOW('T', 1, int) 
#define DCC_GETPOLLPERIOD  _IOR('T', 2, int) 

#endif
