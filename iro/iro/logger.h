/*
 *  Logging api.
 *
 *  We handle logging by creating a 'Logger' for various things (usually individual objects that need
 *  to report things, like a parser) which is named 'logger'. The macros just below here are used to
 *  guard calling the log function based on the logger's verbosity. 
 */

#ifndef _iro_logger_h
#define _iro_logger_h

#include "common.h"
#include "unicode.h"
#include "containers/array.h"
#include "io/io.h"
#include "io/format.h"

#define __HELPER(v, ...) (((u32)logger.verbosity <= (u32)Logger::Verbosity::v) && (logger.log(Logger::Verbosity::v, __VA_ARGS__), true))

#define TRACE(...) __HELPER(Trace, __VA_ARGS__)
#define DEBUG(...) __HELPER(Debug, __VA_ARGS__)
#define INFO(...) __HELPER(Info, __VA_ARGS__)
#define NOTICE(...) __HELPER(Notice, __VA_ARGS__)
#define WARN(...) __HELPER(Warn, __VA_ARGS__)
#define ERROR(...) __HELPER(Error, __VA_ARGS__)
#define FATAL(...) __HELPER(Fatal, __VA_ARGS__)

#define __SCOPED_INDENT(line) iro::ScopedIndent __scoped_indent##line
#define _SCOPED_INDENT(line) __SCOPED_INDENT(line)
#define SCOPED_INDENT _SCOPED_INDENT(__LINE__)

#define DEBUGEXPR(expr) DEBUG(STRINGIZE(expr) ": ", expr, '\n')

namespace iro
{

/* ================================================================================================ Log
 *  Central log object that keeps track of destinations, which are named io targets that have 
 *  some settings to customize their output.
 */
struct Log
{
	struct Dest
	{
		str name;

		enum class Flag : u8
		{
			// prefix the message with the date and time
			ShowDateTime,
			// show the name of the category that is being logged from
			ShowCategoryName,
			// show the verbosity level before the message
			ShowVerbosity,
			// pad the verbosity level to the right so that messages printed at different levels stay aligned
			PadVerbosity,
			// allow color
			AllowColor,
			// track the longest category name and indent all other names so that they stay aligned
			TrackLongestName,
			// if a logger logs a message that spans multiple lines, prefix each line with the enabled information above
			// TODO(sushi) implement this eventually its not that important
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



/* ================================================================================================ Logger
 *  A logger, which is a named thing that logs stuff to the log. Each logger has its own verbosity 
 *  setting so we can be selective about what we want to see in a log.
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

	static Logger create(str name, Verbosity verbosity)
	{
		Logger out = {};
		out.init(name, verbosity);
		return out;
	}

	b8 init(str name, Verbosity verbosity);

	// this language is so fucking dumb man
	// This works for now, so I'm going to leave it be, but this should 
	// really be replaced later on.
	// The reason why this has to be put in a struct is because (at least on clang)
	// trying to pass an integer literal to a non-type template parameter in 
	// logSingle causes a compiler error.
	// This could probably be pulled into log.
	struct u32Range { u32 start, end; }; 

	template<io::Formattable... T>
	void log(Verbosity v, T&... args)
	{
		if (isnil(iro::log.destinations))
			return;


		for (Log::Dest& destination : iro::log.destinations)
		{
			writePrefix(v, destination);

			if (destination.flags.test(Log::Dest::Flag::PrefixNewlines))
				logSingle<u32Range{0, sizeof...(args)-1}, T...>(*this, v, destination, args...);
			else
				io::formatv(destination.io, args...);
		}
	}

	template<io::Formattable... T>
	void log(Verbosity v, T&&... args) { log(v, args...); }

private:

	template<u32Range Range, io::Formattable T, io::Formattable... Rest>
	static void logSingle(Logger& logger, Verbosity v, Log::Dest& dest, T& arg, Rest&... rest)
	{

		struct Intercept : public io::IO
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
					if (slice.ptr[i] == '\n')
					{
						dest.io->write({slice.ptr + i - linelen, linelen + 1});
						if constexpr (Range.start == Range.end)
						{
							if (i != slice.len - 1)
								logger.writePrefix(v, dest);
						}
						else
							logger.writePrefix(v, dest);
						linelen = 0;
					}
					else
						linelen += 1;
				}
				if (linelen)
					dest.io->write({slice.ptr+slice.len-linelen,linelen});
				return slice.len;
			}

			s64 read(Bytes slice) override { return 0; }
		};

		auto intercept = Intercept(dest, logger, v);
		io::formatv(&intercept, arg);

		if constexpr (Range.start != Range.end)
		{
			logSingle<u32Range{Range.start+1, Range.end}, Rest...>(logger, v, dest, rest...);
		}
	}

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
