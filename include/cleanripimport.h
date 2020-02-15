/**
 * CleanRip - main.h
 * Copyright (C) 2010 emu_kidid
 *
 * CleanRip homepage: http://code.google.com/p/cleanrip/
 * email address: emukidid@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

// Modified for my purposes

#define HW_REG_BASE   	0xcd800000
#define HW_ARMIRQMASK 	(HW_REG_BASE + 0x03c)
#define HW_ARMIRQFLAG 	(HW_REG_BASE + 0x038)

int have_ahbprot();