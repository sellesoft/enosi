#include "Server.h"

#include "iro/Logger.h"
#include "iro/fs/File.h"
#include "iro/Platform.h"

#include "lpp/Lpp.h"
#include "lpp/Driver.h"

#include "ctype.h"
#include "lpp/Metaprogram.h"
#include "stdlib.h"
#include <csignal>

static Logger logger = 
  Logger::create("lpp.lsp"_str, Logger::Verbosity::Debug);

extern "C"
{
EXPORT_DYNAMIC
int lpp_lsp_processFile(lua_State* L);
}

/* ----------------------------------------------------------------------------
 */
b8 Server::init()
{
  if (!lua.init())
    return ERROR("failed to initialize lua for lsp server\n");

  if (!lua.require("Server"_str))
    return false;

  I.server = lua.gettop();

  lua.pushstring("setHandle"_str);
  lua.gettable(I.server);
  lua.pushlightuserdata(this);
  if (!lua.pcall(1))
    return ERROR("failed to set server lua module handle:\n",
                 lua.tostring(), "\n");

  lua.pushstring("processMessage"_str);
  lua.gettable(I.server);
  I.processMessage = lua.gettop();

  lua.pushstring("exit"_str);
  lua.gettable(I.server);
  I.exit = lua.gettop();


#define addGlobalCFunc(name) \
  lua.pushcfunction(name); \
  lua.setglobal(STRINGIZE(name));

  addGlobalCFunc(lpp_lsp_processFile);
    
#undef addGlobalCFunc

  log_file = 
    fs::File::from(
        "lpplsp.log"_str,
          fs::OpenFlag::Write
        | fs::OpenFlag::Truncate
        | fs::OpenFlag::Create);

  if (notnil(log_file))
  {
    iro::log.newDestination(
        "lpplsp.log"_str, &log_file, Log::Dest::Flags::none());
  }

  INFO("initialized lpp lsp server\n");

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Server::deinit()
{
  lua.deinit();
}

/* ----------------------------------------------------------------------------
 */
b8 Server::loop()
{
  INFO("entering lsp loop\n");

  io::Memory buffer;
  buffer.open();

  for (;;)
  {
    buffer.clear();
    INFO("loop\n");

    const s32 header_buffer_size = 255;
    u8 header_buffer[header_buffer_size] = {};

    u8* cursor = header_buffer;

    Bytes reserved_space = {};

    for (;;)
    {
      if (cursor > header_buffer + header_buffer_size)
        return ERROR("content header is too long\n");

      fs::stdin.read({cursor, 1});

      if (*cursor == ':')
      {
        auto fieldname = String::from(header_buffer, cursor);

        if (fieldname == "Content-Length"_str)
        {
          cursor = header_buffer;
          fs::stdin.read({cursor, 1});

          while (isspace(*cursor))
            fs::stdin.read({cursor,1});

          while (*cursor != '\r')
          {
            cursor += 1;
            
            if (cursor > header_buffer + header_buffer_size)
              return ERROR("content header is too long\n");

            fs::stdin.read({cursor, 1});
          }

          s32 content_length = strtol((char*)header_buffer, nullptr, 0);

          reserved_space = buffer.reserve(content_length);
          buffer.commit(content_length);
          *header_buffer = '\r';
          cursor = header_buffer + 1;
        }
      }
      else if (header_buffer[0] == '\r' && header_buffer[1] == '\n' &&
               header_buffer[2] == '\r' && header_buffer[3] == '\n')
      {
        break;
      }
      else
      {
        cursor += 1;
      }
    }

    fs::stdin.read(reserved_space);

    lua.pushvalue(I.processMessage);
    lua.pushstring(buffer.asStr());

    if (!lua.pcall(1, 1))
    {
      return ERROR("server.processMessage resulted in error:\n",
                   lua.tostring(), "\n");
    }

    if (lua.isstring())
    {
      String response = lua.tostring();
      io::formatv(&fs::stdout, 
          "Content-Length: ", response.len, "\r\n\r\n",
          response);
      lua.pop();
    }
    else if (lua.istable())
    {
      if (lua.rawequal(-1, I.exit))
        break;
    }
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Server::processFile()
{
  String name = lua.tostring(2);
  String text = lua.tostring(3);

  lua.newtable();
  const s32 I_result = 4;

  lua.newtable();
  const s32 I_result_diags = 5;
  lua.pushstring("diags"_str);
  lua.pushvalue(I_result_diags);
  lua.settable(I_result);

  struct MetaprogramDiagnosticConsumer : lpp::MetaprogramDiagnosticConsumer
  {
    LuaState& lua;
    String name;

    MetaprogramDiagnosticConsumer(
        LuaState& lua,
        String name) : lua(lua), name(name) {}

    void consume(
        const lpp::Metaprogram& mp, 
        const lpp::MetaprogramDiagnostic& diag) override
    {
      INFO("source name: ", diag.source->name, "\n");
      if (diag.source->name == name)
      {
        lua.newtable();
        const s32 I_diag = lua.gettop();

        lua.pushstring("line"_str);
        lua.pushinteger(diag.loc);
        lua.settable(I_diag);

        lua.pushstring("message"_str);
        lua.pushstring(diag.message);
        lua.settable(I_diag);

        lua.tableInsert(I_result_diags, I_diag);

        lua.pop();
      }
    }

  } meta_diag_consumer { lua, name };

  auto input = io::StringView::from(text);

  lpp::Driver::InitParams driver_params = 
  {
    .in_stream_name = name,
    .in_stream = &input,

    .consumers = 
    {
      .meta_diag_consumer = &meta_diag_consumer,
    }
  };

  lpp::Driver driver;
  if (!driver.init(driver_params))
    return ERROR("failed to initialize driver for file '", name, "'\n");
  defer { driver.deinit(); };
  
  String args[] = 
  {
    name,
  };

  switch (driver.processArgs(Slice<String>::from(args, 1)))
  {
  case lpp::Driver::ProcessArgsResult::Error:
    return ERROR("lpp driver failed to process arguments\n");

  case lpp::Driver::ProcessArgsResult::EarlyOut:
    return ERROR("lpp driver encountered an early out argument\n");
  }

  lpp::Lpp lpp = {};
  if (!driver.construct(&lpp))
    return ERROR("lpp driver failed to construct an lpp instance\n");
  defer { lpp.deinit(); };

  lpp.run();

  lua.pop();

  INFO("successfully processed file\n");

  return true;
}

extern "C"
{

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
int lpp_lsp_processFile(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);

  lua.stackDump();

  auto* server = (Server*)lua.tolightuserdata(1);

  server->processFile();

  return 1;
}

}
