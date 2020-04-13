#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>

void zlibc_free(void* ptr)
{
	free(ptr);
}

#include<string.h>
#include<pthread.h>
#include"config.h"
#include"zmalloc.h"
#include"atomicvar.h"

#ifdef HAVE_MALLOC_SIZE
#define PREFIX_SIZE (0)
#else
#if defined(__sun)||defined(__sparc)||defined(__sparc__)
#define PREFIX_SIZE (sizeof(long long))
#else
#define PREFIX_SIZE (sizeof(size_t))
#endif
#endif

#if defined(USE_TCMALLOC)
#define malloc(size) tc_malloc(size)
#define calloc(count,size) tc_calloc(count,size)
#define realloc(ptr,size) tc_realloc(ptr,size)
#define free(ptr) tc_free(ptr)
#elif defined(USE_JEMALLOC)
#define malloc(size) je_malloc(size)
#define calloc(count,size) je_calloc(count,size)
#define realloc(ptr,size) je_realloc(ptr,size)
#define free(ptr) he_free(ptr)
#define mallocx(size,flags) je_mallocx(size,flags)
#define dallocx(ptr,flags) je_dallocx(ptr,flags)
#endif

#define update_zmalloc_stat_alloc(__n) do{\
    size_t _n=(__n);\
    if(_n&(sizeof(long)-1)) _n+=sizeof(long)-(_n&(sizeof(long)-1));\
    atomicIncr(used_memory,__n);\
}while(0)

#define update_zmalloc_stat_free(__n) do { \
    size_t _n = (__n); \
    if (_n&(sizeof(long)-1)) _n += sizeof(long)-(_n&(sizeof(long)-1)); \
    atomicDecr(used_memory,__n); \
} while(0)

static size_t used_memory = 0;
pthread_mutex_t used_memory_mutex = PTHREAD_MUTEX_INITIALIZER;

static void zmalloc_default_oom(size_t size)
{
	fprintf(stderr, "zmalloc: Out of memory trying to allocate %zu bytes\n", size);
	fflush(stderr);
	abort();
}

static void (*zmalloc_oom_handler)(size_t) = zmalloc_default_oom;

void* zmalloc(size_t size)
{
	void* ptr = malloc(size + PREFIX_SIZE);

	if (!ptr) zmalloc_oom_handler(size);
#ifdef HAVE_MALLOC_SIZE
	update_zmalloc_stat_alloc(zmalloc_size(ptr));
	return ptr;
#else
	* ((size_t*)ptr) = size;
	update_zmalloc_stat_alloc(size + PREFIX_SIZE);
	return (char*)ptr + PREFIX_SIZE;
#endif
}

#ifdef HAVE_DEFRAG
void* zmalloc_no_tcache(size_t size)
{
	void* ptr = mallocx(size + PREFIX_SIZE, MALLOCX_TCACHE_NONE);
	if (!ptr) zmalloc_oom_handler(size);
	update_zmalloc_stat_alloc(zmalloc_size(ptr));
	return ptr;
}

void zfree_no_tcache(void* ptr)
{
	if (ptr == NULL) return;
	update_zmalloc_stat_alloc(zmalloc_size(ptr));
	dallocx(ptr, MALLOCX_TCACHE_NONE);
}

#endif

void* zcalloc(size_t size)
{
	void* ptr = calloc(1, size + PREFIX_SIZE);

	if (!ptr) zmalloc_oom_handler(size);
#ifdef HAVE_MALLOC_SIZE
	update_zmalloc_stat_alloc(zmalloc_size(ptr));
	return ptr;
#else
	* ((size_t*)ptr) = size;
	update_zmalloc_stat_alloc(size + PREFIX_SIZE);
	return (char*)ptr + PREFIX_SIZE;
#endif
}

void* zrealloc(void* ptr, size_t size)
{
#ifndef HAVE_MALLOC_SIZE
	void* realptr;
#endif
	size_t oldsize;
	void* newptr;

	if (size == 0 && ptr != NULL)
	{
		zfree(ptr);
		return NULL;
	}
	if (ptr == NULL)return zmalloc(size);
#ifdef HAVE_MALLOC_SIZE
	oldsize = zmalloc_size(ptr);
	newptr = realloc(ptr, size);
	if (!newptr) zmalloc_oom_handler(size);

	update_zmalloc_stat_free(oldsize);
	update_zmalloc_stat_alloc(zmalloc_size(newptr));
	return newptr;
#else
	realptr = (char*)ptr - PREFIX_SIZE;
	oldsize = *((size_t*)realptr);
	newptr = realloc(realptr, size + PREFIX_SIZE);
	if (!newptr) zmalloc_oom_handler(size);

	*((size_t*)newptr) = size;
	update_zmalloc_stat_free(oldsize + PREFIX_SIZE);
	update_zmalloc_stat_alloc(size + PREFIX_SIZE);
	return (char*)newptr + PREFIX_SIZE;
#endif
}

