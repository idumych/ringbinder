#!/bin/sh
# make-sdk.sh — builds a link-time SDK for VitruvianOS 0.6.2 on an ordinary
# x86_64 Linux host (Debian/Ubuntu), so Ringbinder can be compiled, linked and
# packaged as a .deb without a VitruvianOS machine.
#
#   sh tools/vos-sdk/make-sdk.sh [WORK_DIR]       (default: ./.vos-sdk)
#
# Result in WORK_DIR/sdk:
#   include/vos/…   headers, laid out like the vos-dev package
#   lib/            libbe, libroot, libtracker, libtranslation, libtextencoding
#                   built from the real 0.6.2 sources, plus vos.pc
#   bin/            rc, xres, resattr (VitruvianOS resource tools)
#   linkdeps/       libraries only needed so the linker can check the above
#                   (ICU, placeholder libgame/libmedia2, -dev symlinks)
# Then:
#   . WORK_DIR/sdk/env.sh && make deb
#
# None of these libraries go into the package: Folio uses the ones installed
# on VitruvianOS. They exist so the link proves every symbol Folio needs is
# exported by 0.6.2, with the right library names (libbe.so etc., no
# symbol versioning, so runtime binding is by name).
#
# Needs: git, cmake >= 3.25, ninja, g++, gcc, bison, m4, make, pkg-config.
# Everything else is fetched from GitHub when the host lacks it, because the
# environment this was written in could reach GitHub but not apt mirrors.
set -eu

VOS_TAG=0.6.2
ICU_TAG=release-76-1          # same ICU major as Debian trixie (libicu76)
XKB_TAG=xkbcommon-1.6.0
SYSTEMD_TAG=v255
FLEX_TAG=v2.6.4

HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mkdir -p "${1:-.vos-sdk}" && cd "${1:-.vos-sdk}" && pwd)
SRC=$WORK/src
DEPS=$WORK/deps                      # prefix for fetched build deps
SDK=$WORK/sdk
JOBS=$(nproc 2>/dev/null || echo 4)
mkdir -p "$SRC" "$DEPS/bin" "$DEPS/lib" "$DEPS/include"

