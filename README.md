# HZ-RK3506 MiniEVM 复古游戏机 / Retro Gaming Console

基于合众跃恒 HZ-RK3506 开发板的复古游戏机，前端为 Retro-Zero（Libretro 前端）。

A retro gaming console built on the **HZ-RK3506G2 MiniEVM** board (Rockchip
RK3506G2, dual Cortex-A7, no GPU), running Buildroot with
[Retro-Zero](https://github.com/geo-tp/Retro-Zero) as the Libretro front-end.

**简体中文** | [English](#english)

---

<a id="zh"></a>

## 简体中文

本仓库是瑞芯微 Linux 6.1 SDK（`rk3506_linux6.1_release_v1.2.0`）的单树快照，
加上了把它变成掌机的全部板级定制。它**不是**瑞芯微官方仓库，也**不是**
`repo` 工具的检出版本，原因见[来源说明](#来源说明)。

### 硬件

| | |
|---|---|
| 板卡 | HZ-RK3506G2 MiniEVM（RK3506G2，双核 Cortex-A7，无 GPU） |
| 屏幕 | ILI9341 2.8 寸 320x240 SPI 屏，`tinydrm` 驱动 |
| 音频 | USB 声卡（USB Audio Class） |
| 输入 | USB OTG：CDC-ACM 串口控制台 + USB HID 摇杆 |
| 网络 | 无。以太网在设备树中禁用，没有任何联网服务——纯离线设备，传文件走 USB |
| 存储 | TF 卡，GPT 分区：`oem` / `userdata` / `rootfs` / `data` |
| 固件 | Retro-Zero 前端（LVGL 直绘 `/dev/fb0`）+ libretro 模拟器核心 |

调试控制台走 USB gadget 串口（`/dev/ttyGS0`），电脑上显示为一个 COM 口。
传 ROM/音乐走同一个 OTG 口的 **USB 虚拟盘**（mass-storage gadget，见下文）。
Retro-Zero 开机直接进入 ROM 浏览器，并由守护脚本负责拉起——退出、重启或崩溃
都会回到浏览器，而不是停在黑屏上。

### 定制内容

真正与厂商 SDK 不同的文件只有少数几个：

| 路径 | 用途 |
|---|---|
| `device/rockchip/.chips/rk3506/HZ-RK3506G2_MiniEVM_TF_defconfig` | 板级 defconfig：选择 DTS、内核配置片段、根文件系统 overlay |
| `device/rockchip/.chips/rk3506/parameter-EMMC.txt` | 分区表 + 内核命令行（`root=PARTUUID=...`） |
| `kernel-6.1/arch/arm/boot/dts/HZ-RK3506G2_MiniEVM_TF.dts` | 板级 DTS：SPI 屏（50MHz）、背光、无 SoC 声卡、以太网 gmac0 禁用 |
| `kernel-6.1/arch/arm/configs/rk3506-game.config` | 内核片段：tinydrm、USB 声卡、VT/logo、gadget 串口 + mass storage、以太网驱动禁用 |
| `kernel-6.1/arch/arm/configs/rk3506-display.config` | 显示片段 |
| `buildroot/configs/rockchip_rk3506_game_defconfig` | Buildroot：Retro-Zero、模拟器核心、mpg123 |
| `buildroot/package/retro-zero/` | Retro-Zero 前端 package（含 `console/` 下的开机集成） |
| `buildroot/board/rockchip/rk3506/fs-retrogaming-overlay/` | 板级 overlay：inittab、data 分区初始化、USB 串口、开机音频 |
| `buildroot/package/retroarch/libretro-*/` | 模拟器核心 package（编译参数、下载校验） |

音频刻意做成 USB-only：原来的 MAX98357A I2S 功放支持（SoC `sai0`、codec 节点、
`asound.conf` 软音量）已移除，改为打开 `CONFIG_SND_USB_AUDIO`。

### 前端：用 Retro-Zero 替代 RetroArch

[Retro-Zero](https://github.com/geo-tp/Retro-Zero) 是一个带 LVGL 界面的
Libretro 前端，在这里取代了 RetroArch。这块无 GPU 的板子上，决定性的差别在
绘图路径：

- RetroArch 唯一的软渲染路径是 SDL1 的 `sdl_dingux` 驱动，需要 SDL 加
  `SDL_NOMOUSE` 变通。
- Retro-Zero 的 LVGL 界面走 `lv_linux_fbdev`，游戏画面走
  `src/Video/fbdev_video.cpp`——直接写 `/dev/fb0`，中间没有任何层。
  EGL/GLES2 路径是可选的，这里已关闭（`CP0_WITH_EGL_FBDEV=0`）；最终二进制
  只链接 `libasound`、`libstdc++`、`libm`、`libgcc_s`、`libc`，SDL 完全不再参与编译。

它是一个标准 Buildroot package（`buildroot/package/retro-zero/`），开机集成在
`BR2_PACKAGE_RETRO_ZERO_CONSOLE` 后面：`S99retrozero`（开机启动）和
`retro-zero-forever`（守护拉起）。

### 七个上游适配补丁

`package/retro-zero/` 下的补丁处理上游对本板不适用的地方：

- **`0001-adapt-to-rk3506.patch`** — 上游默认 25 个核心、按需从 GitHub 下载
  **ARM64** 预编译 `.so`，在 armv7 上根本加载不了。补丁把核心表裁剪到本固件
  实际提供的 8 个系统，`corePath` 指向 `/usr/lib/libretro`，并清空下载 URL；
  同时把上游硬编码的 `/home/pi/…` 改为 ROM `/root/roms/<系统>`、存档
  `/root/saves/`。
- **`0002-add-cjk-font.patch`** — 引入 tiny_ttf + 文泉驿微米黑（`wqy-cjk.ttf`），
  以 `cjk_font_10/12/20/28` 供全界面使用，中文文件名不再乱码。
- **`0003-add-music-player.patch`** — 新增 **音乐** 卡片，浏览
  `/root/roms/music/*.mp3`（该目录在 FAT32 GAME 分区上，Windows 直接拷歌即可），
  通过 `mpg123 -o alsa` 播放，每首歌一个独立进程：确认键暂停、上/下切歌、
  其他键停止、播完自动下一首。前端本身不链接任何解码库。
- **`0004-zh-cn-ui-eth-upload.patch`** — 界面全面汉化（菜单、设置、弹窗、提示，
  字形来自 0002）；原带一个依赖以太网的浏览器上传工具，后被 0007 取代。
- **`0005-usb-gamepad-support.patch`** — 原生手柄层：现代 Linux 手柄标准
  `BTN_*` 映射 + 复古双摇杆（0810:0001）自动配置、ABS 轴 + HAT 十字键、
  游戏内直接映射 Libretro 按键、菜单消费抽象方向/确认/返回、每秒热插拔扫描；
  SELECT+START 退回菜单。另含 GB 强制 DMG 模式等核心选项修正。
- **`0006-move-page-dots.patch`** — 轮播页圆点下移，不遮卡片内容。
- **`0007-usb-virtual-disk.patch`** — 把"上传"工具重做成 **USB 虚拟盘**：
  菜单项改名"USB虚拟盘"；进入时 umount `/root/roms`，在 `S40usbserial` 建好的
  g1 复合 gadget 上挂 `mass_storage.0` 功能并重绑 UDC（串口控制台保留），
  Windows 直接弹出 GAME 磁盘；按任意键退出时卸下 LUN 并重新挂载数据分区。
  替代了 0004 里的网络上传方案。

### 模拟器核心

| 核心 | 系统 |
|---|---|
| `fceumm` | NES / FC |
| `snes9x2005` | SNES / SFC |
| `gambatte` | Game Boy / Game Boy Color |
| `mgba` | Game Boy Advance |
| `picodrive` | Mega Drive / Master System / Game Gear / 32X |

全部对 ARM 友好，按 Cortex-A7 编译。没有 NDS 核心：RK3506 无 GPU、只有两个
A7，buildroot 树里也没有 melonds/desmume 包。厂商的 Kodi 相关包同样已移除——
这是台专注模拟的主机，树里没有任何东西依赖它们。

核心 package 原本被厂商 gate 在 `BR2_PACKAGE_RETROARCH` 之后；该依赖已去除
（核心本身与前端无关，Retro-Zero 加载的是同一批 `.so`），核心 package 本体未改动。

### 音乐 / USB 虚拟盘 / 中文界面

- **MP3**：见补丁 0003。传歌方式：USB 虚拟盘的 `music` 文件夹，或把 TF 卡
  插到电脑上直接拷。
- **USB 虚拟盘**：见补丁 0007。进入菜单项后 GAME 分区以 UMS 形式挂到电脑，
  全程无需网络。
- **离线设计**：以太网在设备树层禁用（`&gmac0 status="disabled"`），内核
  不再编译 stmmac/dwmac-rockchip；系统里没有任何 FTP/SMB/HTTP 服务或监听
  端口。唯一对外的通路是 OTG 口的 USB（串口控制台 + 虚拟盘）。
- **Wi-Fi**：板上没有 Wi-Fi 硬件，板级 defconfig 已清除 `RK_WIFIBT`，
  RTL8188EU 模块、固件和 `S36wifibt` 初始化脚本不再编译、不再进固件。

### 分区布局

`data` 分区占满卡剩余空间；`rootfs` 固定 1GB。

```
oem       0x00040000 @ 0x00068000   (128M)
userdata  0x00040000 @ 0x000A8000   (128M)
rootfs    0x00200000 @ 0x000E8000   (1G, 固定)
data      grow       @ 0x002E8000   (FAT32 卷标 "GAME", 挂载到 /root/roms)
```

`data` 就是在电脑上显示为可移动磁盘的那个分区。Retro-Zero 的浏览器按系统分
目录查找：`nes` / `snes` / `gb` / `gbc` / `gba` / `md` / `sms` / `gg` /
`music`，首次开机自动创建——把文件放进对应文件夹即可。

`S20data` 首次开机还会把瑞芯微专有的 GPT 分区类型 GUID 改写为
"Microsoft basic data"，否则 Windows 不给这个分区分配盘符。

### 编译

需要 Linux x86_64 主机。Buildroot 源码包自动下载到 `buildroot/dl/`（不入库）。

#### 一键编译

```sh
./make-firmware.sh
```

封装了 `./build.sh`，完成后在项目根目录生成可直接烧录的 **`./firmware.img`**，
并打印大小和 sha256。它处理了几个容易踩的坑：设置 SDK 需要的 `PATH`、全新
clone 上非交互式选板、`sources.buildroot.net` 连通性检测失败自动重试一次、
并以"日志无 ERROR + update.img 确实是新写的"为准判定成败（SDK 的 build.sh
在钩子失败后偶尔仍返回 0），失败时只打印真实错误的末尾几行而不是几千行日志
（完整日志在 `output/build-firmware.log`）。

```sh
./make-firmware.sh <board>_defconfig   # 编译别的板子
./make-firmware.sh -h                  # 帮助
```

#### 手动编译

全新 clone 没有选中板子，先传一次 defconfig：

```sh
env PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    ./build.sh HZ-RK3506G2_MiniEVM_TF_defconfig
```

注意：传 defconfig 只是**选中**板子（写完 `output/.config` 就退出），真正的
编译要再跑一次：

```sh
env PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" ./build.sh
```

产物：`output/update/Image/update.img`，用瑞芯微 SDDiskTool / 升级工具烧写。
也支持按组件编译：`./build.sh uboot | kernel | buildroot`。

#### 交叉工具链

`build.sh` 需要预编译 ARM 工具链
（`prebuilts/gcc/linux-x86/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf`）。
它**不入库**——516MB 的二进制，ARM 官方原样发布，压进 git 也要 ~131MB。

`device/rockchip/common/scripts/fetch-toolchain.sh` 会下载官方 99MB 压缩包、
校验 sha256、解压到暂存目录后原子替换。`get_toolchain()` 会自动调用它，正常
编译按需获取；也可提前手动执行：

```sh
device/rockchip/common/scripts/fetch-toolchain.sh
```

#### 注意事项

- **`env PATH=...`** — SDK 脚本在非标准 `PATH` 下会出错。
- **defconfig 只是选择** — 见上；`make-firmware.sh` 会替你接着跑真正的编译。
- **并行度被封顶** — 两个 buildroot defconfig 都设了 `BR2_JLEVEL=8`
  （默认 `0` 表示 `1 + nproc`）。多核少内存的主机上默认值会耗尽内存、损坏
  编译产物：`host-libopenssl` 报 `Bus error (core dumped)`，
  `libcrypto.a: error adding symbols: file format not recognized`。只在从零
  全量编译时才会咬人（已有的 `output/` 保留着相关时间戳）。内存充裕的机器
  可以调高。
- **`sources.buildroot.net` 连通性检测** — 编译前会探测它，不通就中止。走
  代理时这个检测很飘（403 加 ping 失败就会触发，哪怕实际下载没问题）。
  `make-firmware.sh` 会自动重试一次；手动编译重跑即可。
- **GCC 12 与 `-fcommon`** — 模拟器核心用 `-fcommon` 编译，因为 GCC 12 默认
  `-fno-common`，会让这些老核心在链接期报 "multiple definition"。
- **`picodrive` 编译参数** — 它的 `cpu/cyclone` 子模块会用同一套 CFLAGS 编译
  一个**主机侧**代码生成器，ARM 专属参数（`-mcpu`/`-mfpu`）会弄坏它，所以它
  只拿 `-O3 -fcommon -flto`；其余核心额外加
  `-mcpu=cortex-a7 -mfpu=neon-vfpv4`。

### 来源说明

本树源自瑞芯微 `rk3506_linux6.1_release_v1.2.0`（SDK Release V1.2.0，
2025-03-10；Linux 6.1.118，U-Boot 2017.09）。

#### 不入库的内容

| 排除项 | 原因 |
|---|---|
| `output/`、`buildroot/output/`、`buildroot/dl/` | 编译产物与下载缓存，全部可复现 |
| `prebuilts/gcc/` | 交叉工具链，由 `fetch-toolchain.sh` 拉取（见上） |
| `Ubuntu/`、`rtos/` | 预编译 Ubuntu rootfs 镜像与 RTOS/AMP SDK，本板用不到（`RK_AMP` 未设置） |
| 厂商工具压缩包 | DDR/引脚调试/签名/量产工具；烧录工具（`SDDiskTool`、`RKDevTool`、`DriverAssitant` 等）保留 |
| 厂商文档（`docs/**/*.pdf`） | 瑞芯微 PDF 手册；非 PDF 的来源说明文件保留 |
| 内核/U-Boot 编译产物 | `*.o`、`vmlinux`、`*.dtb`、`System.map`、生成头文件 |

`external/rkwifibt/` 作为厂商快照的一部分**仍然入库**，但已不参与编译：板级
defconfig 清除了 `RK_WIFIBT`（板上无 Wi-Fi 硬件），`post-wifibt.sh` 整段跳过。

厂商 SDK 自带的各目录 `.gitignore`（`kernel-6.1/`、`u-boot/`、`buildroot/`、
`tools/`、`docs/`）对编译产物的界定是权威的；根 `.gitignore` 只管顶层大块头。

---

<a id="english"></a>

## English

This repository is a single-tree snapshot of Rockchip's Linux 6.1 SDK
(`rk3506_linux6.1_release_v1.2.0`) plus every board customization that turns it
into a handheld console. It is **not** an upstream Rockchip repository and
**not** a `repo`-tool checkout — see [Provenance](#provenance-1) for why.

### Hardware

| | |
|---|---|
| Board | HZ-RK3506G2 MiniEVM (RK3506G2, 2x Cortex-A7, no GPU) |
| Display | ILI9341 2.8" 320x240 SPI panel, driven by `tinydrm` |
| Audio | USB sound card (USB Audio Class) |
| Input | USB OTG: CDC-ACM serial console + USB HID joysticks |
| Network | None. Ethernet is disabled at the device-tree level and no network services exist — a purely offline device; files go in over USB |
| Storage | TF card, GPT partitions: `oem` / `userdata` / `rootfs` / `data` |
| Firmware | Retro-Zero front-end (LVGL UI over `/dev/fb0`) + libretro cores |

The debug console runs over a USB gadget at `/dev/ttyGS0` (a COM port on the
host). ROMs and music go in through the **USB virtual disk** on the same OTG
port (a mass-storage gadget, see below). Retro-Zero boots straight into its
ROM browser, and a supervisor script keeps it alive — quitting, restarting,
or crashing returns to the browser rather than a blank framebuffer.

### What was customized

Only a handful of files differ from the vendor SDK:

| Path | Purpose |
|---|---|
| `device/rockchip/.chips/rk3506/HZ-RK3506G2_MiniEVM_TF_defconfig` | Board defconfig: selects DTS, kernel fragments, rootfs overlay |
| `device/rockchip/.chips/rk3506/parameter-EMMC.txt` | Partition table + kernel cmdline (`root=PARTUUID=...`) |
| `kernel-6.1/arch/arm/boot/dts/HZ-RK3506G2_MiniEVM_TF.dts` | Board DTS: SPI panel (50MHz), backlight, no SoC sound card, gmac0 Ethernet disabled |
| `kernel-6.1/arch/arm/configs/rk3506-game.config` | Kernel fragment: tinydrm, USB audio, VT/logo, gadget serial + mass storage, Ethernet drivers disabled |
| `kernel-6.1/arch/arm/configs/rk3506-display.config` | Display fragment |
| `buildroot/configs/rockchip_rk3506_game_defconfig` | Buildroot: Retro-Zero, cores, mpg123 |
| `buildroot/package/retro-zero/` | The Retro-Zero frontend package, incl. console integration under `console/` |
| `buildroot/board/rockchip/rk3506/fs-retrogaming-overlay/` | Board-only overlay: inittab, data-partition init, USB serial, boot audio |
| `buildroot/package/retroarch/libretro-*/` | Emulator core packages (build flags, download hashes) |

Audio is deliberately USB-only: the original MAX98357A I2S amplifier support
(SoC `sai0`, codec node, `asound.conf` softvol) was removed, and
`CONFIG_SND_USB_AUDIO` enabled instead.

### The frontend: Retro-Zero instead of RetroArch

[Retro-Zero](https://github.com/geo-tp/Retro-Zero) is a Libretro frontend with
an LVGL interface, and it replaced RetroArch here. On this GPU-less board the
decisive difference is how each one draws:

- RetroArch's only software-rendering path was its SDL1 `sdl_dingux` driver,
  which needed SDL plus the `SDL_NOMOUSE` workaround.
- Retro-Zero's LVGL UI uses `lv_linux_fbdev` and its game frames go through
  `src/Video/fbdev_video.cpp` — straight `/dev/fb0`, nothing in between.
  EGL/GLES2 is optional and disabled here (`CP0_WITH_EGL_FBDEV=0`); the binary
  links only `libasound`, `libstdc++`, `libm`, `libgcc_s` and `libc`, and SDL
  is no longer built at all.

It is a normal Buildroot package (`buildroot/package/retro-zero/`), with the
console integration behind `BR2_PACKAGE_RETRO_ZERO_CONSOLE`: `S99retrozero`
(start at boot) and `retro-zero-forever` (respawn supervisor).

### Seven upstream adaptation patches

The patches in `package/retro-zero/` handle everything upstream assumes that
does not hold here:

- **`0001-adapt-to-rk3506.patch`** — upstream lists 25 cores and downloads
  prebuilt **ARM64** `.so` files from GitHub on demand; those cannot load on
  armv7. The patch trims the registry to the 8 systems this firmware ships
  cores for, points `corePath` at `/usr/lib/libretro`, blanks the download
  URLs, and remaps the hardcoded `/home/pi/…` to ROMs `/root/roms/<system>`
  and saves `/root/saves/`.
- **`0002-add-cjk-font.patch`** — tiny_ttf + WenQuanYi Micro Hei
  (`wqy-cjk.ttf`), exposed as `cjk_font_10/12/20/28` and used across the UI,
  so Chinese filenames render correctly.
- **`0003-add-music-player.patch`** — adds a **Music** card browsing
  `/root/roms/music/*.mp3` (that folder lives on the FAT32 GAME partition, so
  it can be filled straight from Windows). Playback shells out to
  `mpg123 -o alsa`, one detached process per track: OK pauses, UP/DOWN switch
  tracks, any other key stops, and tracks auto-advance. No decoder is linked
  into the frontend.
- **`0004-zh-cn-ui-eth-upload.patch`** — localizes the launcher UI to Chinese
  (menus, settings, prompts, hints; glyphs come from 0002). It originally
  shipped a browser upload tool that depended on Ethernet; superseded by 0007.
- **`0005-usb-gamepad-support.patch`** — a native gamepad layer: modern Linux
  gamepad `BTN_*` mapping plus a legacy twin-stick (0810:0001) autoconfig
  profile, ABS-axis and HAT d-pad handling, direct Libretro joypad mapping
  in-game, abstract direction/confirm/back consumed by the menu, a hot-plug
  rescan every second, and SELECT+START to return to the menu. Also carries
  core-option fixes such as forcing plain DMG mode for GB.
- **`0006-move-page-dots.patch`** — moves the carousel page dots downward so
  they no longer overlap the cards.
- **`0007-usb-virtual-disk.patch`** — reworks the upload tool into a **USB
  virtual disk**: the menu entry is renamed accordingly; on entry it unmounts
  `/root/roms`, attaches a `mass_storage.0` function to the g1 composite
  gadget created by `S40usbserial`, and rebinds the UDC (serial console
  stays up), so Windows pops up the GAME drive; pressing any key detaches
  the LUN and remounts the partition. Replaces 0004's network upload scheme.

### Emulator cores

| Core | Systems |
|---|---|
| `fceumm` | NES / Famicom |
| `snes9x2005` | SNES / Super Famicom |
| `gambatte` | Game Boy / Game Boy Color |
| `mgba` | Game Boy Advance |
| `picodrive` | Mega Drive / Master System / Game Gear / 32X |

All are ARM-friendly and built for the A7. There is no NDS core: the RK3506
has no GPU and only two A7 cores, and the buildroot tree ships no
melonds/desmume package. The vendor Kodi packages were removed as well — this
device is a dedicated emulation console, and nothing in the tree depended on
them.

The vendor gates every core's `Config.in` on `BR2_PACKAGE_RETROARCH`; those
dependencies are removed because the cores are frontend-agnostic — Retro-Zero
loads the same `.so` files. The core packages themselves are unchanged.

### Music / USB virtual disk / Chinese UI

- **MP3**: see patch 0003. Two ways to load songs: the `music` folder of the
  USB virtual disk, or popping the TF card into a PC.
- **USB virtual disk**: see patch 0007. Selecting the menu entry exposes the
  GAME partition over USB mass storage — no network involved.
- **Offline by design**: Ethernet is disabled at the device-tree level
  (`&gmac0 status = "disabled"`) and the kernel no longer builds
  stmmac/dwmac-rockchip; there are no FTP/SMB/HTTP services or listening
  ports anywhere on the system. The only paths to the outside are the OTG
  port's USB functions (serial console + mass storage).
- **Wi-Fi**: there is no Wi-Fi hardware, so the board defconfig clears
  `RK_WIFIBT`; the RTL8188EU module, its firmware and the `S36wifibt` init
  script are neither built nor shipped.

### Partition layout

`data` grows to fill the card; `rootfs` is fixed at 1GB.

```
oem       0x00040000 @ 0x00068000   (128M)
userdata  0x00040000 @ 0x000A8000   (128M)
rootfs    0x00200000 @ 0x000E8000   (1G, fixed)
data      grow       @ 0x002E8000   (FAT32 label "GAME", mounted /root/roms)
```

The `data` partition is what shows up as a removable drive on a PC. Retro-Zero
browses one folder per system — `nes` / `snes` / `gb` / `gbc` / `gba` / `md` /
`sms` / `gg` / `music` — created on first boot; dropping a file into the
matching folder is all it takes.

On first boot `S20data` also retypes the RK-proprietary GPT partition GUID to
"Microsoft basic data"; without that, Windows refuses to assign a drive letter.

### Building

Requires a Linux x86_64 host. Buildroot fetches its own sources into
`buildroot/dl/` (not tracked here).

#### One-click

```sh
./make-firmware.sh
```

Wraps `./build.sh` and leaves a flashable **`./firmware.img`** in the project
root, printing size and sha256. It handles the parts that trip people up:
sets the `PATH` the SDK expects, picks the default board non-interactively on
a fresh clone, retries the flaky `sources.buildroot.net` reachability check
once, and judges success by "no ERROR in the log + update.img actually freshly
written" (the SDK's build.sh occasionally exits 0 after a hook failure). On
failure it prints the tail of the real error instead of thousands of log
lines (full log at `output/build-firmware.log`).

```sh
./make-firmware.sh <board>_defconfig   # build a different board
./make-firmware.sh -h                  # help
```

#### Manually

A fresh clone has no board selected, so pass the defconfig once:

```sh
env PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    ./build.sh HZ-RK3506G2_MiniEVM_TF_defconfig
```

Note that passing a defconfig only *selects* the board (it writes
`output/.config` and exits). The real build is a second invocation:

```sh
env PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" ./build.sh
```

Output: `output/update/Image/update.img`, to be written with the Rockchip
SDDiskTool / upgrade tool. Component targets also work:
`./build.sh uboot | kernel | buildroot`.

#### The cross toolchain

`build.sh` needs the prebuilt ARM toolchain
(`prebuilts/gcc/linux-x86/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf`).
It is **not stored in this repository** — 516MB of binaries that ARM
republishes verbatim; even compressed it would cost ~131MB of git.

`device/rockchip/common/scripts/fetch-toolchain.sh` downloads the official
99MB tarball, verifies its sha256, unpacks it to a staging directory and
installs it atomically. `get_toolchain()` calls it automatically, so a normal
build fetches it on demand. To fetch ahead of time:

```sh
device/rockchip/common/scripts/fetch-toolchain.sh
```

#### Notes and gotchas

- **`env PATH=...`** — the SDK scripts misbehave with a non-standard `PATH`.
- **Defconfig selection** — see above; `make-firmware.sh` runs the real build
  for you afterwards.
- **Parallelism is capped** — both buildroot defconfigs set `BR2_JLEVEL=8`
  (the default, `0`, means `1 + nproc`). On a many-core, low-RAM host the
  default exhausts memory and corrupts output: `host-libopenssl` dies with
  `Bus error (core dumped)` and `libcrypto.a: error adding symbols: file
  format not recognized`. This only bites on a from-scratch build, since an
  existing `output/` keeps the affected stamps. Raise it on a machine with
  plenty of memory.
- **`sources.buildroot.net` reachability check** — the build probes it and
  aborts if unreachable. Behind a proxy this is flaky: a `403` plus a failed
  ping trips it even when downloads would succeed. `make-firmware.sh` retries
  once automatically; a manual build just needs re-running.
- **GCC 12 and `-fcommon`** — the emulator cores are built with `-fcommon`
  because GCC 12 defaults to `-fno-common`, which breaks these older cores at
  link time with "multiple definition" errors.
- **`picodrive` flags** — its `cpu/cyclone` submodule compiles a *host-side*
  code generator with the same CFLAGS, so ARM-only flags (`-mcpu`/`-mfpu`)
  break it. It gets `-O3 -fcommon -flto` only; the other cores additionally
  get `-mcpu=cortex-a7 -mfpu=neon-vfpv4`.

### Provenance

<a id="provenance-1"></a>

The tree descends from Rockchip's `rk3506_linux6.1_release_v1.2.0` (SDK
Release V1.2.0, 2025-03-10; Linux 6.1.118, U-Boot 2017.09).

#### Not tracked

| Excluded | Why |
|---|---|
| `output/`, `buildroot/output/`, `buildroot/dl/` | Build products and download cache, all reproducible |
| `prebuilts/gcc/` | Cross toolchain; fetched by `fetch-toolchain.sh` (see above) |
| `Ubuntu/`, `rtos/` | Prebuilt Ubuntu rootfs image and RTOS/AMP SDK, unused here (`RK_AMP` is unset) |
| Vendor tool archives | DDR/pin-debug/sign/production-programming tools; the flashing tools (`SDDiskTool`, `RKDevTool`, `DriverAssitant`, …) are kept |
| Vendor docs (`docs/**/*.pdf`) | Rockchip PDF manuals; the non-PDF provenance notes are kept |
| Kernel/U-Boot build artifacts | `*.o`, `vmlinux`, `*.dtb`, `System.map`, generated headers |

`external/rkwifibt/` is tracked as part of the vendor snapshot but no longer
built: the board defconfig clears `RK_WIFIBT` (no Wi-Fi hardware), so
`post-wifibt.sh` is skipped entirely.

The vendor SDK's own per-tree `.gitignore` files (in `kernel-6.1/`, `u-boot/`,
`buildroot/`, `tools/`, `docs/`) are authoritative for build artifacts; the
root `.gitignore` only covers top-level bulk.
