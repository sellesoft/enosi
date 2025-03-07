#include "Driver.h"

#include "Lpp.h"

#include "iro/containers/Slice.h"
#include "iro/fs/File.h"

namespace lpp
{

static Logger logger = 
  Logger::create("lpp.driver"_str, Logger::Verbosity::Info);

/* ----------------------------------------------------------------------------
 */
b8 Driver::init(const InitParams& params)
{
  consumers = params.consumers;

  streams.in.init(params.in_stream_name, params.in_stream);
  streams.out.init(params.out_stream_name, params.out_stream);
  streams.dep.init(params.dep_stream_name, params.dep_stream);
  streams.meta.init(params.meta_stream_name, params.meta_stream);

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Driver::deinit()
{
  consumers = {};

  streams.in.deinit();
  streams.out.deinit();
  streams.dep.deinit();
  streams.meta.deinit();

  require_dirs.deinit();
  passthrough_args.deinit();
  cpath_dirs.deinit();
  include_dirs.deinit();
}

/* ----------------------------------------------------------------------------
 */
Driver::ProcessArgsResult Driver::processArgs(Slice<String> args)
{
  const String* iarg = args.begin();

  b8 passed_input_arg = false;
  
  for (;iarg < args.end(); iarg += 1)
  {
    String arg = *iarg;

    if (arg.startsWith("--"_str))
    {
      arg = arg.sub(2);

      if (arg == "version"_str)
      {
        // Print version and exit.
        io::format(&fs::stdout, "0.1\n");
        return ProcessArgsResult::EarlyOut;
      }
      else
      {
        passthrough_args.push(*iarg);
      }
    }
    else if (arg.startsWith("-"_str))
    {
      arg = arg.sub(1);

      if (arg.startsWith("o"_str))
      {
        if (arg.len == 1)
        {
          iarg += 1;
          if (iarg >= args.end())
          {
            ERROR("expected an output file path after or joined with "
                  "'-o'\n");
            return ProcessArgsResult::Error;
          }
          arg = *iarg;
        }
        else
        {
          arg = arg.sub(1);
        }

        if (isnil(streams.out.name))
          streams.out.name = arg.allocateCopy();

        if (streams.out.stream == nullptr)
        {
          streams.out.file = 
            fs::File::from(
              streams.out.name,
                fs::OpenFlag::Create
              | fs::OpenFlag::Write
              | fs::OpenFlag::Truncate);

          if (isnil(streams.out.file))
          {
            ERROR("failed to open output file at path '", streams.out.name, 
                  "'\n");
            return ProcessArgsResult::Error;
          }

          streams.out.stream = &streams.out.file;
        }
      }
      else if (arg.startsWith("D"_str))
      {
        if (arg.len == 1)
        {
          iarg += 1;
          if (iarg >= args.end())
          {
            ERROR("expected a dependency file path after or joined "
                  "with '-o'\n");
            return ProcessArgsResult::Error;
          }
          arg = *iarg;
        }
        else
        {
          arg = arg.sub(1);
        }

        if (isnil(streams.dep.name))
          streams.dep.name = arg.allocateCopy();

        if (streams.dep.stream == nullptr)
        {
          streams.dep.file = 
            fs::File::from(
              streams.dep.name,
                fs::OpenFlag::Create
              | fs::OpenFlag::Write
              | fs::OpenFlag::Truncate);

          if (isnil(streams.dep.file))
          {
            ERROR("failed to open dependency file at path '",
                  streams.dep.name, "'\n");
            return ProcessArgsResult::Error;
          }

          streams.dep.stream = &streams.dep.file;
        }
      }
      else if (arg.startsWith("M"_str))
      {
        if (arg.len == 1)
        {
          iarg += 1;
          if (iarg >= args.end())
          {
            ERROR("expected a meta file path after or joined "
                  "with '-o'\n");
            return ProcessArgsResult::Error;
          }
          arg = *iarg;
        }
        else
        {
          arg = arg.sub(1);
        }

        if (isnil(streams.meta.name))
          streams.meta.name = arg.allocateCopy();

        if (streams.meta.stream == nullptr)
        {
          streams.meta.file = 
            fs::File::from(
              streams.meta.name,
                fs::OpenFlag::Create
              | fs::OpenFlag::Write
              | fs::OpenFlag::Truncate);

          if (isnil(streams.meta.file))
          {
            ERROR("failed to open meta file at path '",
                  streams.meta.name, "'\n");
            return ProcessArgsResult::Error;
          }

          streams.meta.stream = &streams.meta.file;
        }
      }
      else if (arg.startsWith("R"_str))
      {
        if (arg.len == 1)
        {
          iarg += 1;
          if (iarg >= args.end())
          {
            ERROR("expected a path to a require file after or joined "
                  "with '-R'\n");
            return ProcessArgsResult::Error;
          }
          arg = *iarg;
        }
        else
        {
          arg = arg.sub(1);
        }
        
        require_dirs.push(arg);
      }
      else if (arg.startsWith("C"_str))
      {
        if (arg.len == 1)
        {
          iarg += 1;
          if (iarg >= args.end())
          {
            ERROR("expected a path to a CPATH dir after or joined "
                  "with '-C'\n");
            return ProcessArgsResult::Error;
          }
          arg = *iarg;
        }
        else
        {
          arg = arg.sub(1);
        }

        cpath_dirs.push(arg);
      }
      else if (arg.startsWith("I"_str))
      {
        if (arg.len == 1)
        {
          iarg += 1;
          if (iarg >= args.end())
          {
            ERROR("expected a path to an include dir after or joined "
                  "with '-I'\n");
            return ProcessArgsResult::Error;
          }
          arg = *iarg;
        }
        else
        {
          arg = arg.sub(1);
        }

        include_dirs.push(arg);
      }
      else
      {
        passthrough_args.push(*iarg);
      }
    }
    else
    {
      if (!passed_input_arg)
      {
        passed_input_arg = true;

        if (isnil(streams.in.name))
          streams.in.name = arg.allocateCopy();

        if (streams.in.stream == nullptr)
        {
          streams.in.file = 
            fs::File::from(streams.in.name, fs::OpenFlag::Read);

          if (isnil(streams.in.file))
          {
            ERROR("failed to open input file at path '",
                  streams.in.name, "'\n");
            return ProcessArgsResult::Error;
          }

          streams.in.stream = &streams.in.file;
        }
      }
      else
      {
        passthrough_args.push(arg);
      }
    }
  }

  if (nullptr == streams.in.stream)
  {
    ERROR("no input file specified\n");
    return ProcessArgsResult::Error;
  }

  return ProcessArgsResult::Success;
}

/* ----------------------------------------------------------------------------
 */
b8 Driver::construct(Lpp* lpp)
{
  Lpp::InitParams params = {};

  auto constructLppStream = [&](Lpp::Stream* lppstream, NamedStream* stream)
  {
    lppstream->name = stream->name;
    lppstream->io = stream->stream;
  };

  constructLppStream(&params.streams.in, &streams.in);
  constructLppStream(&params.streams.out, &streams.out);
  constructLppStream(&params.streams.dep, &streams.dep);
  constructLppStream(&params.streams.meta, &streams.meta);

  params.args = passthrough_args.asSlice();

  params.cpath_dirs = cpath_dirs.asSlice();
  params.require_dirs = require_dirs.asSlice();
  params.include_dirs = include_dirs.asSlice();

  params.consumers = consumers;

  return lpp->init(params);
}

}
