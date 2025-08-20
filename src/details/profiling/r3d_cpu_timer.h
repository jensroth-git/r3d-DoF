// C99, high-res monotonic timer. Works on Windows, macOS, Linux, iOS, Android.
#ifndef R3D_TIMER_H
#define R3D_TIMER_H

#include <stdint.h>

#if defined(_WIN32)
typedef struct {
  void *freq; // LARGE_INTEGER
  void *t0;   // LARGE_INTEGER
} r3d_cpu_timer_t;
#elif defined(__APPLE__)
typedef struct {
  void *tb; // mach_timebase_info_data_t
  uint64_t t0;
} r3d_cpu_timer_t;
#else
typedef struct {
  void *t0; // struct timespec
} r3d_cpu_timer_t;
#endif

void r3d_cpu_timer_init(r3d_cpu_timer_t *t);
void r3d_cpu_timer_start(r3d_cpu_timer_t *t);
double r3d_cpu_timer_ms(r3d_cpu_timer_t *t);

#endif // R3D_TIMER_H