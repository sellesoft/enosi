
// its reeeally lame but i gotta do this to avoid including C's time.h instead of mine
// probably should just switch to not using relative paths or something or just rename
// time.h (which cant just be Time.h because Windows SUCKS) to something else
#include "time/time.h"

#include "time.h"
#include "math.h"

#include "platform.h"

namespace iro
{

TimePoint TimePoint::now()
{
	platform::Timespec ts = platform::clock_realtime();
	return {ts.seconds, ts.nanoseconds};
}

TimePoint TimePoint::monotonic()
{
	platform::Timespec ts = platform::clock_monotonic();
	return {ts.seconds, ts.nanoseconds};
}

TimeSpan operator-(const TimePoint& lhs, const TimePoint& rhs)
{
	return { s64(lhs.s * 1000000000 + lhs.ns) - s64(rhs.s * 1000000000 + rhs.ns) };
}

TimeSpan TimeSpan::fromNanoseconds(s64 n)  { return { n }; }
TimeSpan TimeSpan::fromMicroseconds(s64 n) { return { n * 1000 }; }
TimeSpan TimeSpan::fromMilliseconds(s64 n) { return { n * 1000000 }; }
TimeSpan TimeSpan::fromSeconds(s64 n)      { return { n * 1000000000 }; }
TimeSpan TimeSpan::fromMinutes(s64 n)      { return { n * 1000000000 * 60 }; }
TimeSpan TimeSpan::fromHours(s64 n)        { return { n * 1000000000 * 60 * 60 }; }
TimeSpan TimeSpan::fromDays(s64 n)         { return { n * 1000000000 * 60 * 60 * 24 }; }

f64 TimeSpan::toNanoseconds()  const { return (f64)ns; }   
f64 TimeSpan::toMicroseconds() const { return (f64)ns / 1e3; }
f64 TimeSpan::toMilliseconds() const { return (f64)ns / 1e6; }
f64 TimeSpan::toSeconds()      const { return (f64)ns / 1e9; }
f64 TimeSpan::toMinutes()      const { return (f64)ns / 6e10; }
f64 TimeSpan::toHours()        const { return (f64)ns / 3.6e12; }
f64 TimeSpan::toDays()         const { return (f64)ns / 8.64e13; }

namespace io
{

s64 format(IO* io, const TimeSpan& x)
{
	return io::format(io, x.toSeconds());
}

s64 format(IO* io, const UTCDate& x)
{
	char buffer[255];
	struct tm* tm = gmtime(&(time_t&)x.point.s);
	size_t written = strftime(buffer, 255, x.fmtstr, tm);
	return io->write({(u8*)buffer, written});
}

s64 format(IO* io, const LocalDate& x)
{
	char buffer[255];
	struct tm* tm = localtime(&(time_t&)x.point.s);
	size_t written = strftime(buffer, 255, x.fmtstr, tm);
	return io->write({(u8*)buffer, written});
}

s64 format(IO* io, const WithUnits& x)
{
	TimeSpan span = x.x;
	s64 bytes_written = 0;
	s64 units = x.maxunits;
	s64 n = 0;
#define unit(f, u) \
	n = floor(span.GLUE(to,f)()); \
	if (n)                            \
	{                                 \
		if (units != 1) \
			bytes_written += formatv(io, n, STRINGIZE(u) " "); \
		else \
			bytes_written += formatv(io, n, STRINGIZE(u)); \
		units -= 1;                   \
		if (!units)                   \
			return bytes_written;     \
		span.ns -= GLUE(TimeSpan::from, f)(n).ns;                      \
	} \

	unit(Days,         days);
	unit(Hours,        h);
	unit(Minutes,      m);
	unit(Seconds,      s);
	unit(Milliseconds, ms);
	unit(Microseconds, μs);
	unit(Nanoseconds,  ns);

	return bytes_written;
}

}

}
