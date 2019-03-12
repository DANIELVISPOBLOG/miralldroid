#!/bin/bash
set -e

# build and test locally
BUILDDIR=build_release
rm -rf "$BUILDDIR"
meson "$BUILDDIR" --buildtype release --strip -Db_lto=true
cd "$BUILDDIR"
ninja
ninja test
cd -

# build Windows releases
make -f Makefile.CrossWindows

# the generated server must be the same everywhere
cmp "$BUILDDIR/server/miralldroid-server.jar" dist/miralldroid-win32/miralldroid-server.jar
cmp "$BUILDDIR/server/miralldroid-server.jar" dist/miralldroid-win64/miralldroid-server.jar

# get version name
TAG=$(git describe --tags --always)

# create release directory
mkdir -p "release-$TAG"
cp "$BUILDDIR/server/miralldroid-server.jar" "release-$TAG/miralldroid-server-$TAG.jar"
cp "dist/miralldroid-win32-$TAG.zip" "release-$TAG/"
cp "dist/miralldroid-win64-$TAG.zip" "release-$TAG/"

# generate checksums
cd "release-$TAG"
sha256sum "miralldroid-server-$TAG.jar" \
          "miralldroid-win32-$TAG.zip" \
          "miralldroid-win64-$TAG.zip" > SHA256SUMS.txt

echo "Release generated in release-$TAG/"
