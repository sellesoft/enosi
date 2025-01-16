#!/bin/bash

bin/client \
  upload \
  --pw-file upload_pw.txt \
  --server-addr 15.204.247.107 \
  --platform linux \
  --upload-file $1 \
  --upload-name $2 