#ifndef HAVE_MALLOC_SIZE
size_t zmalloc_size(void* ptr)
{
	void* realptr = (char*)ptr - PREFIX_SIZE;
	size_t size = *((size_t*)realptr);

	if (size & (sizeof(long) - 1)) size += sizeof(long) - (size & (sizeof(long) - 1));
	return size + PREFIX_SIZE;
}

size_t zmalloc_usable(void* ptr)
{
	return zmalloc_size(ptr) - PREFIX_SIZE;
}
#endif

void zfree(void* ptr)
{
#ifndef HAVE_MALLOC_SIZE
	void* realptr;
	size_t oldsize;
#endif

	if (ptr == NULL) return;
#ifdef HAVE_MALLOC_SIZE
	update_zmalloc_stat_free(zmalloc_size(ptr));
	free(ptr);
#else
	realptr = (char*)ptr - PREFIX_SIZE;
	oldsize = *((size_t*)realptr);
	update_zmalloc_stat_free(oldsize + PREFIX_SIZE);
	free(realptr);
#endif
}

char* zstrdup(const char* s)
{
	size_t l = strlen(s) + 1;
	char* p = zmalloc(l);

	memcpy(p, s, l);
	return p;
}

size_t zmalloc_used_memory(void)
{
	size_t um;
	atomicGet(used_memory, um);
	return um;
}

void zmalloc_set_oom_handler(void (*oom_handler)(size_t))
{
	zmalloc_oom_handler = oom_handler;
}

#if defined(HAVE_PROC_STAT)
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>

size_t zmalloc_get_rss(void)
{
	int page = sysconf(_SC_PAGESIZE);
	size_t rss;
	char buf[4096];
	char filename[256];
	int fd, count;
	char* p, * x;

	snprintf(filename, 256, "/proc/%d/stat", getpid());
	if ((fd = open(filename, O_RDONLY)) == -1) return 0;
	if (read(fd, buf, 4096) <= 0)
	{
		close(fd);
		return 0;
	}
	close(fd);

	p = buf;
	count = 23;
	while (p && count--)
	{
		p = strchr(p, ' ');
		if (p) p++;
	}
	if (!p) return 0;
	x = strchr(p, ' ');
	if (!x) return 0;
	*x = '\0';

	rss = strtoll(p, NULL, 10);
	rss *= page;
	return rss;
}
#elif defined(HAVE_TASKINFO)
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/sysctl.h>
#include<mach/task.h>
#include<mach/mach_init.h>

size_t zmalloc_get_rss(void)
{
	task_t task = MACH_PORT_NULL;
	struct task_basic_info t_info;
	mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

	if (task_for_pid(current_task(), getpid(), &task) != KERN_SUCCESS)
		return 0;
	task_info(task, TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);

	return t_info.resident_size;
}

#elif defined(__FreeBSD__)
#include<sys/types.h>
#include<sys/sysctl.h>
#include<sys/user.h>
#include<unistd.h>

size_t zmalloc_get_rss(void)
{
	struct kinfo_proc info;
	size_t infolen = sizeof(info);
	int mib[4];
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = getpid();

	if (sysctl(mib, 4, &info, &infolen, NULL, 0) == 0)
		return (size_t)info.ki_rssize;

	return 0L;
}
#else
size_t zmalloc_get_rss(void)
{
	return zmalloc_used_memory();
}
#endif

#if defined(USE_JEMALLOC)

int zmalloc_get_allocator_info(size_t* allocated, size_t* active, size_t* resident)
{
	uint64_t epoch = 1;
	size_t sz;
	*allocated = *resident = *active = 0;
	sz = sizeof(epoch);
	je_mallctl("epoch", &epoch, &sz, &epoch, sz);
	sz = sizeof(size_t);
	je_mallctl("stats.resident", resident, &sz, NULL, 0);
	je_mallctl("stats.active", active, &sz, NULL, 0);
	je_mallctl("stats.allocated", allocated, &sz, NULL, 0);
	return 1;
}

