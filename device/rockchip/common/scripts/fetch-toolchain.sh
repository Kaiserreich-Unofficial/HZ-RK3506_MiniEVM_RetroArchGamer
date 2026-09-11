#!/bin/bash -e
# Fetch the prebuilt ARM cross toolchain that the RK3506 SDK expects under
# prebuilts/gcc/. The toolchain is not tracked in git (it is 516MB of binaries
# and is republished verbatim by ARM), so a fresh clone must download it.
#
# Usage: ./fetch-toolchain.sh [--force]
#
# Verifies the sha256 of the downloaded tarball, extracts it to a staging
# directory, sanity-checks the compiler, then installs it atomically.
# Safe to re-run: --force reinstalls, and a failed run leaves no partial tree.

RK_SCRIPTS_DIR="${RK_SCRIPTS_DIR:-$(dirname "$(readlink -f "$0")")}"
RK_SDK_DIR="${RK_SDK_DIR:-$RK_SCRIPTS_DIR/../../../..}"
PREBUILTS_DIR="$RK_SDK_DIR/prebuilts/gcc/linux-x86/arm"

TC_NAME="gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf"
TC_URL="https://developer.arm.com/-/media/Files/downloads/gnu-a/10.3-2021.07/binrel/$TC_NAME.tar.xz"
# sha256 of the official binrel tarball
TC_SHA256="aa074fa8371a4f73fecbd16bd62c8b1945f23289e26414794f130d6ccdf8e39c"

DL_DIR="${RK_SDK_DIR}/buildroot/dl/toolchain"
TARBALL="$DL_DIR/$TC_NAME.tar.xz"
DEST="$PREBUILTS_DIR/$TC_NAME"

FORCE=""
[ "$1" = "--force" ] && FORCE=1

# Progress goes to stderr: get_toolchain() captures this script's stdout when
# it calls us, so anything on stdout would corrupt the CROSS_COMPILE value.
say() { echo -e "\033[35m$*\033[0m" >&2; }
die() { echo -e "\033[31m$*\033[0m" >&2; exit 1; }

# Already complete? Then there is nothing to do.
if [ -z "$FORCE" ] && [ -x "$DEST/bin/arm-none-linux-gnueabihf-gcc" ]; then
	say "Toolchain already present: $DEST"
	exit 0
fi

mkdir -p "$DL_DIR"
mkdir -p "$PREBUILTS_DIR"

if [ ! -f "$TARBALL" ]; then
	say "Downloading cross toolchain (~99MB) ..."
	say "  $TC_URL"
	if command -v curl >/dev/null 2>&1; then
		curl -L --fail --retry 3 -o "$TARBALL.part" "$TC_URL" || \
			die "Download failed. Fetch it manually and place it at:
  $TARBALL"
	else
		wget -O "$TARBALL.part" "$TC_URL" || \
			die "Download failed. Fetch it manually and place it at:
  $TARBALL"
	fi
	mv "$TARBALL.part" "$TARBALL"
fi

say "Verifying tarball sha256 ..."
GOT="$(sha256sum "$TARBALL" | awk '{print $1}')"
if [ "$GOT" != "$TC_SHA256" ]; then
	die "Checksum mismatch for $TARBALL
  expected: $TC_SHA256
  got:      $GOT
Delete the file and re-run to download it again."
fi

# Extract to a staging dir so a failed unpack cannot leave a half-installed
# toolchain behind: get_toolchain() only checks for an executable gcc.
STAGE="$(mktemp -d "$PREBUILTS_DIR/.stage.XXXXXX")"
trap 'rm -rf "$STAGE"' EXIT

say "Extracting ..."
tar xf "$TARBALL" -C "$STAGE"

SRC="$STAGE/$TC_NAME"
[ -d "$SRC" ] || die "Unexpected tarball layout: $TC_NAME/ not found"

for t in arm-none-linux-gnueabihf-gcc arm-none-linux-gnueabihf-ld; do
	[ -x "$SRC/bin/$t" ] || die "Tarball is missing bin/$t"
done

# Preserve the dangling .git symlink the SDK tree carries, if a previous copy
# had one (harmless either way; the vendor ships it).
[ -L "$DEST/.git" ] && [ ! -e "$DEST/.git" ] && GITLINK=1

rm -rf "$DEST"
mv "$SRC" "$DEST"

if [ -n "$GITLINK" ]; then
	ln -sfn ../../../../.repo/projects/prebuilts/gcc/linux-x86/arm.git "$DEST/.git"
fi

chmod +x "$DEST/bin/"* 2>/dev/null || true

if [ -x "$DEST/bin/arm-none-linux-gnueabihf-gcc" ]; then
	say "Toolchain ready: $DEST"
	"$DEST/bin/arm-none-linux-gnueabihf-gcc" --version | head -1
else
	die "Toolchain install incomplete: $DEST/bin/arm-none-linux-gnueabihf-gcc missing"
fi
