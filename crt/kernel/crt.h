#ifndef __CRT_KERNEL_CRT_H__
#define __CRT_KERNEL_CRT_H__

#include "klog.h"
#include "vfile.h"
#include "malloc.h"
#include "page_alloc.h"

#define CRT_BUG_ON(cond)	\
	BUG_ON(cond)

#define CRT_BUG()		\
	BUG()

#endif
