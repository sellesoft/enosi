#include "Server.h"

#include "iro/Logger.h"
#include "iro/fs/File.h"

#include "lpp/Lpp.h"

#include "ctype.h"
#include "stdlib.h"

static Logger logger = 
  Logger::create("lpp.lsp"_str, Logger::Verbosity::Debug);

extern "C"
{
EXPORT_DYNAMIC
int lpp_lsp_getSemanticTokens(lua_State* L);
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

  lua.pushlightuserdata(this);
  lua.pushstring("setHandle"_str);
  lua.gettable(I.server);
  lua.pcall(1);

  lua.pushstring("processMessage"_str);
  lua.gettable(I.server);
  I.processMessage = lua.gettop();

#define addGlobalCFunc(name) \
  lua.pushcfunction(name); \
  lua.setglobal(STRINGIZE(name));

  addGlobalCFunc(lpp_lsp_getSemanticTokens);
    
#undef addGlobalCFunc

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

    if (!lua.isnil())
    {
      String response = lua.tostring();
      io::formatv(&fs::stdout, 
          "Content-Length: ", response.len, "\r\n\r\n",
          response);
      lua.pop();
    }
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Server::processFile(String name, String text)
{

}

extern "C"
{

/* ----------------------------------------------------------------------------
 */
EXPORT_DYNAMIC
int lpp_lsp_getSemanticTokens(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);

  auto* server = (Server*)lua.tolightuserdata(1);
  String name = lua.tostring(2);
  String text = lua.tostring(3);

  return 1;
}

}
