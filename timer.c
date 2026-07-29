#include "timer.h"
#include "sys.h"

static long long start_time_usec = 0;
static long long accumulated_time_usec = 0;
static bool is_running = false;
static double timer_offset_sec = 0;

void stopwatch_set_offset(double seconds)
{
	timer_offset_sec = seconds;
}

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
	long long adj_usecs = usecs - timer_offset_sec * 1e6;
	const char *sign = "";
	if (adj_usecs < 0)
	{
		adj_usecs = -adj_usecs;
		sign = "-";
	}

    const long long seconds = adj_usecs / 1000000;
    const long long minutes = seconds / 60;
    const long long hours = minutes / 60;

	if (hours)
	{
		snprintf(buf, sizeof(buf), "%s%lli:%02lli:%02lli.%02lli ",
				 sign,
				 hours,
				 minutes % 60,
				 seconds % 60,
				 (adj_usecs/10000) % 100);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s%lli:%02lli.%02lli ",
				 sign,
				 minutes % 60,
				 seconds % 60,
				 (adj_usecs/10000) % 100);
	}

    return buf;
}
