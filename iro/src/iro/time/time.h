/*
 *  Time utilities.
 */

#ifndef _iro_time_h
#define _iro_time_h

#include "../common.h"
#include "../io/format.h"
#include "../traits/nil.h"

namespace iro
{

struct TimeSpan;

enum class WeekDay
{
  Monday,
  Tuesday,
  Wednesday,
  Thursday,
  Friday,
  Saturday,
  Sunday,
};

/* ================================================================================================ TimePoint
 */
struct TimePoint
{
  u64 s, ns;
  
  static TimePoint now();
  static TimePoint monotonic();

  bool operator==(const TimePoint& rhs) const
  {
    return s == rhs.s && ns == rhs.ns;
  }
};

TimeSpan operator-(const TimePoint& lhs, const TimePoint& rhs);

static const char* __default_fmtstr = "%c";

// formatters
struct UTCDate
{
  const TimePoint& point;
  const char* fmtstr;
  UTCDate(const TimePoint& point, const char* fmtstr = __default_fmtstr) : 
    point(point), fmtstr(fmtstr) {}
};

struct LocalDate
{ 
  const TimePoint& point;
  const char* fmtstr;
  LocalDate(const TimePoint& point, const char* fmtstr = __default_fmtstr) : 
    point(point), fmtstr(fmtstr) {}
};

namespace io
{
s64 format(IO* io, const UTCDate& x);
s64 format(IO* io, const LocalDate& x);
}

/* ================================================================================================ TimeSpan
 */
struct TimeSpan
{
  s64 ns;

  static TimeSpan fromNanoseconds(s64 n);
  static TimeSpan fromMicroseconds(s64 n);
  static TimeSpan fromMilliseconds(s64 n);
  static TimeSpan fromSeconds(s64 n);
  static TimeSpan fromMinutes(s64 n);
  static TimeSpan fromHours(s64 n);
  static TimeSpan fromDays(s64 n);

  f64 toNanoseconds() const;
  f64 toMicroseconds() const;
  f64 toMilliseconds() const;
  f64 toSeconds() const;
  f64 toMinutes() const;
  f64 toHours() const;
  f64 toDays() const;
};

struct WithUnits
{
  const TimeSpan& x;
  u32 maxunits;
  WithUnits(const TimeSpan& x, u32 maxunits = 2) : x(x), maxunits(maxunits) {}
};

namespace io
{
  // prints timespan in seconds
s64 format(IO* io, const TimeSpan& x);
s64 format(IO* io, const WithUnits& x);

}

}

DefineNilValue(iro::TimePoint, {(u64)-1}, { return x.s == -1; });
// no nil for timespans rn

#endif // _iro_time_h
