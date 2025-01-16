const util = await import("util");
const fs = await import("node:fs/promises");

function writeProgress(current: number, total: number)
{
  process.stdout.clearLine(0);
  process.stdout.cursorTo(0);
  process.stdout.write(
    `${current} / ${total} ` +
    `(${Math.floor(current / total * 100)}%)`);
}

function openSocket(url: URL)
{
  console.log(`connecting to ${url.host}`);
  const socket = new WebSocket(url);

  if (socket.readyState == socket.CLOSED)
    throw new Error("failed to open socket!");

  return socket;
}

const args = util.parseArgs(
{
  args: Bun.argv.slice(2),
  allowPositionals: true,
  strict: true,
  options:
  {
    // General flags.
    "server-addr": { type: 'string' },
    "platform":    { type: 'string' },

    // Flags for uploading.
    "pw-file":     { type: 'string' },
    "upload-file": { type: 'string' },
    "upload-name": { type: 'string' },

    // Flags for downloading.
    "download-name": { type: 'string' },
    "download-dest": { type: 'string' },
  }
});

const action = args.positionals[0]

function getArg(argname: string)
{
  const arg = args.values[argname];
  if (null == arg)
    throw new Error(`missing arg --${argname}`);
  return arg
}

const server_addr = getArg("server-addr");
const platform = getArg("platform");

const url = new URL(`ws://${server_addr}`);
url.port = "3000";
url.searchParams.set("platform", platform);

const actions = 
{
  /* --------------------------------------------------------------------------
   */
  async upload()
  {
    const pw_file_path = getArg("pw-file");
    const upload_file_path = getArg("upload-file");
    const upload_name = getArg("upload-name");

    const pw_file = Bun.file(pw_file_path);
    if (!await pw_file.exists())
      throw new Error(`password file ${pw_file_path} does not exist`);

    const upload_file = Bun.file(upload_file_path);
    if (!await upload_file.exists())
      throw new Error(`upload file ${upload_file_path} does not exist`);

    url.pathname = "upload";
    url.searchParams.set("pw", (await pw_file.text()).trim());
    url.searchParams.set("name", upload_name);

    console.log("== upload ==");
    console.log(`${upload_file_path} -> ${platform}/${upload_name}\n`)

    const socket = openSocket(url);

    const stat = await fs.stat(upload_file_path);

    const file_reader = upload_file.stream().getReader();

    let bytes_sent = 0;

    socket.addEventListener("message", async (event) =>
    {
      switch (event.data as string)
      {
      case "size":
        socket.send(stat.size.toString());
        break;

      case "poll":
        const chunk = await file_reader.read();
        if (chunk.value == undefined)
        {
          socket.send("done");
          socket.close();
        }
        else
        {
          socket.send(chunk.value);
          bytes_sent += chunk.value.length;
          writeProgress(bytes_sent, stat.size);
        }
        break;
      }
    });
  },

  /* --------------------------------------------------------------------------
   */
  async download()
  {
    const download_name = getArg("download-name");
    const download_dest = getArg("download-dest");

    const dest_file = Bun.file(download_dest);

    url.pathname = "download";
    url.searchParams.set("name", download_name);

    console.log("== download ==");
    console.log(`${platform}/${download_name} -> ${download_dest}\n`);

    const socket = openSocket(url);

    const writer = dest_file.writer();

    let bytes_recieved = 0;
    let size: number | undefined = undefined;
    socket.addEventListener("message", async (event) =>
    {
      if (event.data == undefined)
      {
        console.log("abort: recieved undefined data");
        writer.end();
        socket.close();
        return;
      }

      if (event.data == "ready")
      {
        socket.send("poll");
        return;
      }

      if (size == null)
      {
        size = Number(event.data);
      }
      else if (event.data == "done")
      {
        console.log("\ndone");
        writer.end();
        socket.close();
      }
      else
      {
        writer.write(event.data);
        bytes_recieved += event.data.length;
        writeProgress(bytes_recieved, size);
      }

      socket.send("poll");
    });
  }
}

const actionFn = actions[action];
if (actionFn == null)
  throw new Error(`unknown action ${action}`);
actionFn();
