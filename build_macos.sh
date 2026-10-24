#!/bin/bash
# Usage : ./build_macos.sh /chemin/vers/openfx [--install]
set -euo pipefail
SDK="${1:?Chemin du dépôt openfx requis}"
cmake -S . -B build -DOFX_SDK_DIR="$SDK" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
echo "Bundle : build/RetroSignalChain.ofx.bundle"
if [ "${2:-}" = "--install" ]; then
  sudo rm -rf /Library/OFX/Plugins/RetroSignalChain.ofx.bundle
  sudo mkdir -p /Library/OFX/Plugins
  sudo cp -R build/RetroSignalChain.ofx.bundle /Library/OFX/Plugins/
  echo "Installé. Redémarrez DaVinci Resolve : OpenFX > Retro Signal > Retro Signal Chain"
fi
