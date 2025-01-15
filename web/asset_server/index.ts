const fs = await import('node:fs/promises');

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
async function handleDownload(req: Request, params: URLSearchParams)
{
  console.log("download request");

  const platform = params.get("platform");
  if (platform == null)
    return errResponse("no platform specified");

  const src_dir = `assets/${platform}`;
  if (!await pathExists(src_dir))
    return errResponse("unknown platform");

  const name = params.get("name");
  if (name == null)
    return errResponse("no file specified");

  const src_path = `${src_dir}/${name}`;
  if (!await pathExists(src_path))
    return errResponse(`no file ${name} on server under ${platform}`);

  return new Response(Bun.file(src_path));
}

/* ----------------------------------------------------------------------------
 */
let processing_upload = false;
async function handleUpload(req: Request, params: URLSearchParams)
{
  console.log("upload request");

  const pw = params.get("pw");
  if (pw == null)
    return errResponse("password not provided");

  if (!await Bun.password.verify(pw, pw_hash))
    return errResponse("incorrect password");

  const platform = params.get("platform");
  if (platform == null)
    return errResponse("no platform specified");

  const dest_dir = `assets/${platform}`;
  if (!await pathExists(dest_dir))
    return errResponse("unknown platform");

  const name = params.get("name");
  if (name == null)
    return errResponse("no name provided");

  const dest_path = `${dest_dir}/${name}`;

  console.log(`upload ${dest_path}`);

  await Bun.write(dest_path, new Response(req.body));

  return new Response(`uploaded ${dest_path}`);
}

/* ----------------------------------------------------------------------------
 */
const server = Bun.serve(
{
  // Prevent showing source code when an error occurs.
  development: false,
  port: 3000,
  async fetch(req, server)
  {
    console.log(`incoming request from ${server.requestIP(req)?.address}`)

    const url = new URL(req.url);
    const params = url.searchParams;

    if (url.pathname == "/download")
    {
      return await handleDownload(req, params);
    }
    else if (url.pathname == "/upload")
    {
      if (processing_upload)
        return new Response("an upload is already in progress");

      processing_upload = true;
      const response = await handleUpload(req, params); 
      processing_upload = false;
      return response;
    }
    else
    {
      return new Response("invalid url");
    }
  }
});

console.log("Asset server started");
