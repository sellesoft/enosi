const server = Bun.serve(
  {
    port: 3000,
    async fetch(req)
    {
      const url = new URL(req.url);

      const file = Bun.file(`.${url.pathname}`)

      if (!await file.exists())
        return new Response(`${url.pathname} does not exist`);

      return new Response(file);
    }
  });

console.log(`Listening on http://localhost:${server.port}...`)
