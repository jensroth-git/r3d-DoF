#include "r3d_prof_min.h"

#include <glad.h>
#include <stdio.h>  // printf
#include <string.h> // strcmp, memset

// GL query result enums (if not provided by headers)
#ifndef GL_QUERY_RESULT
#define GL_QUERY_RESULT 0x8866
#endif
#ifndef GL_QUERY_RESULT_AVAILABLE
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#endif

// Pending queries buffer size
#define R3D_PROF_PENDING_MAX 32

// ----------------------------------------------------------------------------
// Pending GPU queries tracking
// ----------------------------------------------------------------------------

typedef struct {
  GLuint q;         // GL query object
  const char *name; // Zone name
  int in_use;       // 1 if slot is active
} r3d__pending_t;

static r3d__pending_t g_pending[R3D_PROF_PENDING_MAX];

static void r3d__pending_clear(void) {
  memset(g_pending, 0, sizeof(g_pending));
}

static void r3d__pending_add(GLuint q, const char *name) {
  for (int i = 0; i < R3D_PROF_PENDING_MAX; ++i) {
    if (!g_pending[i].in_use) {
      g_pending[i].q = q;
      g_pending[i].name = name;
      g_pending[i].in_use = 1;
      return;
    }
  }

  // Should not happen; handle with blocking read to avoid leaking the query
  printf("R3D: GPU Profiler: Pending list full; performing blocking read for "
         "query %u in zone '%s'\n",
         q, name ? name : "(null)");

  GLuint64 ns = 0;
  glGetQueryObjectui64v(q, GL_QUERY_RESULT, &ns);
  if (name)
    r3d_prof_push_gpu_ms(name, (double)ns / 1e6);
  glDeleteQueries(1, &q);
}

void r3d_prof_poll_pending(void) {
  for (int i = 0; i < R3D_PROF_PENDING_MAX; ++i) {
    if (!g_pending[i].in_use)
      continue;
    GLint ready = 0;
    glGetQueryObjectiv(g_pending[i].q, GL_QUERY_RESULT_AVAILABLE, &ready);
    if (ready) {
      GLuint64 ns = 0;
      glGetQueryObjectui64v(g_pending[i].q, GL_QUERY_RESULT, &ns);
      r3d_prof_push_gpu_ms(g_pending[i].name, (double)ns / 1e6);
      glDeleteQueries(1, &g_pending[i].q);
      g_pending[i].in_use = 0;
    }
  }
}

// ----------------------------------------------------------------------------
// Zones data and helpers
// ----------------------------------------------------------------------------

static r3d_prof_zone_t g_zones[R3D_PROF_MAX_ZONES];
static int g_zone_count = 0;

static r3d_prof_zone_t *r3d__find_zone(const char *name) {
  if (!name)
    return NULL;
  for (int i = 0; i < g_zone_count; i++) {
    if (g_zones[i].name && strcmp(g_zones[i].name, name) == 0)
      return &g_zones[i];
  }
  if (g_zone_count < R3D_PROF_MAX_ZONES) {
    r3d_prof_zone_t *z = &g_zones[g_zone_count++];
    z->name = name;
    z->count = 0;
    z->index = 0;
    z->last_ms = 0.0;
    for (int k = 0; k < R3D_PROF_HISTORY; k++)
      z->hist[k] = 0.0;
    return z;
  }
  return NULL; // no free zone slots
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void r3d_prof_init(void) {
  g_zone_count = 0;
  r3d__pending_clear(); // Do not call while queries are active
}

void r3d_prof_reset(void) {
  for (int i = 0; i < g_zone_count; i++) {
    g_zones[i].count = 0;
    g_zones[i].index = 0;
    g_zones[i].last_ms = 0.0;
    for (int k = 0; k < R3D_PROF_HISTORY; k++)
      g_zones[i].hist[k] = 0.0;
  }
}

void r3d_prof_push_gpu_ms(const char *name, double ms) {
  if (!name)
    return;
  r3d_prof_zone_t *z = r3d__find_zone(name);
  if (!z)
    return;
  z->last_ms = ms;
  z->hist[z->index] = ms;
  z->index = (z->index + 1) % R3D_PROF_HISTORY;
  if (z->count < R3D_PROF_HISTORY)
    z->count++;
}

double r3d_prof_get_avg_gpu_ms(const char *name, int samples) {
  if (!name)
    return 0.0;
  r3d_prof_zone_t *z = r3d__find_zone(name);
  if (!z || z->count == 0)
    return 0.0;
  if (samples <= 0)
    samples = 1;
  if (samples > z->count)
    samples = z->count;
  double sum = 0.0;
  int idx = z->index;
  for (int i = 0; i < samples; i++) {
    idx = (idx - 1 + R3D_PROF_HISTORY) % R3D_PROF_HISTORY;
    sum += z->hist[idx];
  }
  return sum / (double)samples;
}

double r3d_prof_get_last_gpu_ms(const char *name) {
  if (!name)
    return 0.0;
  r3d_prof_zone_t *z = r3d__find_zone(name);
  return z ? z->last_ms : 0.0;
}

// ----------------------------------------------------------------------------
// GPU zone RAII-like helpers
// ----------------------------------------------------------------------------

void r3d__zone_gpu_begin(r3d__zone_gpu_inst *z, const char *name) {
  z->name = name ? name : "unnamed";
  glGenQueries(1, (GLuint *)&z->q);
  glBeginQuery(GL_TIME_ELAPSED, z->q);
}

void r3d__zone_gpu_end(r3d__zone_gpu_inst *z) {
  glEndQuery(GL_TIME_ELAPSED);
  r3d__pending_add((GLuint)z->q, z->name);
  r3d_prof_poll_pending();
}

double R3D_ProfGetGPUZoneMS(const char *zoneName, int samplesAverage) {
  return R3D_PROF_GET_ZONE_GPU_MS(zoneName, samplesAverage);
}