say() { printf '\n== %s\n' "$*"; }
have() { command -v "$1" >/dev/null 2>&1; }
clone() {  # clone URL TAG DIR [sparse paths…]
	url=$1 tag=$2 dir=$3; shift 3
	[ -d "$dir/.git" ] && return 0
	if [ $# -gt 0 ]; then
		GIT_LFS_SKIP_SMUDGE=1 git clone -q --depth 1 --filter=blob:none \
			--sparse --branch "$tag" "$url" "$dir"
		git -C "$dir" sparse-checkout set "$@"
	else
		GIT_LFS_SKIP_SMUDGE=1 git clone -q --depth 1 --branch "$tag" "$url" "$dir"
	fi
}
# devlink NAME SONAME: make lib<NAME>.so linkable from the runtime library
devlink() {
	[ -e "$DEPS/lib/lib$1.so" ] && return 0
	for d in /usr/lib/x86_64-linux-gnu /lib/x86_64-linux-gnu /usr/lib64 /usr/lib; do
		if [ -e "$d/lib$1.so" ]; then return 0; fi
		if [ -e "$d/$2" ]; then ln -sf "$d/$2" "$DEPS/lib/lib$1.so"; return 0; fi
	done
	echo "make-sdk: lib$1 ($2) not found on this host" >&2; return 1
}

for t in git cmake ninja g++ gcc bison m4 make pkg-config; do
	have "$t" || { echo "make-sdk: missing tool: $t" >&2; exit 1; }
done

# ---------------------------------------------------------------- sources
say "VitruvianOS $VOS_TAG"
clone https://github.com/VitruvianOS/Vitruvian.git "$VOS_TAG" "$SRC/vos"
git -C "$SRC/vos" submodule update --init --depth 1 src/system/kernel/nexus
if ! git -C "$SRC/vos" diff --quiet; then
	echo "(link-only patch already applied)"
else
	git -C "$SRC/vos" apply "$HERE/vos-$VOS_TAG-linkonly.patch"
fi

# ---------------------------------------------------------------- flex
if have flex; then
	FLEX=$(command -v flex)
else
	say "flex (bootstrapped from flex $FLEX_TAG + winflexbison's generated scanner)"
	clone https://github.com/westes/flex.git "$FLEX_TAG" "$SRC/flex"
	clone https://github.com/lexxmark/winflexbison.git master "$SRC/winflexbison"
	B=$SRC/flex-build; rm -rf "$B"; cp -r "$SRC/flex/src" "$B"
	# scan.l is identical in both; winflexbison commits the generated scan.c
	cmp -s "$SRC/flex/src/scan.l" "$SRC/winflexbison/flex/src/scan.l" || {
		echo "make-sdk: winflexbison's scan.l no longer matches flex $FLEX_TAG;" \
			"install flex instead" >&2; exit 1; }
	sed -e 's|^#include <io.h>|#include <unistd.h>|' \
		-e 's|_isatty( _fileno(file) )|isatty( fileno(file) )|' \
		"$SRC/winflexbison/flex/src/scan.c" > "$B/scan.c"
	( cd "$B"
	  sh mkskel.sh . m4 2.6.4 > skel.c
	  bison -d -o parse.c parse.y
	  cat > config.h <<'EOF'
#define VERSION "2.6.4"
#define PACKAGE "flex"
#define PACKAGE_VERSION "2.6.4"
#define M4 "/usr/bin/m4"
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_REGEX_H 1
#define HAVE_STDBOOL_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_FORK 1
#define HAVE_DUP2 1
#define HAVE_SETLOCALE 1
#define HAVE_STRCASECMP 1
#define HAVE_STRCHR 1
#define HAVE_STRDUP 1
#define HAVE_STRTOL 1
#define HAVE_REALLOC 1
#define HAVE_REALLOCARRAY 1
#define HAVE_POW 1
#define STDC_HEADERS 1
#define ENABLE_NLS 0
EOF
	  gcc -O2 -DHAVE_CONFIG_H -I. -w ccl.c dfa.c ecs.c scanflags.c gen.c \
		main.c misc.c nfa.c parse.c scan.c skel.c sym.c tblcmp.c yylex.c \
		options.c scanopt.c buf.c tables.c tables_shared.c filter.c regex.c \
		-lm -o flex
	  gcc -O2 -DHAVE_CONFIG_H -I. -w -c libmain.c libyywrap.c
	  ar rcs libfl.a libmain.o libyywrap.o
	  cp flex "$DEPS/bin/"; cp libfl.a "$DEPS/lib/"; cp FlexLexer.h "$DEPS/include/" )
	FLEX=$DEPS/bin/flex
fi
FLEXLIB=$(ls "$DEPS/lib/libfl.a" /usr/lib/x86_64-linux-gnu/libfl.a 2>/dev/null | head -1 || true)
FLEX_OPTS="-DFLEX_EXECUTABLE=$FLEX"
[ -n "$FLEXLIB" ] && FLEX_OPTS="$FLEX_OPTS -DFL_LIBRARY=$FLEXLIB -DFLEX_INCLUDE_DIR=$DEPS/include"
# vos maps only the x86_64-linux-gnu triple to an interpreter; Arch-style
# hosts report x86_64-pc-linux-gnu, so name it explicitly.
FLEX_OPTS="$FLEX_OPTS -DVOS_DYNAMIC_LINKER=/lib64/ld-linux-x86-64.so.2"

# ---------------------------------------------------------------- buildtools
say "VitruvianOS buildtools (rc, xres, resattr)"
if [ ! -x "$SRC/vos/buildtools/src/bin/xres" ]; then
	mkdir -p "$SRC/vos/buildtools"
	( cd "$SRC/vos/buildtools"
	  PATH=$DEPS/bin:$PATH cmake -DBUILDTOOLS_MODE=1 $FLEX_OPTS .. -GNinja >/dev/null
	  PATH=$DEPS/bin:$PATH ninja -j"$JOBS" >/dev/null )
fi

# ---------------------------------------------------------------- ICU
ICU_OPTS=""
if ! pkg-config --exists icu-uc icu-i18n icu-io 2>/dev/null; then
	say "ICU ($ICU_TAG, libraries + stub data; link-time only)"
	clone https://github.com/unicode-org/icu.git "$ICU_TAG" "$SRC/icu" icu4c
	if [ ! -e "$DEPS/lib/libicuuc.so" ]; then
		B=$SRC/icu-build; rm -rf "$B"; mkdir -p "$B/lib" "$B/bin"
		( cd "$B"
		  "$SRC/icu/icu4c/source/configure" --prefix="$DEPS" --disable-tests \
			--disable-samples --disable-extras --disable-layoutex \
			--disable-tools >/dev/null
		  for d in stubdata common i18n io; do make -j"$JOBS" -C $d >/dev/null; done )
		cp -a "$B"/lib/* "$B"/stubdata/libicudata.so* "$DEPS/lib/"
		mkdir -p "$DEPS/include/unicode"
		cp "$SRC"/icu/icu4c/source/common/unicode/*.h \
		   "$SRC"/icu/icu4c/source/i18n/unicode/*.h \
		   "$SRC"/icu/icu4c/source/io/unicode/*.h "$DEPS/include/unicode/"
	fi
	ICU_OPTS="-DICU_ROOT=$DEPS"
fi

# ---------------------------------------------------------------- headers
if [ ! -e /usr/include/xkbcommon/xkbcommon.h ]; then
	say "xkbcommon headers ($XKB_TAG)"
	clone https://github.com/xkbcommon/libxkbcommon.git "$XKB_TAG" "$SRC/xkb"
	mkdir -p "$DEPS/include/xkbcommon"
	cp "$SRC"/xkb/include/xkbcommon/*.h "$DEPS/include/xkbcommon/"
fi
if [ ! -e /usr/include/libudev.h ] || [ ! -e /usr/include/systemd/sd-bus.h ]; then
	say "systemd/udev headers ($SYSTEMD_TAG)"
	clone https://github.com/systemd/systemd.git "$SYSTEMD_TAG" "$SRC/systemd" \
		src/systemd src/libudev
	mkdir -p "$DEPS/include/systemd"
	cp "$SRC"/systemd/src/systemd/sd-*.h "$SRC"/systemd/src/systemd/_sd-common.h \
		"$DEPS/include/systemd/"
	cp "$SRC/systemd/src/libudev/libudev.h" "$DEPS/include/"
fi
devlink udev libudev.so.1
devlink systemd libsystemd.so.0
devlink xkbcommon libxkbcommon.so.0
devlink blkid libblkid.so.1

# libtracker links libgame (game kit) and libgame links libmedia2, which
# needs PipeWire + GStreamer. Placeholders with the real names stand in so
# libtracker links; the one symbol libtracker takes from libgame is defined.
if [ ! -e "$DEPS/lib/libmedia2.so" ]; then
	echo '' > "$WORK/empty.c"
	gcc -shared -fPIC -Wl,-soname,libmedia2.so "$WORK/empty.c" -o "$DEPS/lib/libmedia2.so"
	printf '#include <stdint.h>\nvoid set_mouse_position(int32_t, int32_t) {}\n' > "$WORK/game.cpp"
	g++ -shared -fPIC -Wl,-soname,libgame.so "$WORK/game.cpp" -o "$DEPS/lib/libgame.so"
fi

# ---------------------------------------------------------------- libraries
say "libroot, libbe, libtranslation, libtracker (VitruvianOS $VOS_TAG)"
G=$SRC/vos/generated
mkdir -p "$G"
( cd "$G"
  PATH=$DEPS/bin:$PATH cmake -DCMAKE_BUILD_TYPE=Release \
	-DBUILDTOOLS_DIR=../buildtools -DHOST_KERNEL="$(uname -r)" \
	-DKERNEL_RELEASE="$(uname -r)" -DVITRUVIAN_TARGET_ARCH=amd64 \
	$FLEX_OPTS $ICU_OPTS \
	-DCMAKE_C_FLAGS="-I$DEPS/include" -DCMAKE_CXX_FLAGS="-I$DEPS/include" \
	-DCMAKE_SHARED_LINKER_FLAGS="-L$DEPS/lib" \
	.. -GNinja >/dev/null
  PATH=$DEPS/bin:$PATH LIBRARY_PATH=$DEPS/lib ninja -j"$JOBS" root be translation tracker )

# ---------------------------------------------------------------- SDK
say "SDK in $SDK"
rm -rf "$SDK"; mkdir -p "$SDK/include/vos" "$SDK/lib" "$SDK/bin" "$SDK/linkdeps"
( cd "$SRC/vos/headers" && tar cf - --exclude=./private --exclude=./libs \
	--exclude=./tools . ) | tar xf - -C "$SDK/include/vos"
cp "$G/src/system/libroot.so" "$G/src/kits/libbe.so" \
   "$G/src/kits/tracker/libtracker.so" "$G/src/kits/translation/libtranslation.so" \
   "$G/src/kits/textencoding/libtextencoding.so" "$SDK/lib/"
cp -a "$DEPS"/lib/*.so* "$SDK/linkdeps/"
B=$SRC/vos/buildtools/src/bin
cp "$B/rc/rc" "$B/xres" "$B/resattr" "$SDK/bin/"

# vos.pc the way newer VitruvianOS (vos-dev) generates it
python3 - "$SRC/vos" "$SDK" <<'EOF'
import re, sys
vos, sdk = sys.argv[1:]
s = open(vos + '/build/headers.cmake').read()
body = s[s.index('PUBLIC_HEADERS'):]
body = body[:body.index(')')]
flags = []
for d in re.findall(r'"([^"]+)"', body):
    d = d.rstrip('/')
    f = '-I${includedir}' if d == 'headers' else '-I${includedir}/' + d[len('headers/'):]
    if f not in flags:
        flags.append(f)
flags += ['-I${includedir}/build', '-I${includedir}/build/config_headers',
          '-I${includedir}/config', '-include LinuxBuildCompatibility.h']
open(sdk + '/lib/vos.pc', 'w').write(f"""prefix={sdk}
includedir=${{prefix}}/include/vos
libdir=${{prefix}}/lib

Name: vos
Description: VitruvianOS 0.6.2 link-time SDK (built by Folio's make-sdk.sh)
Version: 0.6.2
Libs: -L${{libdir}} -lbe -lroot
Cflags: {' '.join(flags)}
""")
EOF

cat > "$SDK/env.sh" <<EOF
# source me: . $SDK/env.sh
export PATH="$SDK/bin:\$PATH"
export PKG_CONFIG_PATH="$SDK/lib"
export LDFLAGS="-Wl,-rpath-link=$SDK/lib:$SDK/linkdeps"
EOF

say "done"
echo "Use it with:   . $SDK/env.sh && make deb"
