const util = await import("util");

const args = util.parseArgs(
{
  args: Bun.argv,
  allowPositionals: true,
  strict: true,
  options:
  {
    "pw-file":     { type: 'string' },
    "server-addr": { type: 'string' },
    "platform":    { type: 'string' },
    "upload-file": { type: 'string' },
    "upload-name": { type: 'string' },
  }
});

function getArg(argname: string)
{
  const arg = args.values[argname];
  if (null == arg)
    throw new Error(`missing arg --${argname}`);
  return arg
}

const pw_file_path = getArg("pw-file");
const server_addr = getArg("server-addr");
const platform = getArg("platform");
const upload_file_path = getArg("upload-file");
const upload_name = getArg("upload-name");

const pw_file = Bun.file(pw_file_path);
if (!await pw_file.exists())
  throw new Error(`password file ${pw_file_path} does not exist`);

const upload_file = Bun.file(upload_file_path);
if (!await upload_file.exists())
  throw new Error(`upload file ${upload_file_path} does not exist`);

const url = new URL(`http://${server_addr}`);

url.port = "3000";
url.pathname = "upload";
url.searchParams.set("pw", (await pw_file.text()).trim());
url.searchParams.set("platform", platform);
url.searchParams.set("name", upload_name);

const response = await fetch(url.toString(),
{
  method: "POST",
  body: upload_file,
});

response.text().then((text) => console.log(text));

