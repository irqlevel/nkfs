#pragma once

#include <include/types.h>
#include <include/ds_obj_id.h>
#include <include/ds_net_cmd.h>

#ifndef asmlinkage
#define asmlinkage __attribute__((regparm(0)))

/*
 * Make sure the compiler doesn't do anything stupid with the
 * arguments on the stack - they are owned by the *caller*, not
 * the callee. This just fools gcc into not spilling into them,
 * and keeps it from doing tailcall recursion and/or using the
 * stack slots for temporaries, since they are live and "used"
 * all the way to the end of the function.
 *
 * NOTE! On x86-64, all the arguments are in registers, so this
 * only matters on a 32-bit kernel.
 */
#define asmlinkage_protect(n, ret, args...) \
	__asmlinkage_protect##n(ret, ##args)
#define __asmlinkage_protect_n(ret, args...) \
	__asm__ __volatile__ ("" : "=r" (ret) : "0" (ret), ##args)
#define __asmlinkage_protect0(ret) \
	__asmlinkage_protect_n(ret)
#define __asmlinkage_protect1(ret, arg1) \
	__asmlinkage_protect_n(ret, "m" (arg1))
#define __asmlinkage_protect2(ret, arg1, arg2) \
	__asmlinkage_protect_n(ret, "m" (arg1), "m" (arg2))
#define __asmlinkage_protect3(ret, arg1, arg2, arg3) \
	__asmlinkage_protect_n(ret, "m" (arg1), "m" (arg2), "m" (arg3))
#define __asmlinkage_protect4(ret, arg1, arg2, arg3, arg4) \
	__asmlinkage_protect_n(ret, "m" (arg1), "m" (arg2), "m" (arg3), \
			      "m" (arg4))
#define __asmlinkage_protect5(ret, arg1, arg2, arg3, arg4, arg5) \
	__asmlinkage_protect_n(ret, "m" (arg1), "m" (arg2), "m" (arg3), \
			      "m" (arg4), "m" (arg5))
#define __asmlinkage_protect6(ret, arg1, arg2, arg3, arg4, arg5, arg6) \
	__asmlinkage_protect_n(ret, "m" (arg1), "m" (arg2), "m" (arg3), \
			      "m" (arg4), "m" (arg5), "m" (arg6))
#endif

extern asmlinkage void *crt_memset(void *ptr, int value, size_t num);
extern asmlinkage void *crt_memcpy(void * destination, const void * source, size_t num);
extern asmlinkage void *crt_malloc(size_t size);
extern asmlinkage void crt_free(void *ptr);
extern asmlinkage int crt_random_buf(void *buf, size_t len);
extern asmlinkage size_t crt_strlen(const char * str);
extern asmlinkage void crt_log(int level, const char *file, int line,
	const char *func, const char *fmt, ...);

#include <crtlib/include/obj_id.h>
#include <crtlib/include/char2hex.h>
#include <crtlib/include/error.h>
#include <crtlib/include/sha256.h>
#include <crtlib/include/clog.h>
#include <crtlib/include/net_cmd.h>
#include <crtlib/include/random.h>

