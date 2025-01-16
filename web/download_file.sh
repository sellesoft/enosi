#!/bin/bash

bin/client \
  download \
  --server-addr 15.204.247.107 \
  --platform linux \
  --download-name $1 \
  --download-dest $2
