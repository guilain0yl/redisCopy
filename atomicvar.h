#include<pthread.h>


#ifndef __ATOMIC_VAR_H
#define __ATOMIC_VAR_H

#if !defined(__AOTOMIC_VAR_FORCE_SYNC_MACROS)&&defined(__ATOMIC_RELAXED)&&!defined(__sun)&&(!defined(__clang__)||!defined(__APPLE__)||__apple_build_version__>4210057)

#define atomicIncr(var,count) __atomic_add_fetch(&var,(count),__ATOMIC_RELAXED)
#define atomicGetIncr(var,oldvalue_var,count) do{\
    oldvalue_var=__atomic_fetch_add(&var,(count),__ATOMIC_RELAXED);\
}while(0)
#define atomicDecr(var,count) __atomic_sub_fetch(&var,(count),__ATOMIC_RELAXED)
#define atomicGet(var,dstvar) do{\
    dstvar=__atomic_load_n(&var,__ATOMIC_RELAXED);\
}while(0)
#define atomicSet(var,value) __atomic_store_n(&var,value,__ATOMIC_RELAXED)

#define REDIS_ATOMIC_API "atomic-builtin"

#elif defined(HAVE_ATOMIC)

#define atomicIncr(var,count) __sync_add_and_fetch(&var,(count))
#define atomicGetIncr(var,oldvalue_var,count) do{\
    oldvalue_var=__sync_fetch_and_add(&var,(count));\
}while(0)
#define atomicDecr(var,count) __sync_sub_and_fetch(&var,(count))
#define atomicGet(var,dstvar) do{\
    dstvar=__sync_sub_and_fetch(&var,0);\
}while(0)
#define atomicSet(var,value) do{\
    while(!__sync_bool_compare_and_swap(&var,var,value));\
}

#define REDIS_ATOMIC_API "sync-builtin"

#else

#define atomicIncr(var,count) do{\
    pthread_mutex_lock(&var##_mutex);\
    var+=(count);\
    pthread_mutex_unlock(&var##_mutex);\
}while(0)
#define atomicGetIncr(var,oldvalue_var,count)  do{\
    pthread_mutex_lock(&var##_mutex);\
    oldvalue_var=var;\
    var+=(count);\
    pthread_mutex_unlock(&var##_mutex);\
}while(0)
#define atomicDecr(var,count)  do{\
    pthread_mutex_lock(&var##_mutex);\
    var-=(count);\
    pthread_mutex_unlock(&var##_mutex);\
}while(0) 
#define atomicGet(var,dstvar)  do{\
    pthread_mutex_lock(&var##_mutex);\
    dstvar=var;\
    pthread_mutex_unlock(&var##_mutex);\
}while(0)
#define atomicSet(var,value)  do{\
    pthread_mutex_lock(&var##_mutex);\
    var=value;\
    pthread_mutex_unlock(&var##_mutex);\
}while(0)

#define REDIS_ATOMIC_API "pthread-mutex"

#endif
#endif
