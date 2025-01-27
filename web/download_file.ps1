param (
  [Parameter(Mandatory=$true)][string]$downloadpath,
  [Parameter(Mandatory=$true)][string]$downloadname
)

bin/client `
  download `
  --server-addr 15.204.247.107 `
  --platform windows `
  --download-name $downloadname `
  --download-dest $downloadpath
