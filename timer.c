#include "timer.h"
#include "sys.h"

static long long start_time_usec = 0;
static long long accumulated_time_usec = 0;
static bool is_running = false;

void stopwatch_start()
{
    if (!is_running)
    {
        start_time_usec = usectime();
        is_running = true;
    }
}

void stopwatch_stop()
{
    accumulated_time_usec = stopwatch_get_elapsed();
    is_running = false;
}

void stopwatch_reset()
{
    accumulated_time_usec = 0;
    start_time_usec = usectime();
}

long long stopwatch_get_elapsed()
{
    return accumulated_time_usec + (is_running? (usectime() - start_time_usec) : 0);
}

bool stopwatch_is_enabled()
{
    return accumulated_time_usec || is_running;
}

bool stopwatch_is_running()
{
    return is_running;
}

const char *format_time (long long usecs)
{
    static char buf[80];
    const long long seconds = usecs / 1000000;
    const long long minutes = seconds / 60;
    const long long hours = minutes / 60;

    snprintf(buf, sizeof(buf), "%02lli:%02lli:%02lli.%03lli",
             hours,
             minutes % 60,
             seconds % 60,
             (usecs/1000) % 1000);

    return buf;
}
