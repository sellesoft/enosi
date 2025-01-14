const pw_file = Bun.file("upload_pw.txt");

if (!await pw_file.exists())
  throw new Error("failed to find upload_pw.txt!");

const pw_hash = await Bun.password.hash((await pw_file.text()).trim());

console.log(pw_hash);

const server = Bun.serve(
{
  // Prevent showing source code when an error occurs.
  development: false,
  port: 3000,
  async fetch(req)
  {
    const url = new URL(req.url);
    const params = url.searchParams;

    if (url.pathname == "/download")
    {
      const platform = params.get("platform");
    }
    else if (url.pathname == "/upload")
    {
      const pw = params.get("pw");

      if (pw == null)
        return new Response("password not provided");

      console.log(await Bun.password.hash(pw));

      if (!await Bun.password.verify(pw, pw_hash))
        return new Response("incorrect password");

      return new Response("good job");
    }
    else
      return new Response("invalid url");

    console.log(url);

    return new Response("hi");
  }
});

console.log(`Listening on http://localhost:${server.port}...`)
