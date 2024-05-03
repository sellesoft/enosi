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

#define __HELPER(v, ...) if ((u32)logger.verbosity <= (u32)Logger::Verbosity::v) logger.log(Logger::Verbosity::v, __VA_ARGS__)

#define TRACE(...) __HELPER(Trace, __VA_ARGS__)
#define DEBUG(...) __HELPER(Debug, __VA_ARGS__)
#define INFO(...) __HELPER(Info, __VA_ARGS__)
#define NOTICE(...) __HELPER(Notice, __VA_ARGS__)
#define WARN(...) __HELPER(Warn, __VA_ARGS__)
#define ERROR(...) __HELPER(Error, __VA_ARGS__)
#define FATAL(...) __HELPER(Fatal, __VA_ARGS__)

#define SCOPED_INDENT iro::ScopedIndent __FILE__##__LINE__

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

	template<typename... T>
	void log(Verbosity v, T... args)
	{
		using enum Verbosity;

		for (Log::Dest& destination : iro::log.destinations)
		{
			writePrefix(v, destination);
			io::formatv(destination.io, args...);
		}
	}

private:

	// special case where we need to handle prefixing new lines, so we need to intercept 
	// the formatting. I need a better way to do this.
	// TODO(sushi) make a str specialization since we dont need to format it 
	// this is wrong and not working and i dont feel like fixing it atm
	//
	// it would probably be easier if i moved towards things emitting formatting 
	// via an iterator so i could stream each char and just check for newlines 
	// but that would make the whole formatting stuff so much more complex i think
//	template<typename T>
//	void log_single(Verbosity v, T arg, Log::Dest& dest, b8 last)
//	{
//		io::Memory m;
//		m.open();
//		io::format(&m, arg);
//
//		s64 linelen = 0;
//		for (s64 i = 0; i < m.len; i++)
//		{
//			if (m.buffer[i] == '\n')
//			{
//				dest.io->write({m.buffer + i - linelen, linelen+1});
//				if (i != m.len || !last)
//				{
//					write_prefix(v, dest);
//				}
//				linelen = 0;
//			}
//			else
//				linelen += 1;
//		}
//
//		m.close();
//	}

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
