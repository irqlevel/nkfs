#pragma once

#define KL_INV_L	0
#define KL_DBG_L 	1
#define KL_INF_L	2
#define KL_WRN_L 	3
#define KL_ERR_L	4
#define KL_MAX_L	5

#define KL_DBG		KL_DBG_L,__LOGNAME__,__SUBCOMPONENT__,__FILE__, __LINE__, __FUNCTION__
#define KL_INF 		KL_INF_L,__LOGNAME__,__SUBCOMPONENT__,__FILE__, __LINE__, __FUNCTION__
#define KL_WRN 		KL_WRN_L,__LOGNAME__,__SUBCOMPONENT__,__FILE__, __LINE__, __FUNCTION__
#define KL_ERR 		KL_ERR_L,__LOGNAME__,__SUBCOMPONENT__,__FILE__, __LINE__, __FUNCTION__

void klog(int level, const char *log_name, const char *subcomp, const char *file, int line, const char *func, const char *fmt, ...);

int klog_init(int level);

void klog_release(void);

#define ENTER_FUNC \
  klog(KL_INFO, "Enter %s", __FUNCTION__);

#define LEAVE_FUNC \
  klog(KL_INFO, "Leave %s", __FUNCTION__);

