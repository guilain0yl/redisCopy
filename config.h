#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef __APPLE__
#include<AvailabilityMacros.h>
#endif

#ifdef __linux__
#include<linux/version.h>
#include<features.h>
#endif

#if defined(__APPLE__)&&!defined(MAC_OS_X_VERSION_10_6)
#define redis_fstat fstat64
#define redis_stat fstat64
#else
#define redis_fstat fstat
#define redis_stat stat
#endif

#ifdef __linux__
#define HAVE_PROC_STAT 1
#define HAVE_PROC_MAPS 1
#define HAVE_PROC_SMAPS 1
#define HAVE_PROC_SOMAXCONN 1
#endif

#if defined(__APPLE__)
#define HAVE_TASKINFO 1
#endif

#if defined(__APPLE__)||(defined(__linux__)&&defined(__GLIBC__))||\
    defined(__FreeBSD__)||(defined(__OpenBSD__)&&defined(USE_BACKTRACE))\
    ||defined(__DragonFly__)
#define HAVE_BACKTRACE 1
#endif

#ifdef __linux__
#define HAVE_EPOLL 1
#endif

#if (defined(__APPLE__)&&defined(MAC_OS_X_VERSION_10_6))||defined(__FreeBSD__)||defined(__OpenBSD__)||defined(__NetBSD__)
#define HAVE_KQUEUE 1
#endif

#ifdef __sun
#include<sys/feature_tests.h>
#ifdef _DTRACE_VERSION
#define HAVE_EVPORT 1
#endif
#endif

#ifdef __linux__
#define redis_fsync fdatasync
#else
#define redis_fsync fsync
#endif

#ifdef __linux__
#if defined(__CLIBC__)&&defined(__GLIBC_PREREQ)
#if (LINUX_VERSION_CODE>=0x020611&&__GLIBC_PREREQ(2,6))
#define HAVE_SYNC_FILE_RANGE 1
#endif
#else
#if (LINUX_VERSION_CODE>=0x020611)
#define HAVE_SYNC_FILE_RANGE 1
#endif
#endif
#endif

#ifdef HAVE_SYNC_FILE_RANGE
#define rdb_fsync_range(fd,off,size) sync_file_range(fd,off,SYNC_FILE_RANGE_WAIT_BEFORE|SYNC_FILE_RANGE_WRITE)
#else
#define rdb_fsync_range(fd,off,size) fsync(fd)
#endif

#if (defined __NetBSD__ ||defined __FreeBSD__ || defined __OpenBSD__)
#define USE_SETPROCTITLE
#endif

#if ((defined __linux && defined(__GLIBC__))||defined __APPLE__)
#define USE_SETPROCTITLE
#define INIT_SETPROCTITLE_REPLACEMENT
void spt_init(int argc,char *argv[]);
void setproctitle(const char *fmt,...);
#endif

#include<sys/types.h>

#ifndef BYTE_ORDER
#if (BSD>=199103)
#include<machine/endian.h>
#else
#if defined(linux)||defined(__linux__)
#include<endian.h>
#else
#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321
#define PDP_ENDIAN 3412

#if defined(__i386__)||defined(__x86_64__)||defined(__amd64__)||\
    defined(vax)||defined(ns32000)||defined(sun386)||\
    defined(MIPSEL)||defined(_MIPSEL)||defined(BIT_ZERO_ON_RIGHT)||\
    defined(__alpha__)||defined(__alpha)
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#if defined(sel)||defined(pyr)||defined(mc68000)||defined(sparc)||\
    defined(is68k)||defined(tahoe)||defined(ibm032)||defined(ibm370)||\
     defined(MIPSEB) || defined(_MIPSEB) || defined(_IBMR2) || defined(DGUX) ||\
    defined(apollo) || defined(__convex__) || defined(_CRAY) || \
    defined(__hppa) || defined(__hp9000) || \
    defined(__hp9000s300) || defined(__hp9000s700) || \
    defined (BIT_ZERO_ON_LEFT) || defined(m68k) || defined(__sparc)
#define BYTE_ORDER BIG_ENDIAN
#endif
#endif
#endif
#endif

#ifndef BYTE_ORDER
#ifdef __BYTE_ORDER
#if defined(__LITTLE_ENDIAN)&&(__BIG_ENDIAN)
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN __BIG_ENDIAN
#endif
#if (__BYTE_ORDER==__LITTLE_ENDIAN)
#define BYTE_ORDER LITTLE_ENDIAN
#else
#define BYTE_ORDER BIG_ENDIAN
#endif
#endif
#endif
#endif

#if !defined(BYTE_ORDER)||\
    (BYTE_ORDER!=BIG_ENDIAN&&BYTE_ORDER!=LITTLE_ENDIAN)
#error "Undefined or invalid BYTE_ORDER"
#endif

#if (__i386__||__amd64__||__powerpc__)&&__CNUC__
#define GNUC_VERSION(__CNUC__*10000+__CNUC_MINOR__*100+__GNUC_PATCHLEVEL__)
#if defined(__clang__)
#define HAVE_ATOMIC
#endif
#if (defined(__CLIBC__)&&defined(__GLIBC_PREREQ))
#if (GNUC_VERSION>=40100&&__CLIBC_PREREQ(2,6))
#define HAVE_ATOMIC
#endif
#endif
#endif

#if defined(__arm)&&!defined(__arm__)
#define __arm__
#endif
#if defined(__aarch64__)&&!defined(__arm64__)
#define __arm64__
#endif

#if defined(__sparc)&&!defined(__sparc__)
#define __sparc__
#endif

#if defined(__sparc__)||defined(__arm__)
#define USE_ALIGNED_ACCESS
#endif

#endif
