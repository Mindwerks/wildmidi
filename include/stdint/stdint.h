#ifndef WM_STDINT_H
#define WM_STDINT_H

#include <limits.h>

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int int16_t;
typedef unsigned short int uint16_t;

#define INT8_MIN (-128)
#define INT16_MIN (-32767-1)
#define INT8_MAX (127)
#define INT16_MAX (32767)
#define UINT8_MAX (255)
#define UINT16_MAX (65535)

#if (INT_MAX == 2147483647)
typedef signed int int32_t;
typedef unsigned int uint32_t;
#define INT32_MIN (-2147483647-1)
#define INT32_MAX ( 2147483647)
#define UINT32_MAX (4294967295U)
#elif (LONG_MAX == 2147483647)
typedef signed long int int32_t;
typedef unsigned long int uint32_t;
#define INT32_MIN (-2147483647L-1L)
#define INT32_MAX ( 2147483647L)
#define UINT32_MAX (4294967295UL)
#else
#error define a 32bit integral type
#endif /* int32_t */

#ifdef _MSC_VER
typedef unsigned __int64 uint64_t;
typedef signed __int64 int64_t;
#define INT64_MAX   9223372036854775807i64
#define INT64_MIN (-9223372036854775807i64-1i64)
#define UINT64_MAX 18446744073709551615ui64
#define INT64_C(c)  c ## i64
#define UINT64_C(c) c ## ui64
#elif defined(_LP64) || defined(__LP64__)
typedef unsigned long uint64_t;
typedef signed long int64_t;
#define INT64_MAX   9223372036854775807L
#define INT64_MIN (-9223372036854775807L-1L)
#define UINT64_MAX 18446744073709551615UL
#define INT64_C(c)  c ## L
#define UINT64_C(c) c ## UL
#else
typedef unsigned long long uint64_t;
typedef signed long long int64_t;
#define INT64_MAX   9223372036854775807LL
#define INT64_MIN (-9223372036854775807LL-1LL)
#define UINT64_MAX 18446744073709551615ULL
#define INT64_C(c)  c ## LL
#define UINT64_C(c) c ## ULL
typedef int _wm_llong_test[(sizeof(long long) == 8) * 2 - 1];
#endif /* int64_t */

/* make sure of type sizes */
typedef int _wm_int8_test [(sizeof(int8_t ) == 1) * 2 - 1];
typedef int _wm_int16_test[(sizeof(int16_t) == 2) * 2 - 1];
typedef int _wm_int32_test[(sizeof(int32_t) == 4) * 2 - 1];
typedef int _wm_int64_test[(sizeof(int64_t) == 8) * 2 - 1];

#endif /* WM_STDINT_H */
