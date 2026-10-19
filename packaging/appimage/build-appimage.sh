#!/bin/sh
# build-appimage.sh — Ringbinder for any x86_64 Linux desktop, as an AppImage,
# using Cosmoe (https://github.com/haydentech/cosmoe), the Haiku API as a
# library on X11 and Wayland.
#
#   sh packaging/appimage/build-appimage.sh
#
# By default this installs the libraries and headers from Cosmoe's prebuilt
# release packages (.deb, extracted, not installed) into .cosmoe/stage-deb/,
# so only Ringbinder is compiled. Those packages are built for Ubuntu 26.04,
# so the AppImage needs that glibc (2.43) or newer. For an AppImage that runs
# on older distros, build Cosmoe from source on Ubuntu 24.04 instead:
#
#   COSMOE_FROM_SOURCE=1 sh packaging/appimage/build-appimage.sh
#
# which builds the pinned release into .cosmoe/stage/ (about 20 minutes).
# Either way, COSMOE_STAGE can point at an existing install of Cosmoe (a
# DESTDIR install made with --prefix=/usr --libdir=lib). Ringbinder is then
# built against it, bundled with linuxdeploy and packed with appimagetool.
#
# Needs: g++, make, curl, ar, tar, zstd and libcurl's development package
# (Ringbinder's Google sync uses it); plus Cosmoe's runtime libraries (cairo,
# pango, ICU, X11, Wayland, image codecs). With COSMOE_FROM_SOURCE=1 also git,
# meson, ninja, flex, bison, pkg-config and the matching development packages.
set -eu

cd "$(dirname "$0")/../.."
ROOT=$(pwd)

VERSION=1.3.0
COSMOE_TAG=v0.9.0_20261013
LINUXDEPLOY_TAG=1-alpha-20251107-1
APPIMAGETOOL_TAG=1.9.1
ARCH=x86_64

WORK="$ROOT/.cosmoe"
. /etc/os-release 2>/dev/null || :
if [ "${ID:-}" != ubuntu ]; then
	echo "Note: building on ${PRETTY_NAME:-this system}, not Ubuntu 24.04. The AppImage" >&2
	echo "will run here, but it needs this system's newer glibc and CPU baseline, so" >&2
	echo "build the one you release on Ubuntu 24.04 (CI)." >&2
fi
OUT="$ROOT/Ringbinder-${VERSION}-${ARCH}.AppImage"
mkdir -p "$WORK/tools"

# 1. Cosmoe
if [ -z "${COSMOE_STAGE:-}" ]; then
	if [ -n "${COSMOE_FROM_SOURCE:-}" ]; then
		COSMOE_STAGE="$WORK/stage"
		if [ ! -f "$COSMOE_STAGE/usr/lib/libbe.so" ]; then
			[ -d "$WORK/src" ] || git clone --depth 1 --branch "$COSMOE_TAG" \
				https://github.com/haydentech/cosmoe.git "$WORK/src"
			[ -f "$WORK/build/build.ninja" ] || meson setup "$WORK/build" "$WORK/src" \
				--prefix=/usr --libdir=lib --buildtype=release \
				-Denable_unit_test_compile=false
			ninja -C "$WORK/build"
			DESTDIR="$COSMOE_STAGE" meson install -C "$WORK/build" --no-rebuild
		fi
	else
		# Tag v0.9.0_20261013 is package version 0.9.0-1.20261013
		DEBVER="${COSMOE_TAG#v}"
		DEBVER="${DEBVER%_*}-1.${DEBVER#*_}"
		COSMOE_STAGE="$WORK/stage-deb"
		if [ ! -f "$COSMOE_STAGE/usr/lib/libbe.so" ]; then
			rm -rf "$COSMOE_STAGE" "$WORK/debs"
			mkdir -p "$COSMOE_STAGE" "$WORK/debs"
			for pkg in cosmoe cosmoe-dev; do
				deb="${pkg}_${DEBVER}_amd64.deb"
				curl -fsSL -o "$WORK/debs/$deb" \
					"https://github.com/haydentech/cosmoe/releases/download/$COSMOE_TAG/$deb"
				( cd "$WORK/debs" && rm -rf x && mkdir x && cd x && ar x "../$deb" \
					&& tar -xf data.tar.* -C "$COSMOE_STAGE" )
			done
			# The packages use Debian's multiarch lib directory; the Makefile
			# and this script expect Cosmoe's own layout.
			MULTIARCH="$COSMOE_STAGE/usr/lib/$(uname -m)-linux-gnu"
			cp -a "$MULTIARCH"/. "$COSMOE_STAGE/usr/lib/"
			rm -rf "$MULTIARCH"
		fi
	fi
fi
CROOT="$COSMOE_STAGE/usr"

# 2. Ringbinder. Strip at link time; the resources live in an allocated
#    .cosmoe_rsrc section, which stripping keeps.
make clean >/dev/null
make COSMOE_ROOT="$CROOT" LDFLAGS="${LDFLAGS:-} -s"
make test >/dev/null

# 3. AppDir
APPDIR="$ROOT/obj/AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" "$APPDIR/usr/share/cosmoe" \
	"$APPDIR/usr/share/fonts/noto" "$APPDIR/etc/fonts" \
	"$APPDIR/usr/share/doc/ringbinder"
