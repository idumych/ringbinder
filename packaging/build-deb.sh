#!/bin/sh
# Builds ringbinder_<version>_<arch>.deb for VitruvianOS.
#
#   sh packaging/build-deb.sh            (from the project root)
#
# Needs VitruvianOS headers and libraries (vos-dev / pkg-config vos, or
# /system/develop/headers, or the link-time SDK from tools/vos-sdk), the rc,
# xres and resattr tools, libcurl's headers, tar with xattr support and
# dpkg-deb. Override the maintainer with DEBFULLNAME/DEBEMAIL.
set -eu

cd "$(dirname "$0")/.."
ROOT=$(pwd)

VERSION=1.1.1
REVISION=1
ARCH=$(dpkg --print-architecture)
MAINTAINER="${DEBFULLNAME:-Ringbinder maintainer} <${DEBEMAIL:-19998719+idumych@users.noreply.github.com}>"
# Built against VitruvianOS 0.6.2. The Haiku API is C++ and V\OS is pre-1.0,
# so class layouts may change in 0.7: require a rebuild then.
VOS_DEPENDS="vos (>= 0.6.2), vos (<< 0.7~)"

export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(date +%s)}"

BUILD="$ROOT/obj/deb"
PKG="$BUILD/ringbinder_${VERSION}-${REVISION}_${ARCH}"
OUT="$ROOT/ringbinder_${VERSION}-${REVISION}_${ARCH}.deb"

for tool in rc xres resattr dpkg-deb tar; do
	command -v "$tool" >/dev/null 2>&1 || {
		echo "build-deb: '$tool' not found in PATH" >&2; exit 1; }
done

# 1. Build. Strip at link time: xres appends the resources after the ELF
#    data, so stripping afterwards would cut them off.
make clean >/dev/null
make CXXFLAGS="${CXXFLAGS:--O2 -g -fstack-protector-strong -D_FORTIFY_SOURCE=2}" \
	LDFLAGS="${LDFLAGS:-} -s -Wl,--as-needed -Wl,-z,relro -Wl,-z,now"
make test >/dev/null
rm -f sync/tests/sync_test

# 2. Package tree
rm -rf "$BUILD"
mkdir -p "$PKG/DEBIAN" \
	"$PKG/system/apps" \
	"$PKG/system/data/deskbar/menu/Applications" \
	"$PKG/usr/bin" \
	"$PKG/usr/share/ringbinder" \
	"$PKG/usr/share/doc/ringbinder"

install -m 0755 Ringbinder "$PKG/system/apps/Ringbinder"
ln -s /system/apps/Ringbinder \
	"$PKG/system/data/deskbar/menu/Applications/Ringbinder"
ln -s /system/apps/Ringbinder "$PKG/usr/bin/ringbinder"

# 3. Attributes. dpkg drops xattrs, so ship them in a tarball on an empty
#    placeholder (as vos does) and let postinst copy them onto the binary.
STAGE="$BUILD/attrs"
mkdir -p "$STAGE/system/apps"
: > "$STAGE/system/apps/Ringbinder"
resattr -O -o "$STAGE/system/apps/Ringbinder" "$ROOT/obj/Ringbinder.rsrc"
# BEOS:TYPE as mimeset would set it: B_MIME_STRING_TYPE ('MIMS', stored
# little-endian) followed by the NUL-terminated value.
TYPE_HEX=534d494d$(printf '%s' application/x-vnd.Be-elfexecutable | od -A n -t x1 | tr -d ' \n')00
if command -v setfattr >/dev/null 2>&1; then
	setfattr -n user.beos.BEOS:TYPE -v "0x$TYPE_HEX" "$STAGE/system/apps/Ringbinder"
else
	python3 -c 'import os,sys; os.setxattr(sys.argv[1], "user.beos.BEOS:TYPE", bytes.fromhex(sys.argv[2]))' \
		"$STAGE/system/apps/Ringbinder" "$TYPE_HEX"
fi
tar --xattrs --xattrs-include='user.*' --owner=0 --group=0 --numeric-owner \
	--mtime="@$SOURCE_DATE_EPOCH" -cf "$PKG/usr/share/ringbinder/apps_attrs.tar" \
	-C "$STAGE" ./system/apps/Ringbinder

# 4. Docs
cp packaging/debian/copyright "$PKG/usr/share/doc/ringbinder/copyright"
gzip -9n -c packaging/debian/changelog > "$PKG/usr/share/doc/ringbinder/changelog.Debian.gz"
gzip -9n -c README.md > "$PKG/usr/share/doc/ringbinder/README.md.gz"

# 5. Control files
install -m 0755 packaging/debian/postinst "$PKG/DEBIAN/postinst"
SIZE=$(du -sk --apparent-size --exclude=DEBIAN "$PKG" | cut -f1)
cat > "$PKG/DEBIAN/control" <<EOF
Package: ringbinder
Version: ${VERSION}-${REVISION}
Architecture: ${ARCH}
Maintainer: ${MAINTAINER}
Installed-Size: ${SIZE}
Depends: ${VOS_DEPENDS}, libc6 (>= 2.38), libstdc++6 (>= 13), libgcc-s1 (>= 3.0), libcurl4t64 | libcurl4, attr
Section: utils
Priority: optional
Description: a tabbed personal organizer in a leather binder
 Ringbinder is a personal organizer modelled on the classic late-90s
 desktop organizers: a leather ring binder with coloured section tabs, a
 two-page week-at-a-glance diary and two-page spreads for To Do, Address,
 Calls, Notepad and Anniversary, and a year Planner. Everything is saved
 automatically.
 .
 Optional two-way sync with Google Calendar and Google Contacts, using your
 own Google OAuth client.
EOF
( cd "$PKG" && find . -type f ! -path './DEBIAN/*' -printf '%P\0' \
	| sort -z | xargs -0 md5sum ) > "$PKG/DEBIAN/md5sums"

find "$PKG" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +
chmod -R u+rwX,go+rX,go-w "$PKG"

dpkg-deb --root-owner-group -Zxz --build "$PKG" "$OUT"
echo "Built $OUT"
