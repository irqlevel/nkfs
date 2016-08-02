#ifndef __NKFS_TYPES_H__
#define __NKFS_TYPES_H__

#ifndef NULL
#define NULL 0
#endif

#if __ARCH_BITS__ == 64
#elif __ARCH_BITS__ == 32
#else
#error "unsupported arch bits"
#endif

#ifdef __KERNEL__

#include <linux/types.h>
#include <linux/kernel.h>

#ifndef U64_MAX
#define U64_MAX (u64)(~0ULL)
#endif

#else

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

typedef uint8_t		u8;
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;

#define U64_MAX (u64)(~0ULL)

#endif

#endif