void set_jemalloc_bg_thread(int enable)
{
	char val = !!enable;
	je_mallctl("background_thread", NULL, 0, &val, 1);
}

int jemalloc_purge()
{
	char tmp[32];
	unsigned narenas = 0;
	size_t sz = sizeof(unsigned);
	if (!je_mallctl("arenas.narenas", &narenas, &sz, NULL, 0))
	{
		sprintf(tmp, "arena.%d.purge", narenas);
		if (!je_mallctl(tmp, NULL, 0, NULL, 0))
			return 0;
	}
	return -1;
}
#else

int zmalloc_get_allocator_info(size_t* allocated, size_t* active, size_t* resident)
{
	*allocated = *resident = *active = 0;
	return 1;
}

void set_jemalloc_bg_thread(int enable)
{
	((void)(enable));
}

int jemalloc_purge()
{
	return 0;
}

#endif

#if defined(__APPLE__)
#include<libproc.h>
#endif

#if defined(HAVE_PROC_SMAPS)
size_t zmalloc_get_smap_bytes_by_field(char* field, long pid)
{
	char line[1024];
	size_t bytes = 0;
	int flen = strlen(field);
	FILE* fp;

	if (pid == -1)
	{
		fp = fopen("/proc/self/smaps", "r");
	}
	else
	{
		char filename[128];
		snprintf(filename, sizeof(filename), "/proc/%ld/smaps", pid);
		fp = fopen(filename, "r");
	}

	if (!fp) return 0;
	while (fgets(line, sizeof(line), fp) != NULL)
	{
		if (strncmp(line, field, flen) == 0)
		{
			char* p = strchr(line, 'k');
			if (p)
			{
				*p = '\0';
				bytes += strtol(line + flen, NULL, 10) * 1024;
			}
		}
	}
	fclose(fp);
	return bytes;
}
#else
size_t zmalloc_get_smap_bytes_by_field(char* field, long pid)
{
#if defined(__APPLE__)
	struct proc_regioninfo pri;
	if (proc_pidinfo(pid, PROC_PIDREGIONINFO, 0, &pri, PROC_PIDREGIONINFO_SIZE) == PROC_PIDREGIONINFO_SIZE)
	{
		if (!strcmp(field, "Private_Dirty:"))
		{
			return (size_t)pri.pri_pages_dirtied * 4096;
		}
		else if (!strcmp(field, "Rss:"))
		{
			return (size_t)pri.pri_pages_resident * 4096;
		}
		else if (!strcmp(field, "AnonHugePages:"))
		{
			return 0;
		}
	}
	return 0;
#endif
	((void)field);
	((void)pid);
	return 0;
}
#endif

size_t zmalloc_get_private_dirty(long pid)
{
	return zmalloc_get_smap_bytes_by_field("Private_Dirty:", pid);
}

size_t zmalloc_get_memory_size(void)
{
#if defined(__unix__)||defined(__unix)||defined(unix)||\
    (defined(__APPLE__)&&defined(__MACH__))
#if defined(CTL_HW)&&(defined(HW_MEMSIZE))||defined(HW_PHYSMEM64)
	int mib[2];
	mib[0] = CTL_HW;
#if defined(HW_MEMSIZE)
	mib[1] = HW_MEMSIZE;
#elif defined(HW_PHYSMEM64)
	mib[1] = HW_PHYSMEM64;
#endif
	int64_t size = 0;
	size_t len = sizeof(size);
	if (sysctl(mib, 2, &size, &len, NULL, 0) == 0)
		return (size_t)size;
	return 0L;
#elif defined(_SC_PHYS_PAGES)&&defined(_SC_PAGESIZE)
	return (size_t)sysconf(_SC_PHYS_PAGES) * (size_t)sysconf(_SC_PAGESIZE);
#elif defined(CTL_HW)&&(defined(HW_PHYSMEM)||defined(HW_REALMEM))
	int mib[2];
	mib[0] = CTL_HW;
#if defined(HW_REALMEM)
	mib[1] = HW_REALMEM;
#elif defined(HW_PHYSMEM)
	mib[1] = HW_PHYSMEM;
#endif
	unsigned int size = 0;
	size_t len = sizeof(size);
	if (sysctl(mib, 2, &size, &len, NULL, 0) == 0)
		return (size_t)size;
	return 0L;
#else
	return 0L;
#endif
#else
	return 0L;
#endif
}

int main()
{
	size_t memory_size = zmalloc_get_memory_size();
	printf("memory size:%d", memory_size);

	return 1;
}
