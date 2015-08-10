#include <crt/include/crt.h>

static char *ulog_level_s[] = {"INV", "DBG3", "DBG2", "DBG1", "DBG", "INF" ,
			       "WRN" , "ERR", "FTL", "TST", "MAX"};

static char *ulog_name = NULL;
static char *ulog_path = NULL;
static int ulevel = CL_INV;
static int uprintf = 0;

void crt_log_set_path(char *log_path, char *log_name)
{
	ulog_name = log_name;
	ulog_path = log_path;
}

void crt_log_set_level(int level)
{
	ulevel = level;
}

void crt_log_enable_printf(int enable)
{
	uprintf = !!enable;
}

static void crt_write_fmt_args(char **buff, int *left,
			       const char *fmt, va_list args)
{
	int res;

	if (*left < 0)
		return ;

	res = vsnprintf(*buff,*left,fmt,args);
	if (res > 0) {
		*buff+=res;
		*left-=res;
	}
	return;
}

static void crt_write_fmt(char **buff, int *left, const char *fmt, ...)
{
	va_list args;

	va_start(args,fmt);
	crt_write_fmt_args(buff, left,fmt,args);
	va_end(args);
}

static const char * truncate_file_path(const char *filename)
{
	char *pos;
	pos = strrchr(filename, '/');
	if (pos)
		return ++pos;
	else
		return filename;
}

void crt_log_v(int level, const char *log_name, const char *subcomp,
	       const char *file, int line, const char *func, const char *fmt,
	       va_list args)
{
	char buf[PAGE_SIZE];
	char *pos, *begin;
	int left, count;
	struct timeval tv;
	struct tm tm;
	char *level_s;
	FILE *fp;
	time_t secs;
	char curr_dir[256];
	char log_path[256];
	const char *plog_path;
	const char *plog_name;

	if (level < 0 || level >= ARRAY_SIZE(ulog_level_s))
		return;

	if (level < ulevel)
		return;

	level_s = ulog_level_s[level];
	begin = buf;
	pos = begin;
	count = PAGE_SIZE/sizeof(char);
	left = count - 1;

	gettimeofday(&tv, NULL);
	secs = tv.tv_sec;
	gmtime_r(&secs, &tm);

#define FMT "%04d-%02d-%02d %02d:%02d:%02d.%.6ld - %s - %s - %u - %s %u %s() - "
	crt_write_fmt(&pos, &left, FMT, 1900+tm.tm_year, tm.tm_mon+1,
		      tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
		      tv.tv_usec, level_s, subcomp, getpid(),
		      truncate_file_path(file), line, func);

	crt_write_fmt_args(&pos,&left,fmt,args);

	begin[count-1] = '\0';

	if (uprintf)
		printf("%s\n", begin);

	plog_path = ulog_path;
	plog_name = ulog_name;

	if (!plog_path) {
		if (!(plog_path = getcwd(curr_dir, sizeof(curr_dir))))
			return;
	}

	if (!plog_name)
		plog_name = log_name;

	snprintf(log_path, sizeof(log_path), "%s/%s", plog_path, plog_name);
	fp = fopen(log_path, "a+");
	if (fp) {
		fprintf(fp, "%s\n", begin);
		fclose(fp);
	}
}

int fd_read(int fd, void *buf, size_t len)
{
	int err;
	ssize_t ret;
	size_t pos = 0;
	while (pos < len) {
		ret = read(fd, (char *)buf + pos, len - pos);
		if (ret < 0) {
			err = errno;
			goto out;
		}
		if (ret == 0) {
			err = -EIO;
			goto out;
		}
		pos += ret;
	}
	err = 0;
out:
	return err;
}


void *crt_malloc(size_t size)
{
	return malloc(size);
}

void *crt_memcpy(void *dst, const void *src, size_t len)
{
	return memcpy(dst, src, len);
}

void *crt_memset(void *ptr, int value, size_t len)
{
	return memset(ptr, value, len);
}

int crt_memcmp(const void *ptr1, const void *ptr2, size_t len)
{
	return memcmp(ptr1, ptr2, len);
}

void crt_free(void *ptr)
{
	free(ptr);
}

int crt_random_buf(void *buf, size_t len)
{
	int fd = open("/dev/urandom", O_RDONLY);
	int err;

	if (fd < 0)
		return errno;

	err = fd_read(fd, buf, len);
	close(fd);
	return err;
}

void crt_log(int level, const char *file, int line,
	const char *func, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	crt_log_v(level,  __LOGNAME__, "crt", file, line, func, fmt, args);
	va_end(args);
}

size_t crt_strlen(const char *s)
{
	return strlen(s);
}

