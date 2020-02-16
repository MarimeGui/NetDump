#include <gccore.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <ogcsys.h>

#define READ_SIZE 0x10000

#define WII_MAGIC 0x5D1C9EA3
#define NGC_MAGIC 0xC2339F3D

#define NGC_DISC_SIZE   0x0AE0B0
#define WII_D5_SIZE     0x230480
#define WII_D9_SIZE     0x3F69C0

#define HW_REG_BASE 0xcd800000
#define HW_ARMIRQMASK (HW_REG_BASE + 0x03c)
#define HW_ARMIRQFLAG (HW_REG_BASE + 0x038)

enum discTypes
{
    IS_NGC_DISC=0,
    IS_WII_DISC,
    IS_DATEL_DISC,
    IS_UNK_DISC
};

int have_ahbprot();
int find_ios(u32 ios);
int identify_disc();
void get_game_name(char *out_buf, int *index);
void get_internal_name(char *out_buf, int *index);