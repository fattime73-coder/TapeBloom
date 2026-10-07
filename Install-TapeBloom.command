#!/bin/bash
cd "$(dirname "$0")"
bash scripts/install-mac.sh
result=$?
read -r -p 'Press Return to close.'
exit "$result"
