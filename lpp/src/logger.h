/*
 *  Logging api.
 *
 *  We handle logging by creating a 'Logger' for various things (usually individual objects that need
 *  to report things, like a parser) which is named 'logger'. The macros just below here are used to
 *  guard calling the log function based on the logger's verbosity. 
 */

#ifndef _lake_logger_h
#define _lake_logger_h

#include "common.h"
#include "unicode.h"
#include "array.h"
#include "io.h"

#define __HELPER(v, ...) if ((u32)logger.verbosity <= (u32)Logger::Verbosity::v) logger.log(Logger::Verbosity::v, __VA_ARGS__)

#define TRACE(...) __HELPER(Trace, __VA_ARGS__)
#define DEBUG(...) __HELPER(Debug, __VA_ARGS__)
#define INFO(...) __HELPER(Info, __VA_ARGS__)
#define NOTICE(...) __HELPER(Notice, __VA_ARGS__)
#define WARN(...) __HELPER(Warn, __VA_ARGS__)
#define ERROR(...) __HELPER(Error, __VA_ARGS__)
#define FATAL(...) __HELPER(Fatal, __VA_ARGS__)

#define SCOPED_INDENT ScopedIndent __FILE__##__LINE__

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
		};
		typedef ::Flags<Flag> Flags;

		Flags flags;

		io::IO* io;
	};

	u64 indentation;

	u64 max_name_len;

	Array<Dest> destinations;

	b8   init();
	void deinit();

	void new_destination(str name, io::IO* d, Dest::Flags flags);
};

extern Log log; 


/* ================================================================================================ Log
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

		for (Log::Dest& destination : ::log.destinations)
		{
			write_preamble(v, destination);
			io::formatv(destination.io, args...);
		}
	}

private:

	void write_preamble(Verbosity v, Log::Dest& destination);

};

struct ScopedIndent
{
	ScopedIndent()
	{
		log.indentation += 1;
	}

	~ScopedIndent()
	{
		if (log.indentation)
			log.indentation -= 1;
	}
};

#endif
