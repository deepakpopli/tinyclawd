#!/bin/sh
# Small three.js build with only the parts site/js/clawd3d.js uses.
#   sh site/tools/build_vendor.sh   -> site/vendor/three.js
# needs node + npm, only when you change which THREE.* names the script uses
set -e
VERSION=0.169.0
site=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"
npm pack -s three@$VERSION >/dev/null
mkdir -p node_modules/three
tar xzf three-$VERSION.tgz -C node_modules/three --strip-components=1

names=$(grep -o 'THREE\.[A-Za-z0-9]*' "$site/js/clawd3d.js" | sed 's/THREE\.//' | grep -v RoomEnvironment | sort -u | paste -sd, - | sed 's/,/, /g')
cat > entry.js <<EOF
export { $names } from 'three';
export { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
EOF

npx --yes esbuild@0.24.0 entry.js --bundle --format=esm --minify --legal-comments=none --outfile=out.js
{
  echo "// three.js $VERSION (MIT license, see three.LICENSE), trimmed for this site by tools/build_vendor.sh"
  cat out.js
} > "$site/vendor/three.js"
cp node_modules/three/LICENSE "$site/vendor/three.LICENSE"
ls -l "$site/vendor/three.js"
