const fs = await import('node:fs/promises');

import * as bun from 'bun';

/* ----------------------------------------------------------------------------
 */
async function pathExists(path: Bun.PathLike)
{
  return await fs.stat(path.toString()).catch(() => false);
}

/* ----------------------------------------------------------------------------
 */
if (!await pathExists("upload_pw.txt"))
  throw new Error("failed to find upload_pw.txt!");

const pw_file = Bun.file("upload_pw.txt");
const pw_hash = await Bun.password.hash((await pw_file.text()).trim());

/* ----------------------------------------------------------------------------
 */
function writeProgress(current: number, total: number, time_spent: number)
{
  process.stdout.clearLine(0);
  process.stdout.cursorTo(0);
  process.stdout.write(
    `${current} / ${total} ` +
    `(${Math.floor(current / total * 100)}%) `);

  const mb_sent = current / 1e6;
  const mbps = mb_sent / (time_spent / 1000);

  process.stdout.write(`${mbps.toFixed(2)} mb/s`);
}

/* ----------------------------------------------------------------------------
 */
type WebSocketData = 
{
  pathname: string,
  params: URLSearchParams,
  handler: Function,
}

/* ----------------------------------------------------------------------------
 */
async function handleDownload(ws: bun.ServerWebSocket<WebSocketData>)
{
  console.log("== download ==");

  const err = (message: string) => ws.close(1011, message);

  const params = ws.data.params;

  const platform = params.get("platform");
  if (platform == null)
    return err("no platform specified");

  const src_dir = `assets/${platform}`;
  if (!await pathExists(src_dir))
    return err("unknown platform");

  const name = params.get("name");
  if (name == null)
    return err("no file specified");

  const src_path = `${src_dir}/${name}`;
  if (!await pathExists(src_path))
    return err(`no file ${name} under ${platform}`);

  const file = Bun.file(src_path);

  const stat = await fs.stat(src_path);

  const reader = file.stream().getReader();

  const sendSize = () =>
  {
    ws.send(stat.size.toString());
    ws.data.handler = sendChunks;
  }

  const start_time = Date.now();

  const interval_id = setInterval(() =>
  {
    writeProgress(bytes_sent, stat.size, Date.now() - start_time);
  }, 50);

  let bytes_sent = 0;
  const sendChunks = async () =>
  {
    const chunk = await reader.read();

    if (chunk.value == undefined)
    {
      ws.send("done");
      clearInterval(interval_id);
    }
    else
    {
      ws.send(chunk.value);
      bytes_sent += chunk.value.length;
    }
  }

  ws.data.handler = sendSize;
  ws.send("ready");
}

/* ----------------------------------------------------------------------------
 */
async function handleUpload(ws: bun.ServerWebSocket<WebSocketData>)
{
  console.log("== upload ==");

  let size: number | undefined = undefined;
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

  console.log(dest_path);

  const dest_file = Bun.file(dest_path);

  const writer = dest_file.writer();

  const start_time = Date.now();

  const interval_id = setInterval(() =>
  {
    if (size != null)
      writeProgress(bytes_recieved, size, Date.now() - start_time);
  }, 50);

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
      clearInterval(interval_id);
    }
    else
    {
      writer.write(message);
      bytes_recieved += (message as Buffer).length;
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

    console.log(req)

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
