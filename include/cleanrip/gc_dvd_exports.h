#include <stdint.h>
#include <gccore.h>

#define GC_CPU_VERSION 0x00083214
#define NO_HW_ACCESS -1000
#define NO_DISC      -1001
#define NORMAL 0xA8000000
#define DVDR   0xD0000000

int init_dvd();
int dvd_read_id();
u32 dvd_get_error(void);
int DVD_LowRead64(void* dst, u32 len, uint64_t offset);