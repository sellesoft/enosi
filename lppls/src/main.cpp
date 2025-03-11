#include "iro/io/IO.h"
#include "lpp/Lpp.h"

#include "iro/Common.h"
#include "iro/Logger.h"
#include "iro/Platform.h"
#include <csignal>

using namespace iro;

#include "Server.h"

int main()
{
  iro::log.init();
  defer { iro::log.deinit(); };

  auto logger = 
    Logger::create("lppls"_str, Logger::Verbosity::Trace);

  {
    using namespace fs;
    using enum OpenFlag;
    using enum Log::Dest::Flag;

    iro::log.newDestination("stderr"_str, &fs::stderr,
          ShowCategoryName
        | ShowVerbosity);

    auto* f = mem::stl_allocator.construct<File>();
    *f = 
      File::from(
          "lpplsp.log"_str,
            Write
          | Truncate
          | Create);

    if (notnil(*f))
    {
      iro::log.newDestination(
          "lpplsp.log"_str, f, Log::Dest::Flags::none());
    }
  }

  auto pidfile = 
    fs::File::from("lpplsp.pid"_str, 
          fs::OpenFlag::Create
        | fs::OpenFlag::Write);
  if (isnil(pidfile))
  {
    ERROR("failed to make pidfile\n");
    return 1;
  }
  io::format(&pidfile, platform::getPid());

  INFO("procid: ", platform::getPid(), "\n");
  // raise(SIGSTOP);

  Server server;

  if (!server.init())
  {
    ERROR("failed to start language server\n");
    return 1;
  }

  server.loop();
  server.deinit();
}
