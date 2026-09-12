# retro-zero

Buildroot package for [Retro-Zero](https://github.com/geo-tp/Retro-Zero), a
Libretro frontend with an LVGL interface, used here as the console front-end in
place of RetroArch.

## Why it works on this board

Retro-Zero is upstream-targeted at the Cardputer Zero (ARM64, GLES2). This
board is an RK3506G2: **armv7 (32-bit) with no GPU at all**. Two things make it
usable anyway:

- It has a plain `/dev/fb0` video path (`src/Video/fbdev_video.cpp`) and its
  LVGL UI uses `lv_linux_fbdev`, so no GPU, KMS or EGL is involved.
- EGL is fully behind `CP0_WITH_EGL_FBDEV`, and `eglfbdev_video.cpp` compiles
  down to a stub when it is 0. The package sets `CP0_WITH_EGL_FBDEV=0`; the
  resulting binary links only `libasound`, `libstdc++`, `libm`, `libgcc_s` and
  `libc` — no EGL/GLES.

## Patch: 0001-adapt-to-rk3506.patch

Two upstream behaviours are unsuitable as-is:

- **Core registry** — upstream lists 25 cores and downloads prebuilt `.so`
  files from GitHub on demand. Those are **ARM64** and cannot be loaded on
  armv7. The patch trims the registry to the systems this firmware actually
  ships cores for, points each `corePath` at `/usr/lib/libretro`, and blanks
  the download URLs so nothing is ever fetched at runtime.
- **Save root** — upstream hardcodes `/home/pi/retrozero/saves/`, which does
  not exist here. The patch moves it to `/root/saves/`.

ROM directories are remapped to `/root/roms/<system>` in the same patch
(`/root/roms` is where the FAT32 data partition is mounted, so ROMs are copied
straight from a PC).

## Patch: 0002-add-cjk-font.patch

tiny_ttf + `wqy-cjk.ttf` (WenQuanYi Micro Hei) give the UI CJK glyphs, exposed
as `cjk_font_10/12/20/28` and used everywhere `lv_font_montserrat_*` was.

## Patch: 0003-add-music-player.patch

Adds a **Music** card (`/root/roms/music/*.mp3`). Playback shells out to
`/usr/bin/mpg123 -o alsa`, one detached process per track: OK pauses
(SIGSTOP/SIGCONT), UP/DOWN switch tracks, any other key stops, tracks
auto-advance when `waitpid` reaps the player in the UI loop. No decoder is
linked into the frontend; `BR2_PACKAGE_MPG123` provides the binary.

## Patch: 0004-zh-cn-ui-eth-upload.patch

Localizes the launcher UI to Chinese (settings, prompts, overlays — the glyphs
come from 0002), and adapts the ROM Upload tool to this board: it accepts any
non-loopback interface instead of `wl*` only, so uploading works over Ethernet
at `http://192.168.10.1`, and the upload target is `/root/roms/<system>`.

## Cores

Retro-Zero loads the same libretro cores as RetroArch. This board ships five
ARM32 cores, mapped onto the registry like this:

| System | Core |
|---|---|
| NES / Famicom | `libretro-fceumm` |
| SNES | `libretro-snes9x2005` |
| Game Boy / Color | `libretro-gambatte` |
| Game Boy Advance | `libretro-mgba` |
| Mega Drive / Master System / Game Gear | `libretro-picodrive` |

The registry patch keeps exactly these eight system entries (plus the Settings
and ROM Upload tools). Adding another core means enabling its
`BR2_PACKAGE_LIBRETRO_*` option and adding a registry entry.

## Licensing

**Upstream Retro-Zero has no LICENSE file**, so by default its code is
all-rights-reserved. Vendored LVGL is MIT. Treat this as a build-time
dependency for private use; do not redistribute the resulting firmware
publicly without clarifying the license with upstream first.

## Configuration

`BR2_PACKAGE_RETRO_ZERO_CONSOLE` installs the boot integration: an init script
(`/etc/init.d/S99retrozero`) and a respawn supervisor
(`/usr/bin/retro-zero-forever`) so that quitting or crashing returns to the ROM
browser instead of a blank screen.
