param (
  [Parameter(Mandatory=$true)][string]$uploadpath,
  [Parameter(Mandatory=$true)][string]$uploadname
)

bin/client `
  upload `
  --pw-file upload_pw.txt `
  --server-addr 15.204.247.107 `
  --platform windows `
  --upload-file $uploadpath `
  --upload-name $uploadname

