#pragma once

#ifndef NULL
#define NULL 0
#endif

#if __ARCH_BITS__ == 64
#elif __ARCH_BITS__ == 32
#else
#error "unsupported arch bits"
#endif

#ifdef __CRT_LIB__
typedef __UINT16_TYPE__ u16;
typedef __UINT32_TYPE__ u32;
typedef __UINT64_TYPE__ u64;
typedef __SIZE_TYPE__ size_t;
#endif

#ifdef __KERNEL__

#include <linux/types.h>
#else

#include <stdint.h>
typedef uint16_t	u16;
typedef uint32_t	u32;
typedef uint64_t	u64;

#endif
