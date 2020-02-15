/**
 * CleanRip - main.c
 * Copyright (C) 2010 emu_kidid
 *
 * Main driving code behind the disc ripper
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

#include "cleanripimport.h"
#include <ogc/machine/processor.h>

int have_ahbprot() {
	if (read32(HW_ARMIRQMASK) && read32(HW_ARMIRQFLAG)) {
		// disable DVD irq for starlet
		mask32(HW_ARMIRQMASK, 1<<18, 0);
		return 1;
	}
	return 0;
}