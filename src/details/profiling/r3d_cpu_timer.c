#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include "r3d_cpu_timer.h"
#include <stdint.h>
#include <windows.h>

void r3d_cpu_timer_init(r3d_cpu_timer_t *t) {
  LARGE_INTEGER freq = {0}, t0 = {0};
  QueryPerformanceFrequency(&freq);
  t->freq = *(void **)&freq;
  t->t0 = *(void **)&t0;
}
void r3d_cpu_timer_start(r3d_cpu_timer_t *t) {
  LARGE_INTEGER t0;
  QueryPerformanceCounter(&t0);
  t->t0 = *(void **)&t0;
}
double r3d_cpu_timer_ms(r3d_cpu_timer_t *t) {
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  LARGE_INTEGER t0 = *(LARGE_INTEGER *)&t->t0;
  LARGE_INTEGER freq = *(LARGE_INTEGER *)&t->freq;
  return 1000.0 * (double)(now.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
}

#elif defined(__APPLE__)
#include "r3d_timers.h"
#include <mach/mach_time.h>
#include <stdint.h>

void r3d_cpu_timer_init(r3d_cpu_timer_t *t) {
  mach_timebase_info_data_t tb;
  mach_timebase_info(&tb);
  t->tb = *(void **)&tb;
  t->t0 = 0;
}
void r3d_cpu_timer_start(r3d_cpu_timer_t *t) { t->t0 = mach_absolute_time(); }
double r3d_cpu_timer_ms(r3d_cpu_timer_t *t) {
  uint64_t now = mach_absolute_time();
  mach_timebase_info_data_t tb = *(mach_timebase_info_data_t *)&t->tb;
  uint64_t ns = (now - t->t0) * tb.numer / tb.denom;
  return ns / 1e6;
}

#else
#include "r3d_timers.h"
#include <stdint.h>
#include <time.h>

void r3d_cpu_timer_init(r3d_cpu_timer_t *t) { (void)t; }
void r3d_cpu_timer_start(r3d_cpu_timer_t *t) {
  struct timespec t0;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  t->t0 = *(void **)&t0;
}
double r3d_cpu_timer_ms(r3d_cpu_timer_t *t) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  struct timespec t0 = *(struct timespec *)&t->t0;
  long sec = now.tv_sec - t0.tv_sec;
  long nsec = now.tv_nsec - t0.tv_nsec;
  return (double)sec * 1000.0 + (double)nsec / 1e6;
}
#endif