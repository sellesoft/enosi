/*
 *  Logging api.
 *
 *  We handle logging by creating a 'Logger' for various things (usually 
 *  individual objects that need to report things, like a parser) which is 
 *  named 'logger'. The macros just below here are used to guard calling the 
 *  log function based on the logger's verbosity. 
 *
 *  TODOs
 *
 *    Add a message length limit and word wrapping.
 *    And maybe be a little fancy and wrap to the same indentation as the 
 *    current line and stsuff yeah.
 */

#ifndef _iro_logger_h
#define _iro_logger_h

#include "common.h"
#include "unicode.h"
#include "containers/array.h"
#include "io/io.h"
#include "io/format.h"

#define __HELPER(v, r, nf, ...) \
  ((((u32)logger.verbosity <= (u32)Logger::Verbosity::v) && \
   (logger.log(Logger::Verbosity::v, nf, __VA_ARGS__), true)), r)

#define TRACE(...) __HELPER(Trace, true, false, __VA_ARGS__)
#define DEBUG(...) __HELPER(Debug, true, false, __VA_ARGS__)
#define INFO(...) __HELPER(Info, true, false, __VA_ARGS__)
#define NOTICE(...) __HELPER(Notice, true, false, __VA_ARGS__)
#define WARN(...) __HELPER(Warn, true, false, __VA_ARGS__)
#define ERROR(...) __HELPER(Error, false, false, __VA_ARGS__)
#define FATAL(...) __HELPER(Fatal, false, false, __VA_ARGS__)
// Alternatives for logging w/o most formatting. This still handles colors.
#define ERROR_NOFMT(...) __HELPER(Error, false, true, __VA_ARGS__)

#define __SCOPED_INDENT(line) iro::ScopedIndent __scoped_indent##line
#define _SCOPED_INDENT(line) __SCOPED_INDENT(line)
#define SCOPED_INDENT _SCOPED_INDENT(__LINE__)

#define DEBUGEXPR(expr) DEBUG(STRINGIZE(expr) ": ", expr, '\n')

namespace iro
{

/* ============================================================================
 *  Central log object that keeps track of destinations, which are io 
 *  targets that have some settings to customize their output.
 */
struct Log
{
  struct Dest
  {
    str name;

    enum class Flag : u8
    {
      // Prefix the message with the date and time.
      ShowDateTime,
      // Show the name of the category that is being logged from.
      ShowCategoryName,
      // Show the verbosity level before the message.
      ShowVerbosity,
      // Pad the verbosity level to the right so that messages printed at 
      // different levels stay aligned.
      PadVerbosity,
      // Allow color.
      AllowColor,
      // Track the longest category name and indent all other names so that 
      // they stay aligned.
      TrackLongestName,
      // If a logger logs a message that spans multiple lines, prefix each 
      // line with the enabled information above.
      PrefixNewlines,
    };
    typedef iro::Flags<Flag> Flags;

    Flags flags;

    io::IO* io;
  };

  u64 indentation;
  u64 max_name_len;

  Array<Dest> destinations;

  b8   init();
  void deinit();

  void newDestination(str name, io::IO* d, Dest::Flags flags);
};

DefineFlagsOrOp(Log::Dest::Flags, Log::Dest::Flag);

extern Log log;


// Goofy color formatting via types. This allows using 
// color::<color name> to start a colored region which may be 
// reset by color::reset.
//
// All colors can also wrap another formattable thing such that
// only that thing is colored, eg.
//  color::red("hello")
//  color::blue(1)
//  color::green(io::SanitizeControlCharacters("hello"))
namespace color
{

enum class Color
{
    Reset,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
};

/* ----------------------------------------------------------------------------
 */
static str getColor(color::Color col)
{
#define x(c, n) \
  case color::Color::c: return "\e[" n "m"_str;

    switch (col)
    {
    x(Reset,   "0");
    x(Black,   "30");
    x(Red,     "31");
    x(Green,   "32");
    x(Yellow,  "33");
    x(Blue,    "34");
    x(Magenta, "35");
    x(Cyan,    "36");
    x(White,   "37");
    }

#undef x
}

template<typename T>
struct Colored
{
  Color color;
  b8 enabled;
  T& x;
  Colored(Color color, T& x) : color(color), x(x), enabled(true) {}
};

template<typename T>
T& sanitizeColored(T& x, Log::Dest& dest) { return x; }

template<typename T>
Colored<T>& sanitizeColored(Colored<T>& x, Log::Dest& dest) 
{
  if (!dest.flags.test(Log::Dest::Flag::AllowColor))
    x.enabled = false;
  return x;
}

struct Colorer
{
  b8 enabled;
  Color color;
  Colorer(Color color) : enabled(true), color(color) {}

