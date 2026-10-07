#!/bin/bash
cd "$(dirname "$0")"
bash scripts/build-mac.sh
result=$?
echo "Build exit code: $result"
read -r -p 'Press Return to close.'
exit "$result"
