#!/bin/bash
# One-click build: produces ./firmware.img, ready to flash with the Rockchip
# SDDiskTool (TF card) or upgrade tool.
#
#   ./make-firmware.sh                       build with the saved config,
#                                            or the default board on a fresh clone
#   ./make-firmware.sh <board>_defconfig     apply that defconfig first
#   ./make-firmware.sh -h                    this help
#
# The build itself is ./build.sh; this wrapper handles the parts that trip
# people up: the PATH the SDK expects, picking a defconfig non-interactively on
# a fresh checkout, and surfacing the real error instead of a 4000-line log.

set -u

RK_SDK_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
cd "$RK_SDK_DIR" || exit 1

DEFAULT_DEFCONFIG="HZ-RK3506G2_MiniEVM_TF_defconfig"
OUT_IMG="$RK_SDK_DIR/output/update/Image/update.img"
FIRMWARE_IMG="$RK_SDK_DIR/firmware.img"
LOG="$RK_SDK_DIR/output/build-firmware.log"

# The SDK scripts misbehave when PATH carries anything unusual.
export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

say()  { printf '\033[35m%s\033[0m\n' "$*"; }
ok()   { printf '\033[32m%s\033[0m\n' "$*"; }
fail() { printf '\033[31m%s\033[0m\n' "$*" >&2; exit 1; }

usage() {
	cat <<'EOF'
One-click build: produces ./firmware.img, ready to flash with the Rockchip
SDDiskTool (TF card) or the upgrade tool.

  ./make-firmware.sh                       build with the saved config,
                                           or the default board on a fresh clone
  ./make-firmware.sh <board>_defconfig     apply that defconfig first
  ./make-firmware.sh -h                    this help

The build itself is ./build.sh; this wrapper handles the parts that trip
people up: the PATH the SDK expects, picking a defconfig non-interactively on
a fresh checkout, and surfacing the real error instead of a huge log.
EOF
	exit 0
}

DEFCONFIG=""
case "${1:-}" in
	-h|--help) usage ;;
	"") ;;
	-*) fail "Unknown option: $1 (try -h)" ;;
	*)  DEFCONFIG="$1" ;;
esac

[ -x "$RK_SDK_DIR/build.sh" ] || fail "build.sh not found in $RK_SDK_DIR"

# On a fresh clone there is no saved config, so a bare ./build.sh would stop at
# an interactive prompt. Select a board explicitly in that case.
if [ -z "$DEFCONFIG" ] && [ ! -f "$RK_SDK_DIR/output/.config" ]; then
	DEFCONFIG="$DEFAULT_DEFCONFIG"
	say "Fresh checkout: selecting $DEFCONFIG"
fi

if [ -n "$DEFCONFIG" ] && [ ! -f "$RK_SDK_DIR/device/rockchip/.chips/rk3506/$DEFCONFIG" ]; then
	fail "No such defconfig: $DEFCONFIG
Available boards:
$(cd "$RK_SDK_DIR/device/rockchip/.chips/rk3506" && ls ./*_defconfig 2>/dev/null | sed 's|^\./|  |')"
fi

mkdir -p "$RK_SDK_DIR/output"

# Run the SDK build. Passing a defconfig does NOT build: build.sh treats a
# *_defconfig argument as "select this board" and exits after writing
# output/.config, so a real build has to follow. Do both into one log.
# The build also probes sources.buildroot.net and aborts if unreachable;
# behind a proxy that check misfires, so the caller retries it once.
run_build() {
	{
		if [ -n "$DEFCONFIG" ]; then
			./build.sh "$DEFCONFIG" || return 1
			echo
			say "Config selected; starting the build proper ..."
		fi
		./build.sh || return 1
	} >> "$LOG" 2>&1
}

: > "$LOG"
BUILD_START=$(date +%s)

# build.sh can exit 0 even when a build hook aborted (its error trap does not
# always propagate), and a stale output/update/Image/update.img would then be
# copied as if it were new. Judge success by the log and image freshness.
build_ok() {
	! grep -q "^ERROR" "$LOG" && [ -s "$OUT_IMG" ] &&
		[ "$(stat -c%Y "$OUT_IMG")" -ge "$BUILD_START" ]
}

say "=========================================="
say " Building firmware ($(date +%H:%M:%S))"
say "=========================================="
if [ -n "$DEFCONFIG" ]; then
	say "Board: $DEFCONFIG"
fi
say "Log:   $LOG"
echo

if ! run_build && ! build_ok; then
	if grep -q "network is not able to access" "$LOG"; then
		say "Flaky sources.buildroot.net check tripped; retrying ..."
		: > "$LOG"
		BUILD_START=$(date +%s)
		run_build || true
	fi
fi
build_ok || {
	tail -25 "$LOG"
	fail "Build failed (full log: $LOG)"
}

say "Copying to firmware.img ..."
cp -f "$OUT_IMG" "$FIRMWARE_IMG.part" || fail "Could not write $FIRMWARE_IMG"
mv -f "$FIRMWARE_IMG.part" "$FIRMWARE_IMG"

SIZE="$(stat -c%s "$FIRMWARE_IMG")"
[ "$SIZE" -gt 10000000 ] || fail "firmware.img looks too small ($SIZE bytes)"

echo
ok "Firmware ready: $FIRMWARE_IMG"
printf '  size    %s bytes (%.0f MB)\n' "$SIZE" "$(echo "$SIZE/1048576" | bc -l)"
printf '  sha256  %s\n' "$(sha256sum "$FIRMWARE_IMG" | awk '{print $1}')"
echo
cat <<'EOF'
Flash it with the Rockchip SDDiskTool (TF card) or the upgrade tool:
  1. open SDDiskTool, pick "Firmware" mode
  2. select firmware.img
  3. choose the TF card and click Create
EOF
