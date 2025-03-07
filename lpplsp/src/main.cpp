#include "lpp/Lpp.h"

#include "iro/Common.h"
#include "iro/fs/FileSystem.h"
#include "iro/Logger.h"
#include "iro/Platform.h"

using namespace iro;

#include "Server.h"

int main()
{
  iro::log.init();
  defer { iro::log.deinit(); };

  auto logger = 
    Logger::create("lpplsp"_str, Logger::Verbosity::Trace);

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

  Server server;

  if (!server.init())
  {
    ERROR("failed to start language server\n");
    return 1;
  }

  server.loop();
  server.deinit();
}
