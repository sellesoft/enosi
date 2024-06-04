#include "logger.h"

#include "time.h"

namespace iro
{

Log log;

/* ------------------------------------------------------------------------------------------------ Log::init
 */
b8 Log::init()
{
	// make sure we're not already initialized 
	if (notnil(destinations))
		return true;

    destinations = Array<Dest>::create(4);
    return true;
}

/* ------------------------------------------------------------------------------------------------ Log::deinit
 */
void Log::deinit()
{
    destinations.destroy();
}

/* ------------------------------------------------------------------------------------------------ Log::new_destination
 */
void Log::newDestination(str name, io::IO* d, Dest::Flags flags)
{
    destinations.push({name, flags, d});
}

/* ------------------------------------------------------------------------------------------------ Logger::init
 */
b8 Logger::init(str name_, Verbosity verbosity_)
{
    name = name_;
    verbosity = verbosity_;
    return true;
}

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

/* ------------------------------------------------------------------------------------------------ tryColor
 */
str tryColor(Log::Dest& d, Color col)
{
    if (!d.flags.test(Log::Dest::Flag::AllowColor))
        return ""_str;

#define x(c, n) \
    case Color::c: return "\e[" n "m"_str;

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

/* ------------------------------------------------------------------------------------------------ Logger::writePreamble
 */
void Logger::writePrefix(Verbosity v, Log::Dest& d)
{
    using enum Log::Dest::Flag;

    if (d.flags.test(ShowDateTime))
    {
        char buffer[64];
        time_t t = time(0);
        struct tm* tm = localtime(&t);
        size_t written = strftime(buffer, 64, "[%Y-%m-%d %H:%M:%S] ", tm);
        io::format(d.io, str{(u8*)buffer, written});
    }

    if (d.flags.test(ShowCategoryName))
    {
        if (d.flags.test(TrackLongestName))
        {
            if (name.len > iro::log.max_name_len)
                iro::log.max_name_len = name.len;
            for (s32 i = 0; i < iro::log.max_name_len - name.len; i++)
                io::format(d.io, " ");
        }

        io::formatv(d.io, name, ": ");
    }

    if (d.flags.test(ShowVerbosity))
    {
        using enum Verbosity;
        using enum Color;

        if (d.flags.test(PadVerbosity))
        {
            switch (v)
            {
                case Trace:  io::formatv(d.io, tryColor(d, Cyan),    " trace"_str, tryColor(d, Reset)); break;
				case Debug:  io::formatv(d.io, tryColor(d, Green),   " debug"_str, tryColor(d, Reset)); break;
				case Info:   io::formatv(d.io, tryColor(d, Blue),    "  info"_str, tryColor(d, Reset)); break;
				case Notice: io::formatv(d.io, tryColor(d, Magenta), "notice"_str, tryColor(d, Reset)); break;
				case Warn:   io::formatv(d.io, tryColor(d, Yellow),  "  warn"_str, tryColor(d, Reset)); break;
				case Error:  io::formatv(d.io, tryColor(d, Red),     " error"_str, tryColor(d, Reset)); break;
				case Fatal:  io::formatv(d.io, tryColor(d, Red),     " fatal"_str, tryColor(d, Reset)); break;
            }
        }
        else
        {
            switch (v)
            {
                case Trace:  io::formatv(d.io, tryColor(d, Cyan),    "trace"_str,  tryColor(d, Reset)); break;
				case Debug:  io::formatv(d.io, tryColor(d, Green),   "debug"_str,  tryColor(d, Reset)); break;
				case Info:   io::formatv(d.io, tryColor(d, Blue),    "info"_str,   tryColor(d, Reset)); break;
				case Notice: io::formatv(d.io, tryColor(d, Magenta), "notice"_str, tryColor(d, Reset)); break;
				case Warn:   io::formatv(d.io, tryColor(d, Yellow),  "warn"_str,   tryColor(d, Reset)); break;
				case Error:  io::formatv(d.io, tryColor(d, Red),     "error"_str,  tryColor(d, Reset)); break;
                case Fatal:  io::formatv(d.io, tryColor(d, Red),     "fatal"_str,  tryColor(d, Reset)); break;
            }
        }

        io::format(d.io, ": ");
    }

    for (s32 i = 0; i < iro::log.indentation; i++)
        d.io->write(" "_str);
}

}
