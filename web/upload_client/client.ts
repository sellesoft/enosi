const pw_file = Bun.file("upload_pw.txt");

if (!await pw_file.exists())
  throw new Error("failed to find upload_pw.txt!");

fetch(`localhost:3000/upload?pw=${(await pw_file.text()).trim()}`).then(
  (response) =>
  {
    response.text().then((text) => console.log(text));
  })
