const pw_file = Bun.file("upload_pw.txt");

if (!await pw_file.exists())
  throw new Error("failed to find upload_pw.txt!");

const url = new URL("http://15.204.247.107");

url.port = "3000";
url.pathname = "upload";
url.searchParams.set("pw", (await pw_file.text()).trim());
url.searchParams.set("platform", "linux");
url.searchParams.set("name", "package.zip")

const response = await fetch(url.toString(),
{
  method: "POST",
  body: Bun.file("package.zip"),
});

response.text().then((text) => console.log(text));

