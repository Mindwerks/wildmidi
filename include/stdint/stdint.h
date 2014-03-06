#ifndef WM_STDINT_H
#define WM_STDINT_H

#include <limits.h>

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int int16_t;
typedef unsigned short int uint16_t;

#if (INT_MAX == 2147483647)
typedef signed int int32_t;
typedef unsigned int uint32_t;
#elif (LONG_MAX == 2147483647)
typedef signed long int int32_t;
typedef unsigned long int uint32_t;
#else
#error define a 32bit integral type
#endif /* int32_t */

/* make sure of type sizes */
typedef int _wm_int8_test [(sizeof(int8_t ) == 1) * 2 - 1];
typedef int _wm_int16_test[(sizeof(int16_t) == 2) * 2 - 1];
typedef int _wm_int32_test[(sizeof(int32_t) == 4) * 2 - 1];

#endif /* WM_STDINT_H */

