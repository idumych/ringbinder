#!/bin/sh
# build-hpkg.sh — builds ringbinder-<version>-<revision>-<arch>.hpkg on Haiku.
#
#   make hpkg            (or: sh packaging/haiku/build-hpkg.sh)
#
# Run it on Haiku R1/beta6 or later: the package requires the Haiku it was
# built on (>= r1~beta6), because the Haiku API is C++ and a binary built
# against newer libraries may not load on older ones.
set -eu

cd "$(dirname "$0")/../.."
ROOT=$(pwd)

VERSION=1.0.0
REVISION=1
ARCH=$(getarch)
PACKAGER="${PACKAGER:-idumych <19998719+idumych@users.noreply.github.com>}"
OUT="$ROOT/ringbinder-${VERSION}-${REVISION}-${ARCH}.hpkg"

for tool in rc xres mimeset package getarch; do
	command -v "$tool" >/dev/null 2>&1 || {
		echo "build-hpkg: '$tool' not found (needs Haiku with haiku_devel)" >&2
		exit 1; }
done

# 1. Build. Strip at link time: xres appends the resources after the ELF
#    data, so stripping afterwards would cut them off.
make clean >/dev/null
make LINKER_FLAGS=-s
BIN=$(ls objects.*-release/Ringbinder)
sh sync/tests/run_tests.sh >/dev/null
rm -f sync/tests/sync_test

# 2. Package tree. Inside the package, paths are relative to the
#    installation location (/boot/system).
PKG="$ROOT/obj/hpkg"
rm -rf "$PKG"
mkdir -p "$PKG/apps" \
	"$PKG/bin" \
	"$PKG/data/deskbar/menu/Applications" \
	"$PKG/documentation/packages/ringbinder"

cp "$BIN" "$PKG/apps/Ringbinder"
mimeset -f "$PKG/apps/Ringbinder"
ln -s ../apps/Ringbinder "$PKG/bin/ringbinder"
ln -s ../../../../apps/Ringbinder \
	"$PKG/data/deskbar/menu/Applications/Ringbinder"
cp README.md LICENSE "$PKG/documentation/packages/ringbinder/"

sed -e "s/@VERSION@/${VERSION}-${REVISION}/g" -e "s/@PVERSION@/${VERSION}/g" \
	-e "s/@ARCH@/${ARCH}/g" \
	-e "s/@PACKAGER@/${PACKAGER}/g" \
	packaging/haiku/PackageInfo.in > "$PKG/.PackageInfo"

# 3. Package
rm -f "$OUT"
package create -C "$PKG" "$OUT"
package list -i "$OUT" >/dev/null
echo "Built $OUT"
