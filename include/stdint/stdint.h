#ifndef WM_STDINT_H
#define WM_STDINT_H

#include <limits.h>

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int int16_t;
typedef unsigned short int uint16_t;

#define INT8_MIN (-128)
#define INT16_MIN (-32767-1)
#define INT32_MIN (-2147483647-1)
#define INT8_MAX (127)
#define INT16_MAX (32767)
#define INT32_MAX (2147483647)
#define UINT8_MAX (255)
#define UINT16_MAX (65535)

#if (INT_MAX == 2147483647)
typedef signed int int32_t;
typedef unsigned int uint32_t;
#define UINT32_MAX (4294967295U)
#elif (LONG_MAX == 2147483647)
typedef signed long int int32_t;
typedef unsigned long int uint32_t;
#define UINT32_MAX (4294967295UL)
#else
#error define a 32bit integral type
#endif /* int32_t */

/* make sure of type sizes */
typedef int _wm_int8_test [(sizeof(int8_t ) == 1) * 2 - 1];
typedef int _wm_int16_test[(sizeof(int16_t) == 2) * 2 - 1];
typedef int _wm_int32_test[(sizeof(int32_t) == 4) * 2 - 1];

#endif /* WM_STDINT_H */