  template<typename T>
  Colored<T> operator()(T&& x) { return Colored<T>(color, x); }
};

static const Colorer sanitizeColored(Colorer x, Log::Dest& dest)
{
  if (!dest.flags.test(Log::Dest::Flag::AllowColor))
    x.enabled = false;
  return x;
}

#define colorer(fname, cname) static Colorer fname(Color::cname);

colorer(black, Black);
colorer(red, Red);
colorer(green, Green);
colorer(yellow, Yellow);
colorer(blue, Blue);
colorer(magenta, Magenta);
colorer(cyan, Cyan);
colorer(white, White);
colorer(reset, Reset);

#undef colorfunc

}

namespace io
{
static s64 format(io::IO* out, const color::Colorer& x)
{
  if (!x.enabled)
    return 0;
  return io::format(out, color::getColor(x.color));
}

template<typename T>
s64 format(io::IO* out, color::Colored<T>& x)
{
  if (!x.enabled)
    return io::format(out, x.x);

  return io::formatv(out, 
      getColor(x.color), x.x, getColor(color::Color::Reset));
}
}

/* ============================================================================
 *  A logger, which is a named thing that logs stuff to the log. Each logger 
 *  has its own verbosity setting so we can be selective about what we want to 
 *  see in a log.
 */
struct Logger
{
  str name;

  enum class Verbosity : u8
  {
    Trace,
    Debug,
    Info,
    Notice,
    Warn,
    Error,
    Fatal,
  };

  Verbosity verbosity;

  b8 need_prefix;

  static Logger create(str name, Verbosity verbosity)
  {
    Logger out = {};
    out.init(name, verbosity);
    return out;
  }

  b8 init(str name, Verbosity verbosity);

  template<io::Formattable... T>
  void log(Verbosity v, b8 nofmt, T&... args)
  {
    if (isnil(iro::log.destinations))
      return;

    if (nofmt)
    {
      for (Log::Dest& destination : iro::log.destinations)
      {
        io::formatv(destination.io, 
            color::sanitizeColored(args, destination)...);
      }
    }
    else
    {
      for (Log::Dest& destination : iro::log.destinations)
      {
        if (destination.flags.test(Log::Dest::Flag::PrefixNewlines))
        {
          struct Intercept : io::IO
          {
            Log::Dest& dest;
            Logger& logger;
            Verbosity v;

            Intercept(Log::Dest& dest, Logger& logger, Verbosity v) : 
              dest(dest), logger(logger), v(v) {}

            s64 write(Bytes slice) override
            {
              u64 linelen = 0;
              for (u64 i = 0; i < slice.len; ++i)
              {
                if (logger.need_prefix)
                {
                  logger.writePrefix(v, dest);
                  logger.need_prefix = false;
                }

                if (slice.ptr[i] == '\n')
                {
                  dest.io->write({slice.ptr+i-linelen,linelen+1});
                  logger.need_prefix = true;
                  linelen = 0;
                }
                else
                  linelen += 1;

              }

              if (linelen)
                dest.io->write({slice.ptr+slice.len-linelen,linelen});
              return slice.len;
            }

            s64 read(Bytes bytes) override { return 0; }
          };

          auto intercept = Intercept(destination, *this, v);
          io::formatv(&intercept, args...);
        }
        else
        {
          writePrefix(v, destination);
          io::formatv(destination.io, 
              color::sanitizeColored(args, destination)...);
        }
      }
    }
  }

  template<io::Formattable... T>
  void log(Verbosity v, b8 nofmt, T&&... args) { log(v, nofmt, args...); }

private:

  void writePrefix(Verbosity v, Log::Dest& destination);

};

struct ScopedIndent
{
  ScopedIndent()
  {
    iro::log.indentation += 1;
  }

  ~ScopedIndent()
  {
    if (iro::log.indentation)
      iro::log.indentation -= 1;
  }
};

}

#endif
