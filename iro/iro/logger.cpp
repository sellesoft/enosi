#include "logger.h"

#include "time.h"

namespace iro
{

Log log;

/* ------------------------------------------------------------------------------------------------
 */
b8 Log::init()
{
	// make sure we're not already initialized 
	if (notnil(destinations))
		return true;

    destinations = Array<Dest>::create(4);
    return true;
}

/* ------------------------------------------------------------------------------------------------
 */
void Log::deinit()
{
    destinations.destroy();
}

/* ------------------------------------------------------------------------------------------------
 */
void Log::newDestination(str name, io::IO* d, Dest::Flags flags)
{
    destinations.push({name, flags, d});
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Logger::init(str name_, Verbosity verbosity_)
{
    name = name_;
    verbosity = verbosity_;
    return true;
}


/* ------------------------------------------------------------------------------------------------
 */
str tryColor(Log::Dest& dest, color::Color color)
{
	if (!dest.flags.test(Log::Dest::Flag::AllowColor))
		return ""_str;

	return color::getColor(color);
}

/* ------------------------------------------------------------------------------------------------
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
        using enum color::Color;

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
