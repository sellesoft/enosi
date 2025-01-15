#/bin/bash

./client \
  --pw-file ../upload_pw.txt \
  --server-addr localhost \
  --platform linux \
  --upload-file $1 \
  --upload-name $2

# 15.204.247.107 \
