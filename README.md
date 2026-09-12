# HZ-RK3506 MiniEVM RetroArch Gamer

基于合众跃恒 HZ-RK3506 开发板的复古游戏机，采用 Retro-Zero（Libretro 前端）固件。

A retro gaming console built on the **HZ-RK3506G2 MiniEVM** board (Rockchip
RK3506G2, dual Cortex-A7, no GPU), running Buildroot with
[Retro-Zero](https://github.com/geo-tp/Retro-Zero) as the Libretro front-end.

This repository is a single-tree snapshot of the vendor Rockchip Linux 6.1 SDK
with the board customizations that turn it into a handheld. It is **not** an
upstream Rockchip repository and **not** a `repo`-tool checkout — see
[Provenance](#provenance) for why.

## Hardware

| | |
|---|---|
| Board | HZ-RK3506G2 MiniEVM (RK3506G2, 2x Cortex-A7, no GPU) |
| Display | ILI9341 2.8" 320x240 SPI panel, driven by `tinydrm` |
| Audio | USB sound card (USB Audio Class) |
| Input | USB OTG: CDC-ACM serial console + USB HID joysticks |
| Ethernet | gmac0 + RMII PHY, fixed address `192.168.10.1/24` (no Wi-Fi on board) |
| Storage | TF card, GPT: `oem` / `userdata` / `rootfs` / `data` |
| Firmware | Retro-Zero front-end (LVGL UI over `/dev/fb0`) + libretro cores |

The debug console runs over a USB gadget at `/dev/ttyGS0`; the host sees it as
a serial (COM) port. Retro-Zero starts into its ROM browser, and is supervised
so that quitting or crashing returns to the browser rather than a blank
framebuffer.

## What was customized

Only a handful of files differ from the vendor SDK.

| Path | Purpose |
|---|---|
| `device/rockchip/.chips/rk3506/HZ-RK3506G2_MiniEVM_TF_defconfig` | Board defconfig: selects DTS, kernel fragments, rootfs overlay |
| `device/rockchip/.chips/rk3506/parameter-EMMC.txt` | Partition table + kernel cmdline (`root=PARTUUID=...`) |
| `kernel-6.1/arch/arm/boot/dts/HZ-RK3506G2_MiniEVM_TF.dts` | Board DTS: SPI panel, backlight, no SoC sound card |
| `kernel-6.1/arch/arm/configs/rk3506-game.config` | Kernel fragment: tinydrm, USB audio, VT/logo, gadget serial |
| `kernel-6.1/arch/arm/configs/rk3506-display.config` | Display fragment |
| `buildroot/configs/rockchip_rk3506_game_defconfig` | Buildroot: Retro-Zero, cores, dosfstools, sfdisk |
| `buildroot/package/retro-zero/` | The Retro-Zero frontend package, including console integration under `console/` |
| `buildroot/board/rockchip/rk3506/fs-retrogaming-overlay/` | Board-only overlay: inittab, data-partition init, USB serial, boot audio |
| `buildroot/package/retroarch/libretro-*/` | Emulator core packages (build flags, download hashes) |

Audio is deliberately USB-only: the original MAX98357A I2S amplifier support
(SoC `sai0`, codec node, `asound.conf` softvol) was removed, and
`CONFIG_SND_USB_AUDIO` enabled instead.

### The frontend: Retro-Zero instead of RetroArch

[Retro-Zero](https://github.com/geo-tp/Retro-Zero) is a Libretro frontend with
an LVGL interface, and it replaced RetroArch here. The decisive difference on
this GPU-less board is how each one draws:

- RetroArch's only software-rendering path was its SDL1 `sdl_dingux` driver,
  which needed SDL plus the `SDL_NOMOUSE` workaround.
- Retro-Zero's LVGL UI uses `lv_linux_fbdev` and its game frames go through
  `src/Video/fbdev_video.cpp` — straight `/dev/fb0`, nothing in between. Its
  EGL/GLES2 path is optional and is disabled (`CP0_WITH_EGL_FBDEV=0`); the
  binary links only `libasound`, `libstdc++`, `libm`, `libgcc_s` and `libc`.
  SDL is no longer built at all.

It is a normal Buildroot package (`buildroot/package/retro-zero/`), with the
console integration behind `BR2_PACKAGE_RETRO_ZERO_CONSOLE`:

| File in `package/retro-zero/console/` | Installs to |
|---|---|
| `S99retrozero` | `/etc/init.d/S99retrozero` — starts Retro-Zero at boot |
| `retro-zero-forever` | `/usr/bin/retro-zero-forever` — respawn supervisor |

The supervisor relaunches Retro-Zero (into its ROM browser) whenever it exits,
so quitting, restarting, or crashing returns to the front-end rather than a
blank framebuffer. It creates `/root/roms` and `/root/saves`.

### Two upstream adaptations

`package/retro-zero/0001-adapt-to-rk3506.patch` (documented in
`package/retro-zero/README.md`) handles the parts of upstream that assume its
original ARM64 target:

- **Cores** — upstream downloads prebuilt **ARM64** libretro cores from GitHub
  on demand; those cannot load on this armv7 board. The patch trims the core
  registry to the systems we ship ARM32 cores for, points them at
  `/usr/lib/libretro`, and blanks the download URLs so nothing is fetched at
  runtime.
- **Paths** — upstream hardcodes `/home/pi/…`. The patch maps ROMs to
  `/root/roms/<system>` and saves to `/root/saves/`.

### Libretro cores are frontend-independent

The vendor gates every core's `Config.in` on `BR2_PACKAGE_RETROARCH`, which
would make it impossible to build a core without RetroArch. Those dependencies
are removed and the cores menu is no longer gated, because the cores are
frontend-agnostic: Retro-Zero loads the same `.so` files RetroArch did. The
core packages themselves are unchanged.

### Rootfs overlay

What remains in `board/rockchip/rk3506/fs-retrogaming-overlay/` is genuinely
board-specific:

- `etc/init.d/S20data` — first boot: formats the leftover GPT `data` partition
  as FAT32 (label `GAME`), retypes its GPT GUID to "Microsoft basic data" so
  Windows assigns a drive letter, then mounts it at `/root/roms` and creates
  the per-system folders (`nes`, `snes`, `gb`, `gbc`, `gba`, `md`, `sms`,
  `gg`, `music`).
- `etc/network/interfaces` — static `192.168.10.1/24` on `eth0`.
- `etc/init.d/S45ftpd` — anonymous FTP (busybox `ftpd` via `tcpsvd`, write
  enabled) rooted at `/root`, so ROMs and music can be pushed from a PC.
- `etc/inittab` — a getty on the USB gadget serial (`ttyGS0`).
- `etc/init.d/S40usbserial`, `S50usbdevice.sh` — USB gadget setup.
- `root/overmyhead.wav` — 16-bit/44.1kHz stereo test/boot audio.

`buildroot/board/rockchip/rk3506/busybox-game.fragment` adds the `ftpd` and
`tcpsvd` applets to busybox for the FTP server.

### Emulator cores

| Core | Systems |
|---|---|
| `mgba` | Game Boy Advance |
| `fceumm` | NES / Famicom |
| `gambatte` | Game Boy / Game Boy Color |
| `snes9x2005` | SNES / Super Famicom |
| `picodrive` | Mega Drive / Master System / Game Gear / 32X |

All are ARM-friendly and built for the A7. There is no NDS core: the RK3506 has
no GPU and only two A7 cores, and the buildroot tree ships no melonds/desmume
package. The vendor Kodi packages were removed as well — this device is a
dedicated emulation console, and nothing in the tree depended on them.

### MP3 music, Ethernet, FTP, Chinese UI

`0003-add-music-player.patch` adds a **音乐 (Music)** card to the Retro-Zero
launcher. It browses `/root/roms/music/*.mp3` (that folder is on the FAT32
`GAME` partition, so it can also be filled from Windows) and plays through
`mpg123` (`-o alsa`), one detached process per track; OK pauses, UP/DOWN
switch tracks, any other key stops, and tracks auto-advance. No decoder is
linked into the frontend.

`0004-zh-cn-ui-eth-upload.patch` localizes the launcher UI to Chinese (menus,
settings, prompts and hints — the CJK font work from `0002` covers the glyphs)
and reworks the ROM Upload tool for this board: it now binds to any interface
instead of Wi-Fi only, so uploading over `http://192.168.10.1` works, and the
upload target is `/root/roms/<system>`.

Networking: the board DTS already enables `gmac0` (RMII PHY); the kernel
fragment bakes `stmmac-platform` + `dwmac-rockchip` in (instead of modules
loaded by a udev coldplug race) and enables the common PHY drivers. `eth0`
comes up as `192.168.10.1/24` — point a PC at e.g. `192.168.10.2/24` and use
either the FTP server (`ftp://192.168.10.1`, anonymous, read/write into
`/root`) or the browser upload page. The SDK's Wi-Fi/BT stage (`RK_WIFIBT`)
is disabled in the board defconfig: there is no Wi-Fi hardware, so the
`RTL8188EU` module, firmware and `S36wifibt` init script are no longer built
or shipped.

## Partition layout

`data` grows to fill the card; `rootfs` is fixed at 1GB.

```
oem       0x00040000 @ 0x00068000   (128M)
userdata  0x00040000 @ 0x000A8000   (128M)
rootfs    0x00200000 @ 0x000E8000   (1G, fixed)
data      grow       @ 0x002E8000   (FAT32 label "GAME", mounted /root/roms)
```

The `data` partition is what you see as a removable drive on a PC. Retro-Zero's
browser looks in one folder per system (`/root/roms/nes`, `/root/roms/snes`,
`/root/roms/gb`, `/root/roms/gbc`, `/root/roms/gba`, `/root/roms/md`,
`/root/roms/sms`, `/root/roms/gg`); the first boot creates them, so dropping a
file into the matching folder is all it takes.

## Building

Requires a Linux x86_64 host. Buildroot fetches its own sources into
`buildroot/dl/` (not tracked here).

### One-click

```sh
./make-firmware.sh
```

This wraps `./build.sh` and leaves a flashable **`./firmware.img`** in the
project root, printing its size and sha256. It handles the parts that trip
people up: sets the `PATH` the SDK expects, picks the default board
non-interactively on a fresh clone, retries the flaky `sources.buildroot.net`
reachability check once, and on failure prints the tail of the real error
instead of thousands of log lines (full log at `output/build-firmware.log`).

```sh
./make-firmware.sh <board>_defconfig   # build a different board
./make-firmware.sh -h                  # help
```

### Manually

A fresh clone has no board selected, so pass the defconfig once:

```sh
env PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    ./build.sh HZ-RK3506G2_MiniEVM_TF_defconfig
```

Then build normally:

```sh
env PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" ./build.sh
```

Output: `output/update/Image/update.img`, to be written with the Rockchip
SDDiskTool / upgrade tool. Component targets also work:
`./build.sh uboot | kernel | buildroot`.

### The cross toolchain

`build.sh` needs the prebuilt ARM toolchain
(`prebuilts/gcc/linux-x86/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf`).
It is **not stored in this repository** — it is 516MB of binaries that ARM
republishes verbatim, and compressing it into git still costs ~131MB.

Instead, `device/rockchip/common/scripts/fetch-toolchain.sh` downloads the
official 99MB tarball, verifies its sha256, unpacks it to a staging directory,
and installs it atomically. `get_toolchain()` calls that script automatically,
so a normal build fetches it on demand. To do it ahead of time:

```sh
device/rockchip/common/scripts/fetch-toolchain.sh
```

### Notes and gotchas

- **`env PATH=...`** — the SDK scripts misbehave with a non-standard `PATH`.
- **Defconfig selection** — running `./build.sh` with no argument on a fresh
  clone prints a numbered list and waits for input; pick
  `5. HZ-RK3506G2_MiniEVM_TF_defconfig`, or pass it as above. Note that
  passing a defconfig only *selects* the board: it writes `output/.config`
  and exits. `make-firmware.sh` handles running the real build afterwards.
- **Parallelism is capped** — both buildroot defconfigs set `BR2_JLEVEL=8`
  (the default, `0`, means `1 + nproc`). On a host with many cores but little
  RAM, the default exhausts memory and corrupts build output: `host-libopenssl`
  dies with `Bus error (core dumped)` and `libcrypto.a: error adding symbols:
  file format not recognized`. This only bites on a from-scratch build, since
  an existing `output/` keeps the affected stamps. Raise the value on a
  machine with plenty of memory.
- **`sources.buildroot.net` reachability check** — the build probes it and
  aborts if unreachable. Behind a proxy this is flaky: a `403` plus a failed
  ping trips it even when downloads would succeed. `make-firmware.sh` retries
  once automatically; a manual build just needs re-running.
- **GCC 12 and `-fcommon`** — the emulator cores are built with `-fcommon`
  because GCC 12 defaults to `-fno-common`, which breaks these older cores at
  link time with "multiple definition" errors.
- **`picodrive` flags** — its `cpu/cyclone` submodule compiles a *host-side*
  code generator with the same CFLAGS, so ARM-only flags (`-mcpu`/`-mfpu`)
  break it. It gets `-O3 -fcommon -flto` only; the other cores additionally get
  `-mcpu=cortex-a7 -mfpu=neon-vfpv4`.

## Provenance

The tree descends from Rockchip's `rk3506_linux6.1_release_v1.2.0` (SDK Release
V1.2.0, 2025-03-10; Linux 6.1.118, U-Boot 2017.09). The original checkout was
made with the `repo` tool, but its `.repo` directory is **gone**, leaving 43
dangling `.git` symlinks — per-component history and upstream commit IDs are
unrecoverable.

Rockchip's manifest repository is not public (`redmine.rock-chips.com`,
partner access only), so this cannot be expressed as submodules: no valid
upstream URLs exist for most components, and the board DTS and defconfig are
vendor-private and appear in no upstream repository. Pointing at upstream
commits would produce a tree that does not build. Hence: one flat snapshot.

### Not tracked

| Excluded | Why |
|---|---|
| `output/`, `buildroot/output/`, `buildroot/dl/` | Build products and download cache, all reproducible |
| `prebuilts/gcc/` | Cross toolchain; fetched by `fetch-toolchain.sh` (see above) |
| `Ubuntu/`, `rtos/` | Prebuilt Ubuntu rootfs image and RTOS/AMP SDK, unused here (`RK_AMP` is unset) |
| Vendor tool archives | DDR/pin-debug/sign/production-programming tools; the flashing tools (`SDDiskTool`, `RKDevTool`, `DriverAssitant`, ...) are kept |
| Vendor docs (`docs/**/*.pdf`) | Rockchip PDF manuals; the non-PDF provenance notes are kept |
| Kernel/U-Boot build artifacts | `*.o`, `vmlinux`, `*.dtb`, `System.map`, generated headers |

`external/rkwifibt/` **is** tracked even though it looks optional: the board
defconfig sets `RK_WIFIBT=y` with `RK_WIFIBT_CHIP=RTL8188EU`, so `post-wifibt.sh`
compiles the rtl8188eu driver and copies firmware from it.

The vendor SDK's own per-tree `.gitignore` files (in `kernel-6.1/`, `u-boot/`,
`buildroot/`, `tools/`, `docs/`) are authoritative for build artifacts; the
root `.gitignore` only covers top-level bulk.
