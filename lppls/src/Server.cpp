#include "Server.h"

#include "iro/Logger.h"
#include "iro/fs/File.h"
#include "iro/Platform.h"
#include "iro/io/IO.h"
#include "iro/fs/Path.h"

#include "lpp/Lex.h"
#include "lpp/Lpp.h"
#include "lpp/Driver.h"
#include "lpp/Metaprogram.h"
#include "lpp/Source.h"

#include "ctype.h"
#include "stdlib.h"
#include "signal.h"

static Logger logger = 
  Logger::create("lpp.lsp"_str, Logger::Verbosity::Debug);

extern "C"
{
EXPORT_DYNAMIC
int lpp_lsp_processFile(lua_State* L);
int lpp_lsp_sendClientMessage(lua_State* L);
int lpp_lsp_readServerMessage(lua_State* L);
int lpp_lsp_sendServerMessage(lua_State* L);
int lpp_lsp_getPid(lua_State* L);
}

/* ----------------------------------------------------------------------------
 */
static s32 readContentLength(io::IO* in)
{
  const s32 header_buffer_size = 255;
  u8 header_buffer[header_buffer_size] = {};

  u8* cursor = header_buffer;

  for (;;)
  {
    if (cursor > header_buffer + header_buffer_size)
    {
      assert(false);
      return ERROR("content header is too long\n");
    }

    in->read({cursor, 1});

    if (*cursor == ':')
    {
      auto fieldname = String::from(header_buffer, cursor);

      if (fieldname == "Content-Length"_str)
      {
        cursor = header_buffer;
        in->read({cursor, 1});

        while (isspace(*cursor))
          in->read({cursor,1});

        while (*cursor != '\r')
        {
          cursor += 1;

          if (cursor > header_buffer + header_buffer_size)
            return ERROR("content header is too long\n");

          in->read({cursor, 1});
        }

        in->read({cursor, 3});

        return strtol((char*)header_buffer, nullptr, 0);
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

  return -1;
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

  lua.pushstring("processFromQueue"_str);
  lua.gettable(I.server);
  I.processFromQueue = lua.gettop();

  lua.pushstring("pollSubServers"_str);
  lua.gettable(I.server);
  I.pollSubServers = lua.gettop();

  lua.pushstring("exit"_str);
  lua.gettable(I.server);
  I.exit = lua.gettop();

#define addGlobalCFunc(name) \
  lua.pushcfunction(name); \
  lua.setglobal(STRINGIZE(name));

  addGlobalCFunc(lpp_lsp_processFile);
  addGlobalCFunc(lpp_lsp_sendClientMessage);
  addGlobalCFunc(lpp_lsp_readServerMessage);
  addGlobalCFunc(lpp_lsp_sendServerMessage);
  addGlobalCFunc(lpp_lsp_getPid);
    
#undef addGlobalCFunc

  log_file = 
    fs::File::from(
        "lpplsp.log"_str,
          fs::OpenFlag::Write
        | fs::OpenFlag::Truncate
        | fs::OpenFlag::Create);

#if 0
  clangd_log = 
    fs::File::from(
        "clangd.log"_str,
          fs::OpenFlag::Write
        | fs::OpenFlag::Truncate
        | fs::OpenFlag::Create);
#endif

  if (notnil(log_file))
  {
    iro::log.newDestination(
        "lpplsp.log"_str, &log_file, Log::Dest::Flags::none());
  }

#if 0
  // I'm disabling this for now, as the features gained from luals aren't
  // very important to me right now. We already get proper lua errors 
  // since the Metaprogram will emit them itself, and properly filtering luals
  // diagnostics to ignore those in the metaprogram unrelated to the input
  // file as well as mapping them is difficult. I would much rather focus
  // on getting clangd to work instead.

  String args[] = 
  {
    // "--loglevel=debug"_str,
    // "--logpath=luals.log"_str,
  };

  luals = 
    Process::spawn("lua-language-server"_str, nil, nil, false);
#endif

  clangd = Process::spawn("clangd"_str, nil, nil, false, false);

  INFO("initialized lpp lsp server\n");

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Server::deinit()
{
  log_file.close();
  clangd_log.close();
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

    while (platform::stdinHasData())
    {
      Bytes reserved_space = buffer.reserve(readContentLength(&fs::stdin));
      buffer.commit(reserved_space.len);

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

    lua.pushvalue(I.processFromQueue);

    if (!lua.pcall())
    {
      return ERROR("server.processFromQueue resulted in error:\n",
                   lua.tostring(), "\n");
    }

    lua.pushvalue(I.pollSubServers);

    if (!lua.pcall(0, 1))
    {
      return ERROR("server.pollSubServers resulted in error:\n",
                   lua.tostring(), "\n");
    }

    lua.pop();

  #if 0
    if (clangd.hasErrOutput())
    {
      u8 clangdStderr[255];
      u64 read = clangd.readstderr({clangdStderr, 255});
      if (read)
        clangd_log.write({clangdStderr, read});
    }
  #endif

    // TODO(sushi) set up an events api in platform so we don't have to do 
    //             this.
    platform::sleep(TimeSpan::fromMilliseconds(10));
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
static s32 getOrCreateSourceTable(
    LuaState&     lua, 
    s32           I_result, 
    const Source& source)
{
  assert(fs::Path::isRooted(source.name) && 
      "TODO(sushi) handle non-file sources");

  lua.pushstring(source.name);
  lua.gettable(I_result);
  if (lua.isnil())
  {
    lua.pop();
    lua.newtable();
    const s32 I_newtable = lua.gettop();
    lua.pushstring("diags"_str);
    lua.newtable();
    lua.settable(I_newtable);
    lua.pushstring("macro_calls"_str);
    lua.newtable();
    lua.settable(I_newtable);
    lua.pushstring("output_expansions"_str);
    lua.newtable();
    lua.settable(I_newtable);
    lua.pushstring("meta_expansions"_str);
    lua.newtable();
    lua.settable(I_newtable);
    lua.pushstring(source.name);
    lua.pushvalue(I_newtable);
    lua.settable(I_result);
  }
  return lua.gettop();
}

/* ----------------------------------------------------------------------------
 */
b8 Server::processFile()
{
  const s32 I_document = 2;
  const s32 I_overlay = 3;

  lua.pushstring("uri"_str);
  lua.gettable(I_document);
  String name = lua.tostring();
  lua.pop();

  name = name.sub("file://"_str.len);

  lua.pushstring("text"_str);
  lua.gettable(I_document);
  String text = lua.tostring();
  lua.pop();

  lua.newtable();
  const s32 I_result = lua.gettop();

  io::Memory output;
  io::Memory meta;
  io::Memory dep;
  output.open();
  meta.open();
  dep.open();
  defer
  {
    output.close();
    meta.close();
    dep.close();
  };

  // TODO(sushi) the way the result is organized here sucks, it needs to 
  //             be reorganized again to simplify Server.lua:processDocument

  struct MetaprogramConsumer : lpp::MetaprogramConsumer
  {
    LuaState& lua;
    String name;
    io::Memory& meta;

    s32 I_result;

    MetaprogramConsumer(
        LuaState& lua,
        String name,
        io::Memory& meta) : 
      lua(lua), 
      name(name),
      meta(meta) 
    {
      lua.newtable();
      I_result = lua.gettop();
    }

    void consumeDiag(
        const lpp::Metaprogram& mp, 
        const lpp::MetaprogramDiagnostic& diag) override
    {
      const s32 I_source = 
        getOrCreateSourceTable(lua, I_result, *diag.source);

      lua.newtable();
      const s32 I_diag = lua.gettop();

      lua.pushstring("range"_str);
      lua.newtable();
      const s32 I_range = lua.gettop();

      lua.pushstring("start"_str);
      lua.newtable();
      const s32 I_start = lua.gettop();

      lua.pushstring("end"_str);
      lua.newtable();
      const s32 I_end = lua.gettop();

      lua.pushstring("line"_str);
      lua.pushinteger(diag.loc-1);
      lua.pushstring("line"_str);
      lua.pushinteger(diag.loc-1);
      lua.settable(I_start);
      lua.settable(I_end);

      lua.pushstring("character"_str);
      lua.pushinteger(0);
      lua.pushstring("character"_str);
      lua.pushinteger(0);
      lua.settable(I_start);
      lua.settable(I_end);

      lua.settable(I_range);
      lua.settable(I_range);
      lua.settable(I_diag);

      lua.pushstring("message"_str);
      lua.pushstring(diag.message);
      lua.settable(I_diag);

      lua.pushstring("diags"_str);
      lua.gettable(I_source);
      const s32 I_diags = lua.gettop();

      lua.pushinteger(lua.objlen(I_diags) + 1);
      lua.pushvalue(I_diag);
      lua.settable(I_diags);

      lua.pop(3);
    }

    void consumeSection(
        const lpp::Metaprogram& mp,
        const lpp::Section& section,
        u64 expansion_start,
        u64 expansion_end) override
    {
      const s32 I_source = 
        getOrCreateSourceTable(lua, I_result, *mp.input);
        
      lpp::Token tok = mp.parser.lexer.tokens[section.token_idx];

      switch (tok.kind)
      {
      case lpp::Token::Kind::MacroSymbol:
      case lpp::Token::Kind::MacroSymbolImmediate:
        tok = mp.parser.lexer.tokens[section.token_idx + 1];
        break;

      default:
        lua.pop();
        return;
      }

      lua.newtable();
      const s32 I_call = lua.gettop();

      lua.newtable();
      const s32 I_macro_range = lua.gettop();
      lua.pushinteger(1);
      lua.pushinteger(tok.loc + 1);
      lua.settable(I_macro_range);
      lua.pushinteger(2);
      lua.pushinteger(tok.loc + tok.len);
      lua.settable(I_macro_range);
      lua.pushstring("macro_range"_str);
      lua.pushvalue(I_macro_range);
      lua.settable(I_call);
      lua.pop();

      lua.newtable();
      const s32 I_expansion = lua.gettop();
      lua.pushinteger(1);
      lua.pushinteger(expansion_start);
      lua.settable(I_expansion);
      lua.pushinteger(2);
      lua.pushinteger(expansion_end);
      lua.settable(I_expansion);
      lua.pushstring("expansion"_str);
      lua.pushvalue(I_expansion);
      lua.settable(I_call);
      lua.pop();

      lua.pushstring("macro_calls"_str);
      lua.gettable(I_source);
      const s32 I_macro_calls = lua.gettop();

      lua.pushinteger(lua.objlen(I_macro_calls) + 1);
      lua.pushvalue(I_call);
      lua.settable(I_macro_calls);

      lua.pop(3);
    }

    void consumeExpansions(
        const lpp::Metaprogram& mp,
        const lpp::ExpansionList& exps) override
    {
      const s32 I_source = 
        getOrCreateSourceTable(lua, I_result, *mp.input);

      lua.pushstring("meta_expansions"_str);
      lua.gettable(I_source);
      const s32 I_meta_expansions = lua.gettop();
      
      for (const lpp::Parser::LocMapping& exp : mp.parser.locmap)
      {
        Source::Loc from = mp.input->getLoc(exp.from);
        Source::Loc to = mp.meta.getLoc(exp.to);

        lua.pushinteger(lua.objlen(I_meta_expansions) + 1);
        lua.newtable();
        const s32 I_expansion = lua.gettop();

        lua.pushstring("from"_str);
        lua.newtable();
        const s32 I_from = lua.gettop();
        lua.pushstring("line"_str);
        lua.pushinteger(from.line-1);
        lua.settable(I_from);
        lua.pushstring("column"_str);
        lua.pushinteger(from.column-1);
        lua.settable(I_from);
        lua.pushstring("bytes"_str);
        lua.pushinteger(exp.from);
        lua.settable(I_from);
        lua.pushstring("len"_str);
        lua.pushinteger(exp.token.len);
        lua.settable(I_from);
        switch (exp.token.kind)
        {
        case lpp::Token::Kind::LuaBlock:
        case lpp::Token::Kind::LuaInline:
        case lpp::Token::Kind::LuaLine:
          lua.pushstring("is_lua"_str);
          lua.pushboolean(true);
          lua.settable(I_from);
          break;
        case lpp::Token::Kind::MacroIdentifier:
          lua.pushstring("is_macroid"_str);
          lua.pushboolean(true);
          lua.settable(I_from);
          break;
        }
        lua.settable(I_expansion);

        lua.pushstring("to"_str);
        lua.newtable();
        const s32 I_to = lua.gettop();
        lua.pushstring("line"_str);
        lua.pushinteger(to.line-1);
        lua.settable(I_to);
        lua.pushstring("column"_str);
        lua.pushinteger(to.column-1);
        lua.settable(I_to);
        lua.pushstring("bytes"_str);
        lua.pushinteger(exp.to);
        lua.settable(I_to);
        lua.settable(I_expansion);

        lua.settable(I_meta_expansions);
      }

      lua.pop();
      lua.pushstring("output_expansions"_str);
      lua.gettable(I_source);
      const s32 I_output_expansions = lua.gettop();
        
      for (const lpp::Expansion& exp : exps)
      {
        Source::Loc from = mp.input->getLoc(exp.from);
        Source::Loc to = mp.output->getLoc(exp.to);

        lua.pushinteger(lua.objlen(I_output_expansions) + 1);
        lua.newtable();
        const s32 I_expansion = lua.gettop();

        lua.pushstring("from"_str);
        lua.newtable();
        const s32 I_from = lua.gettop();
        lua.pushstring("line"_str);
        lua.pushinteger(from.line-1);
        lua.settable(I_from);
        lua.pushstring("column"_str);
        lua.pushinteger(from.column-1);
        lua.settable(I_from);
        lua.pushstring("bytes"_str);
        lua.pushinteger(exp.from);
        lua.settable(I_from);
        if (notnil(exp.invoking_macros))
        {
          lua.pushstring("macro_stack"_str);
          lua.newtable();
          const s32 I_macro_stack = lua.gettop();
          for (u64 loc : exp.invoking_macros)
          {
            Source::Loc macro_loc = mp.input->getLoc(loc);
            lua.pushinteger(lua.objlen(I_macro_stack) + 1);
            lua.pushinteger(macro_loc.line-1);
            lua.settable(I_macro_stack);
          }
          lua.settable(I_from);
        }
        lua.settable(I_expansion);

        lua.pushstring("to"_str);
        lua.newtable();
        const s32 I_to = lua.gettop();
        lua.pushstring("line"_str);
        lua.pushinteger(to.line-1);
        lua.settable(I_to);
        lua.pushstring("column"_str);
        lua.pushinteger(to.column-1);
        lua.settable(I_to);
        lua.pushstring("bytes"_str);
        lua.pushinteger(exp.to);
        lua.settable(I_to);
        lua.settable(I_expansion);

        lua.settable(I_output_expansions);
      }

      lua.pop(2);
    }

    void consumeMetafile(
        const lpp::Metaprogram& mp,
        String metafile) override
    {
      const s32 I_source = 
        getOrCreateSourceTable(lua, I_result, *mp.input);

      lua.pushstring("output"_str);
      lua.pushstring(metafile);
      lua.settable(I_source);

      lua.pop();
    }

  } meta_consumer { lua, name, meta };

  struct LexerConsumer : lpp::LexerConsumer
  {
    LuaState& lua;
    String name;

    s32 I_result;

    LexerConsumer(
        LuaState& lua,
        String name) : lua(lua), name(name) 
    {
      lua.newtable();
      I_result = lua.gettop();
    }

    void consumeDiag(
        const lpp::Lexer& lexer, 
        const lpp::LexerDiagnostic& diag) override
    {
      const s32 I_source =
        getOrCreateSourceTable(lua, I_result, *lexer.source);

      lua.newtable();
      const s32 I_diag = lua.gettop();

      lua.pushstring("range"_str);
      lua.newtable();
      const s32 I_range = lua.gettop();

      lua.pushstring("start"_str);
      lua.newtable();
      const s32 I_start = lua.gettop();

      lua.pushstring("end"_str);
      lua.newtable();
      const s32 I_end = lua.gettop();

      lua.pushstring("line"_str);
      lua.pushinteger(diag.line-1);
      lua.pushstring("line"_str);
      lua.pushinteger(diag.line-1);
      lua.settable(I_start);
      lua.settable(I_end);

      lua.pushstring("character"_str);
      lua.pushinteger(diag.column-1);
      lua.pushstring("character"_str);
      lua.pushinteger(diag.column-1);
      lua.settable(I_start);
      lua.settable(I_end);

      lua.settable(I_range);
      lua.settable(I_range);
      lua.settable(I_diag);

      lua.pushstring("message"_str);
      lua.pushstring(diag.message);
      lua.settable(I_diag);

      lua.pushstring("diags"_str);
      lua.gettable(I_source);
      const s32 I_diags = lua.gettop();

      lua.pushinteger(lua.objlen(I_diags) + 1);
      lua.pushvalue(I_diag);
      lua.settable(I_diags);

      lua.pop(3);
    }

  } lex_diag_consumer { lua, name };

  struct VFS : lpp::LppVFS
  {
    String overlay_path;
    String content;

    String open(String path) override
    {
      if (path == overlay_path)
        return content;
      return nil;
    }
  };

  // whatever
  VFS vfs;
  VFS* pvfs = nullptr;
  if (!lua.isboolean(I_overlay))
  {
    pvfs = &vfs;
    lua.pushstring("uri"_str);
    lua.gettable(I_overlay);
    String uri = lua.tostring();
    vfs.overlay_path = uri.sub("file://"_str.len);
    
    lua.pushstring("text"_str);
    lua.gettable(I_overlay);
    vfs.content = lua.tostring();
  }

  auto input = io::StringView::from(text);

  lpp::Driver::InitParams driver_params = 
  {
    .in_stream_name = name,
    .in_stream = &input,
    
    .out_stream = &output,
    .meta_stream = &meta,
    .dep_stream = &dep,

    .consumers = 
    {
      .meta = &meta_consumer,
      .lex_diag_consumer = &lex_diag_consumer,
    },

    .vfs = pvfs,
  };

  lpp::Driver driver;
  if (!driver.init(driver_params))
    return ERROR("failed to initialize driver for file '", name, "'\n");
  defer { driver.deinit(); };
  
  SmallArray<String, 16> args;

  // Instruct lpp to use full filenames when processing files to simplify 
  // mapping Sources to their internal document.
  args.push("--use-full-filenames"_str);

  auto cwd = fs::Path::cwd();
  fs::Path dir;
  dir.init();

  defer 
  {
    cwd.destroy();
    dir.destroy();
  };

  lua.pushstring("compile_command"_str);
  lua.gettable(I_document);
  const s32 I_compile_command = lua.gettop();

  if (lua.isnil(I_compile_command))
  {
    args.push(name);
  }
  else
  {
    lua.pushstring("dir"_str);
    lua.gettable(I_compile_command);
    String directory = lua.tostring();
    lua.pop();

    dir.set(directory);

    lua.pushstring("cmd"_str);
    lua.gettable(I_compile_command);
    const s32 I_cmd = lua.gettop();
    s32 args_len = lua.objlen();

    // We skip the first arg because it will be the program.
    for (s32 i = 2; i <= args_len; ++i)
    {
      lua.pushinteger(i);
      lua.gettable(I_cmd);
      args.push(lua.tostring());
      lua.pop();
    }

    lua.pop();
  }

  if (notnil(dir))
    dir.chdir();
  defer
  {
    if (notnil(dir))
      cwd.chdir();
  };

  switch (driver.processArgs(args.asSlice()))
  {
  case lpp::Driver::ProcessArgsResult::Error:
    return ERROR("lpp driver failed to process arguments\n");

  case lpp::Driver::ProcessArgsResult::EarlyOut:
    return ERROR("lpp driver encountered an early out argument\n");
  }

  if (!lua.isnil(I_compile_command))
  {
    lua.pushstring("output_path"_str);
    lua.gettable(I_document);
    if (lua.isnil())
    {
      // Try to extract whatever the compile commands say the output path
      // of the document is, as this likely maps to whatever the user 
      // uses to compile the resulting document is expecting.

      lua.pushstring("output_path"_str);
      lua.pushstring(driver.streams.out.name);
      lua.settable(I_document);
    }
    lua.pop();
  }

  lua.pop(); // compile_command

  lpp::Lpp lpp = {};
  if (!driver.construct(&lpp))
    return ERROR("lpp driver failed to construct an lpp instance\n");
  defer { lpp.deinit(); };

  lpp.run();

  lua.pushvalue(meta_consumer.I_result);

  lua.pushstring("meta"_str);
  lua.newtable();
  const s32 I_meta_result = lua.gettop();

  lua.pushstring("docs"_str);
  lua.pushvalue(meta_consumer.I_result);
  lua.settable(I_meta_result);

  /*if (!meta.asStr().isEmpty())*/
  /*{*/
  /*  lua.pushstring("output"_str);*/
  /*  lua.pushstring(output.asStr());*/
  /*  lua.settable(I_meta_result);*/
  /*}*/

  lua.settable(I_result);

  lua.pushstring("lex"_str);
  lua.newtable();
  const s32 I_lex_result = lua.gettop();

  lua.pushstring("docs"_str);
  lua.pushvalue(lex_diag_consumer.I_result);
  lua.settable(I_lex_result);

  lua.settable(I_result);

  if (!output.asStr().isEmpty())
  {
    lua.pushstring("output"_str);
    lua.pushstring(output.asStr());
    lua.settable(I_result);
  }

  lua.settop(I_result);

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Server::sendServerMessage()
{
  String client = lua.tostring(2);
  String message = lua.tostring(3);

  if (false || client == "lua"_str)
  {
    io::StaticBuffer<255> header_buffer;
    io::formatv(&header_buffer, "Content-Length: ", message.len, "\r\n\r\n");

    luals.write(header_buffer.asStr());
    luals.write(message);
  }
  else if (client == "clangd"_str)
  {
    io::StaticBuffer<255> header_buffer;
    io::formatv(&header_buffer, "Content-Length: ", message.len, "\r\n\r\n");

    clangd.write(header_buffer.asStr());
    clangd.write(message);
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Server::readServerMessage()
{
  String client = lua.tostring(2);

  if (false || client == "lua"_str)
  {
    if (!luals.hasOutput())
    {
      lua.pushnil();
      return true;
    }

    ProcessIO procio(luals);

    s32 content_length = readContentLength(&procio);

    io::Memory buffer;
    buffer.open(content_length);
    defer { buffer.close(); };

    Bytes reserved = buffer.reserve(content_length);
    buffer.commit(content_length);

    assert(content_length == luals.read(reserved));

    lua.pushstring(buffer.asStr());
  }
  else if (client == "clangd"_str)
  {
    if (!clangd.hasOutput())
    {
      lua.pushnil();
      return true;
    }

    ProcessIO procio(clangd);

    s32 content_length = readContentLength(&procio);

    io::Memory buffer;
    buffer.open(content_length);
    defer { buffer.close(); };

    Bytes reserved = buffer.reserve(content_length);
    assert(reserved.len == content_length);
    buffer.commit(content_length);

    u64 bytes_read = 0;
    while (bytes_read < reserved.len)
    {
      u64 read = 
        clangd.read({reserved.ptr+bytes_read, reserved.len-bytes_read});
      bytes_read += read;
    }

    lua.pushstring(buffer.asStr());
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Server::sendClientMessage()
{
  String message = lua.tostring(2);
  io::formatv(&fs::stdout, 
      "Content-Length: ", message.len, "\r\n\r\n",
      message);
  return true;
}

extern "C"
{

/* ----------------------------------------------------------------------------
 */
int lpp_lsp_processFile(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);
  auto* server = lua.tolightuserdata<Server>(1);
  server->processFile();
  return 1;
}

/* ----------------------------------------------------------------------------
 */
int lpp_lsp_sendServerMessage(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);
  auto* server = lua.tolightuserdata<Server>(1);
  server->sendServerMessage();
  return 1;
}

/* ----------------------------------------------------------------------------
 */
int lpp_lsp_readServerMessage(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);
  auto* server = lua.tolightuserdata<Server>(1);
  server->readServerMessage();
  return 1;
}

/* ----------------------------------------------------------------------------
 */
int lpp_lsp_sendClientMessage(lua_State* L)
{
  auto lua = LuaState::fromExistingState(L);
  auto* server = lua.tolightuserdata<Server>(1);
  server->sendClientMessage();
  return 1;
}

/* ----------------------------------------------------------------------------
 */
int lpp_lsp_getPid(lua_State* L)
{
  LuaState::fromExistingState(L).pushinteger(platform::getPid());
  return 1;
}

}
