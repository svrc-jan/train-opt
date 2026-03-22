#pragma once

#ifdef _OPENMP

#include "omp.h"
#define OMP_STATIC _Pragma("omp parallel for schedule(static,1024)")
#define OMP_STATIC_SMALL _Pragma("omp parallel for schedule(static,16)")
#define OMP_DYNAMIC _Pragma("omp parallel for schedule(dynamic,16)")
#define OMP_NUM_THR omp_get_max_threads()
#define OMP_THR_ID omp_get_thread_num()

#else

#define OMP_STATIC
#define OMP_STATIC_SMALL
#define OMP_DYNAMIC
#define OMP_NUM_THR 1
#define OMP_THR_ID 0

#endif

