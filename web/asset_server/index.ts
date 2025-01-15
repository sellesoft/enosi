const fs = await import('node:fs/promises');

import * as bun from 'bun';

async function pathExists(path: Bun.PathLike)
{
  return await fs.stat(path.toString()).catch(() => false);
}

const pw_file = Bun.file("upload_pw.txt");

if (!await pw_file.exists())
  throw new Error("failed to find upload_pw.txt!");

const pw_hash = await Bun.password.hash((await pw_file.text()).trim());

/* ----------------------------------------------------------------------------
 */
function errResponse(message: string)
{
  console.log("error: ", message);
  return new Response(message);
}

/* ----------------------------------------------------------------------------
 */
// async function handleDownload(req: Request, params: URLSearchParams)
// {
//   console.log("download request");
// 
//   const platform = params.get("platform");
//   if (platform == null)
//     return errResponse("no platform specified");
// 
//   const src_dir = `assets/${platform}`;
//   if (!await pathExists(src_dir))
//     return errResponse("unknown platform");
// 
//   const name = params.get("name");
//   if (name == null)
//     return errResponse("no file specified");
// 
//   const src_path = `${src_dir}/${name}`;
//   if (!await pathExists(src_path))
//     return errResponse(`no file ${name} on server under ${platform}`);
// 
//   return new Response(Bun.file(src_path));
// }

/* ----------------------------------------------------------------------------
 */
//let processing_upload = false;
//async function handleUpload(req: Request, params: URLSearchParams)
//{
//  console.log("upload request");
//
//  const pw = params.get("pw");
//  if (pw == null)
//    return errResponse("password not provided");
//
//  if (!await Bun.password.verify(pw, pw_hash))
//    return errResponse("incorrect password");
//
//  const platform = params.get("platform");
//  if (platform == null)
//    return errResponse("no platform specified");
//
//  const dest_dir = `assets/${platform}`;
//  if (!await pathExists(dest_dir))
//    return errResponse("unknown platform");
//
//  const name = params.get("name");
//  if (name == null)
//    return errResponse("no name provided");
//
//  const dest_path = `${dest_dir}/${name}`;
//
//  console.log(`upload ${dest_path}`);
//
//  // Ensure file exists.
//  // (await fs.open(dest_path, "a")).close();
//
//  const dest_file = Bun.file(dest_path);
//
//  const writer = dest_file.writer();
//
//  req.blob().then(async (blob) =>
//  {
//    console.log("incrementally writing file");
//    for await (const chunk of blob.stream())
//    {
//      const bytes_written = writer.write(chunk);
//      console.log("wrote ", bytes_written, " bytes");
//    }
//  })
//
//  return new Response(`uploaded ${dest_path}`);
//}

type WebSocketData = 
{
  pathname: string,
  params: URLSearchParams,
  handler: Function,
}

/* ----------------------------------------------------------------------------
 */
function handleDownload(ws: bun.ServerWebSocket<WebSocketData>)
{
}

/* ----------------------------------------------------------------------------
 */
async function handleUpload(ws: bun.ServerWebSocket<WebSocketData>)
{
  console.log("upload request");

  let size: Number | undefined = undefined;
  let bytes_recieved = 0;

  const err = (message: string) => ws.close(1011, message);

  const params = ws.data.params;

  const pw = params.get("pw")
  if (pw == null)
    return err("no password provided");

  if (!await Bun.password.verify(pw, pw_hash))
    return err("incorrect password");

  const platform = params.get("platform");
  if (platform == null)
    return err("no platform specified");

  const dest_dir = `assets/${platform}`;
  if (!await pathExists(dest_dir))
    return err("unknown platform");

  const name = params.get("name");
  if (name == null)
    return err("no name provided");

  const dest_path = `${dest_dir}/${name}`;

  console.log("upload", dest_path);

  const dest_file = Bun.file(dest_path);

  const writer = dest_file.writer();

  ws.data.handler = async (message: string | Buffer) =>
  {
    if (message == undefined)
    {
      ws.close(1011, "recieved undefined data");
      return;
    }

    if (size == null)
    {
      size = Number(message);
    }
    else if (message == "done")
    {
      console.log("\ndone");
      writer.end();
      ws.close();
    }
    else
    {
      bytes_recieved += (message as Buffer).length;
      writer.write(message);
      process.stdout.clearLine(0);
      process.stdout.cursorTo(0);
      process.stdout.write(
        `${bytes_recieved} / ${size} ` +
        `(${Math.floor(bytes_recieved / size * 100)}%)`);
    }

    ws.send("poll");
  }

  ws.send("size");
}

/* ----------------------------------------------------------------------------
 */
const server = Bun.serve<WebSocketData>(
{
  // Prevent showing source code when an error occurs.
  development: false,
  port: 3000,
  // Max request body size of 2 gb.
  maxRequestBodySize: 2147483648,
  fetch(req, server)
  {
    const url = new URL(req.url);

    // Upgrade to a websocket.
    const success = server.upgrade(req, 
      { 
        data: 
        { 
          pathname: url.pathname,
          params: url.searchParams 
        } 
      });
    return success? undefined : new Response("Upgrade failed", {status: 500});
  },

  websocket:
  {
    open(ws)
    {
      console.log("connection opened from ", ws.remoteAddress);

      console.log(ws.data.pathname)

      switch (ws.data.pathname)
      {
      case "/download":
        handleDownload(ws);
        break;

      case "/upload":
        handleUpload(ws);
        break;

      default:
        ws.close(1011, "unknown request");
        break;
      }

    },

    message(ws, message)
    {
      ws.data.handler(message);
    },
  }
});

console.log("Asset server started");


  // async fetch(req, server)
  // {
  //   console.log(`incoming request from ${server.requestIP(req)?.address}`)

  //   const url = new URL(req.url);
  //   const params = url.searchParams;

  //   if (url.pathname == "/download")
  //   {
  //     return await handleDownload(req, params);
  //   }
  //   else if (url.pathname == "/upload")
  //   {
  //     if (processing_upload)
  //       return new Response("an upload is already in progress");

  //     processing_upload = true;
  //     const response = await handleUpload(req, params); 
  //     processing_upload = false;
  //     return response;
  //   }
  //   else
  //   {
  //     return new Response("invalid url");
  //   }
  // }
