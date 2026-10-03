#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <kras_file>"
    exit 1
fi

for i in $(seq 1 10); do
    echo "--- run $i ---"
    python3 sendkras.py 192.168.5.2 "testdata/$1"
done