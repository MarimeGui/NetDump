#include "cleanrip/gc_dvd_exports.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ogc/dvd.h>
#include <malloc.h>
#include <string.h>
#include <gccore.h>
#include <unistd.h>
#include <di/di.h>
#include <ogc/machine/processor.h>

u32 dvd_hard_init = 0;
static u32 read_cmd = NORMAL;

#ifdef HW_RVL
volatile u32* dvd = (volatile u32*) 0xCD806000;
#else
volatile u32* dvd = (volatile u32*)0xCC006000;
#endif

int init_dvd() {
    // Gamecube Mode
#ifdef HW_DOL
    DVD_Reset(DVD_RESETHARD);
    dvd_read_id();
    if (!dvd_get_error()) {
        xeno_disable();
        return 0; //we're ok
    }
    if (dvd_get_error() >> 24) {
        return NO_DISC;
    }
    return -1;

#endif
    // Wii (Wii mode)
#ifdef HW_RVL
    STACK_ALIGN(u8,id,32,32);
    u32 error;

    // enable GPIO for spin-up on drive reset (active low)
    mask32(0x0D8000E0, 0x10, 0);
    // assert DI reset (active low)
    mask32(0x0D800194, 0x400, 0);
    usleep(1000);
    // deassert DI reset
    mask32(0x0D800194, 0, 0x400);

    error = dvd_get_error();
    if ((error >> 24) == 1) {
        return NO_DISC;
    }

    if ((!dvd_hard_init) || (dvd_get_error())) {
        // read id
        dvd[0] = 0x54;
        dvd[2] = 0xA8000040;
        dvd[3] = 0;
        dvd[4] = 0x20;
        dvd[5] = (u32)id & 0x1FFFFFFF;
        dvd[6] = 0x20;
        dvd[7] = 3;
        while (dvd[7] & 1)
            usleep(20000);
        dvd_hard_init = 1;
    }

    if ((dvd_get_error() & 0xFFFFFF) == 0x053000) {
        read_cmd = DVDR;
    } else {
        read_cmd = NORMAL;
    }
    dvd_read_id();

    return 0;
#endif
}

int dvd_read_id() {
#ifdef HW_RVL
    char readbuf[2048] __attribute__((aligned(32)));
    DVD_LowRead64(readbuf, 2048, 0ULL);
    memcpy((void*)0x80000000, readbuf, 32);
    return 0;
#endif
    dvd[0] = 0x2E;
    dvd[1] = 0;
    dvd[2] = 0xA8000040;
    dvd[3] = 0;
    dvd[4] = 0x20;
    dvd[5] = 0;
    dvd[6] = 0x20;
    dvd[7] = 3; // enable reading!
    while (dvd[7] & 1)
        LWP_YieldThread();
    if (dvd[0] & 0x4)
        return 1;
    return 0;
}

void dvd_read_bca(void* dst)
{
	dvd[2] = 0xDA000000;
	dvd[5] = (unsigned long)dst & 0x1FFFFFFF;
	dvd[6] = 0x40;
	dvd[7] = 3;
	DCInvalidateRange(dst, 64);
	while (dvd[7] & 1);
}

u32 dvd_get_error(void) {
    dvd[2] = 0xE0000000;
    dvd[8] = 0;
    dvd[7] = 1; // IMM
    while (dvd[7] & 1);
    return dvd[8];
}

void dvd_motor_off() {
	dvd[0] = 0x2E;
	dvd[1] = 0;
	dvd[2] = 0xe3000000;
	dvd[3] = 0;
	dvd[4] = 0;
	dvd[5] = 0;
	dvd[6] = 0;
	dvd[7] = 1; // IMM
	while (dvd[7] & 1);
}

/*
 DVD_LowRead64(void* dst, unsigned int len, uint64_t offset)
 Read Raw, needs to be on sector boundaries
 Has 8,796,093,020,160 byte limit (8 TeraBytes)
 Synchronous function.
 return -1 if offset is out of range
 */
int DVD_LowRead64(void* dst, u32 len, uint64_t offset) {
    if (offset >> 2 > 0xFFFFFFFF)
        return -1;

    if ((((u32) dst) & 0xC0000000) == 0x80000000) // cached?
        dvd[0] = 0x2E;
    dvd[1] = 0;
    dvd[2] = read_cmd;
    dvd[3] = read_cmd == DVDR ? offset >> 11 : offset >> 2;
    dvd[4] = read_cmd == DVDR ? len >> 11 : len;
    dvd[5] = (u32) dst & 0x1FFFFFFF;
    dvd[6] = len;
    dvd[7] = 3; // enable reading!
    DCInvalidateRange(dst, len);
    while (dvd[7] & 1)
        LWP_YieldThread();

    if (dvd[0] & 0x4)
        return 1;
    return 0;
}