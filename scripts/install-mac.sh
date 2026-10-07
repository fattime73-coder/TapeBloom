#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin ]]; then echo 'macOS required.'; exit 1; fi
artifacts="build/TapeBloom_artefacts/Release"
[[ -d "$artifacts/Standalone/TapeBloom.app" && -d "$artifacts/AU/TapeBloom.component" ]] || { echo 'Build first: bash scripts/build-mac.sh'; exit 1; }
mkdir -p "$HOME/Applications" "$HOME/Library/Audio/Plug-Ins/Components"
stamp="$(date +%Y%m%d-%H%M%S)"
for target in "$HOME/Applications/TapeBloom.app" "$HOME/Library/Audio/Plug-Ins/Components/TapeBloom.component"; do
  if [[ -e "$target" ]]; then mv "$target" "$target.backup-$stamp"; fi
done
ditto "$artifacts/Standalone/TapeBloom.app" "$HOME/Applications/TapeBloom.app"
ditto "$artifacts/AU/TapeBloom.component" "$HOME/Library/Audio/Plug-Ins/Components/TapeBloom.component"
codesign --force --deep --sign - "$HOME/Applications/TapeBloom.app"
codesign --force --deep --sign - "$HOME/Library/Audio/Plug-Ins/Components/TapeBloom.component"
echo 'Installed for the current user. Restart Logic Pro and run: auval -v aumf TpBl TbAu'
open "$HOME/Applications"
