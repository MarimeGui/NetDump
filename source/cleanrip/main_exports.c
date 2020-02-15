#include "cleanrip/main_exports.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <malloc.h>
#include <stdarg.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>
#include "cleanrip/gc_dvd_exports.h"

static char gameName[32];
static char internalName[512];
static int dumpCounter = 0;

int have_ahbprot() {
    if (read32(HW_ARMIRQMASK) && read32(HW_ARMIRQFLAG)) {
        // disable DVD irq for starlet
        mask32(HW_ARMIRQMASK, 1<<18, 0);
        return 1;
    }
    return 0;
}

int find_ios(u32 ios) {
    s32 ret;
    u32 n;

    u64 *titles = NULL;
    u32 num_titles = 0;

    ret = ES_GetNumTitles(&num_titles);
    if (ret < 0)
        return 0;

    if (num_titles < 1)
        return 0;

    titles = (u64 *) memalign(32, num_titles * sizeof(u64) + 32);
    if (!titles)
        return 0;

    ret = ES_GetTitles(titles, num_titles);
    if (ret < 0) {
        free(titles);
        return 0;
    }

    for (n = 0; n < num_titles; n++) {
        if ((titles[n] & 0xFFFFFFFF) == ios) {
            free(titles);
            return 1;
        }
    }
    free(titles);
    return 0;
}

int identify_disc() {
    char readbuf[2048] __attribute__((aligned(32)));

    memset(&internalName[0],0,512);
    // Read the header
    DVD_LowRead64(readbuf, 2048, 0ULL);
    if (readbuf[0]) {
        strncpy(&gameName[0], readbuf, 6);
        gameName[6] = 0;
        // Multi Disc identifier support
        if (readbuf[6]) {
            size_t lastPos = strlen(gameName);
            sprintf(&gameName[lastPos], "-disc%i", (readbuf[6]) + 1);
        }
        strncpy(&internalName[0],&readbuf[32],512);
        internalName[511] = '\0';
    } else {
        sprintf(&gameName[0], "disc%i", dumpCounter);
    }
    if ((*(volatile u32*) (readbuf+0x1C)) == NGC_MAGIC) {
        return IS_NGC_DISC;
    }
    if ((*(volatile u32*) (readbuf+0x18)) == WII_MAGIC) {
        return IS_WII_DISC;
    } else {
        return IS_UNK_DISC;
    }
}
