// r3d_prof_min.h
// Minimal GPU Profiler for r3d
// Suitable for baselines and simple optimization.
// Notes:
// - Not thread-safe.
// - Zone name pointers must remain valid for the duration of the program (use
// string literals or static storage).
#ifndef R3D_PROF_MIN_H
#define R3D_PROF_MIN_H

#ifndef R3D_PROFILING
#define R3D_PROFILING 1 // Set to 0 to disable (macros become no-ops)
#endif

// Simple limits
#ifndef R3D_PROF_MAX_ZONES
#define R3D_PROF_MAX_ZONES 64
#endif

#ifndef R3D_PROF_HISTORY
#define R3D_PROF_HISTORY 128
#endif

// GL dependencies (loader must be included beforehand)
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Represents a profiling zone for GPU timings.
 * Stores a history of measurements and statistics.
 */
typedef struct {
  const char *name;              // Zone name (must be persistent)
  double hist[R3D_PROF_HISTORY]; // History of measurements (ms)
  int count;                     // Number of samples collected
  int index;                     // Current index in history
  double last_ms;                // Last measured value (ms)
} r3d_prof_zone_t;

// Initialization and reset
void r3d_prof_init(void);  // Initialize profiler state
void r3d_prof_reset(void); // Reset all zones and history (keeps zone list)

// Polls for pending GPU queries and updates zones
void r3d_prof_poll_pending(void);

// Pushes a measurement to the zone (creates it if needed)
void r3d_prof_push_gpu_ms(const char *name, double ms);

// Returns the average over the last N samples of the zone
// If fewer samples exist, averages available ones
// Returns 0.0 if zone not found
// 'samples' must be > 0
// Not thread-safe
//
double r3d_prof_get_avg_gpu_ms(const char *name, int samples);

// Returns the last single value of the zone
// Returns 0.0 if zone not found
// Not thread-safe
//
double r3d_prof_get_last_gpu_ms(const char *name);

// Convenience: average time retrieval via macro-friendly API
double R3D_ProfGetGPUZoneMS(const char *zoneName, int samplesAverage);

/**
 * Internal: Represents an active GPU profiling zone instance.
 */
typedef struct {
  const char *name; // Zone name
  unsigned int q;   // GL query object (GLuint compatible)
  int _active;      // 1 while zone is open
} r3d__zone_gpu_inst;

// Begin a GPU profiling zone
void r3d__zone_gpu_begin(r3d__zone_gpu_inst *z, const char *name);
// End a GPU profiling zone
void r3d__zone_gpu_end(r3d__zone_gpu_inst *z);

// Helper function: returns an active instance by value
static inline r3d__zone_gpu_inst r3d__zone_gpu_begin_ret(const char *name) {
  r3d__zone_gpu_inst z;
  r3d__zone_gpu_begin(&z, name);
  z._active = 1;
  return z;
}

// Macro: opens a block; end() is guaranteed to be called at block end
#if R3D_PROFILING
#define R3D_PROF_ZONE_GPU(NameLiteral)                                         \
  for (r3d__zone_gpu_inst r3d__z = r3d__zone_gpu_begin_ret((NameLiteral));     \
       r3d__z._active; r3d__z._active = 0, r3d__zone_gpu_end(&r3d__z))

#define R3D_PROF_GET_ZONE_GPU_MS(NameLiteral, NSamples)                        \
  r3d_prof_get_avg_gpu_ms((NameLiteral), (NSamples))
#else
#define R3D_PROF_ZONE_GPU(NameLiteral)                                         \
  if (1)                                                                       \
    for (int r3d__dummy = 1; r3d__dummy; r3d__dummy = 0)
#define R3D_PROF_GET_ZONE_GPU_MS(n, s) (0.0)
#endif

#ifdef __cplusplus
}
#endif

#endif // R3D_PROF_MIN_H