install -m 0755 Ringbinder "$APPDIR/usr/bin/Ringbinder"
for lib in libbe.so libtracker.so libcosmoe-x11.so libcosmoe-wayland.so; do
	install -m 0755 "$CROOT/lib/$lib" "$APPDIR/usr/lib/$lib"
done
cp -a "$CROOT/lib/addons" "$APPDIR/usr/lib/"
for d in mime_db artwork locale; do
	cp -a "$CROOT/share/cosmoe/$d" "$APPDIR/usr/share/cosmoe/"
done
cp -a "$CROOT"/share/cosmoe/*.png "$APPDIR/usr/share/cosmoe/" 2>/dev/null || :
# Cosmoe's default fonts, in the styles Ringbinder uses
for f in NotoSans-Regular NotoSans-Bold NotoSans-Italic NotoSans-BoldItalic \
		NotoSansMono-Regular NotoSansMono-Bold; do
	find "$CROOT/share/fonts" -name "$f.ttf" -exec cp {} "$APPDIR/usr/share/fonts/noto/" \;
done
cp packaging/appimage/fonts.conf "$APPDIR/etc/fonts/fonts.conf"
cp LICENSE "$APPDIR/usr/share/doc/ringbinder/LICENSE"
cp "$WORK/src/License.md" "$APPDIR/usr/share/doc/ringbinder/COSMOE-LICENSE.md" 2>/dev/null \
	|| curl -fsSL "https://raw.githubusercontent.com/haydentech/cosmoe/$COSMOE_TAG/License.md" \
		-o "$APPDIR/usr/share/doc/ringbinder/COSMOE-LICENSE.md"
find "$APPDIR/usr/lib" -name '*.so' -exec strip --strip-unneeded {} \;

# Drop add-ons whose libraries this system doesn't have (the prebuilt
# Cosmoe packages are linked against Ubuntu 26.04's, e.g. libraw.so.23).
# Ringbinder uses none of them: no images, no translators.
for so in $(find "$APPDIR/usr/lib/addons" -name '*.so'); do
	missing=$(LD_LIBRARY_PATH="$APPDIR/usr/lib" ldd "$so" 2>/dev/null \
		| sed -n 's/^[[:space:]]*\(.*\) => not found.*/\1/p' | tr '\n' ' ')
	if [ -n "$missing" ]; then
		echo "Note: leaving out add-on $(basename "$so"): needs $missing" >&2
		rm -f "$so"
	fi
done

# 4. Bundle the libraries Cosmoe needs (ICU, pango, cairo, image codecs…),
#    leaving out the ones every desktop has (glibc, X11, fontconfig, GL…).
#    NO_STRIP: its bundled strip is older than some distros' libraries.
fetch() {
	[ -x "$WORK/tools/$1" ] || {
		curl -fsSL "$2" -o "$WORK/tools/$1"
		chmod +x "$WORK/tools/$1"
	}
}
fetch linuxdeploy "https://github.com/linuxdeploy/linuxdeploy/releases/download/$LINUXDEPLOY_TAG/linuxdeploy-$ARCH.AppImage"
fetch appimagetool "https://github.com/AppImage/appimagetool/releases/download/$APPIMAGETOOL_TAG/appimagetool-$ARCH.AppImage"
export APPIMAGE_EXTRACT_AND_RUN=1	# no FUSE needed (CI containers)

# linuxdeploy appends $ORIGIN to a library's RUNPATH but keeps what is
# there, and Cosmoe's install bakes in /usr/lib, which would make the
# AppImage prefer the host's ICU, pango and cairo over the bundled ones.
# Reset those first, with the patchelf that ships inside linuxdeploy.
LDROOT="$WORK/tools/linuxdeploy-root"
if [ ! -x "$LDROOT/AppRun" ]; then
	( cd "$WORK/tools" && rm -rf squashfs-root && ./linuxdeploy --appimage-extract >/dev/null \
		&& mv squashfs-root linuxdeploy-root )
fi
PATCHELF=$(find "$LDROOT" -name patchelf -type f | head -1)
for so in "$APPDIR"/usr/lib/*.so; do
	"$PATCHELF" --set-rpath '$ORIGIN' "$so"
done
for so in $(find "$APPDIR/usr/lib/addons" -name '*.so'); do
	"$PATCHELF" --set-rpath '$ORIGIN/../..' "$so"
done

DEPS_ONLY=""
for so in $(find "$APPDIR/usr/lib/addons" -name '*.so') \
		"$APPDIR/usr/lib/libcosmoe-x11.so" "$APPDIR/usr/lib/libcosmoe-wayland.so"; do
	DEPS_ONLY="$DEPS_ONLY --deploy-deps-only=$so"
done
# shellcheck disable=SC2086
NO_STRIP=1 LD_LIBRARY_PATH="$APPDIR/usr/lib" "$LDROOT/AppRun" --appdir "$APPDIR" \
	--executable "$APPDIR/usr/bin/Ringbinder" $DEPS_ONLY \
	--desktop-file packaging/appimage/ringbinder.desktop \
	--icon-file packaging/appimage/ringbinder.png \
	--custom-apprun packaging/appimage/AppRun

# 5. Pack
rm -f "$OUT"
ARCH=$ARCH "$WORK/tools/appimagetool" --no-appstream "$APPDIR" "$OUT"
echo "Built $OUT"
