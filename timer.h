#ifndef TENES_TIMER_H
#define TENES_TIMER_H

#include <stdbool.h>

/// Start (or unpause) the stopwatch running. Time is not reset.
void stopwatch_start();
/// Pause the stopwatch (but preserve accumulated time)
void stopwatch_stop();
/// Reset stopwatch time to zero
void stopwatch_reset();
/// Get total time counted by the stopwatch, in microseconds
long long stopwatch_get_elapsed();
/// Return true if stopwatch is running or paused, false if stopped with zero accumulated time
bool stopwatch_is_enabled();
/// Return true if stopwatch is running
bool stopwatch_is_running();
/// Format time in hours:minutes:seconds:milliseconds format
const char *format_time (long long usecs);

#endif
