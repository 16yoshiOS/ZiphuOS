# Ziphu.OS 1.0 — OpenZK (Open-ZiphuKernel)

**Ziphu.OS** is a 32-bit monolithic hobby operating system for the i386 architecture, written from scratch as a freestanding kernel (no host libc) and booted via GRUB using the Multiboot 1 protocol. On top of the kernel runs the "Lightning" (Молния) graphical shell: a monochrome desktop with neon-turquoise window frames, mouse support, themes, a bilingual interface (EN/ZH), and a set of system applications.

The finished bootable ISO image weighs **~566 KB** and contains only the kernel (~52 KB) plus a self-contained GRUB core image with an embedded configuration.

---

## Features

### Kernel
- **Boot:** Multiboot 1; the kernel is linked at physical address 1 MiB (`link.ld`).
- **Architecture:** GDT, IDT, PIC remapped to vectors 32/40, hand-written ISR stubs in assembly (`boot/isr.asm`).
- **Video:** primary mode is VBE/BGA linear framebuffer 640×480×32 (LFB discovery via PCI BAR0 of the Bochs VGA device 1234:1111); on absence, falls back to standard VGA mode 13h (256 colours, 2× downscale). Double buffering: all drawing targets an off-screen back buffer, `fb_present()` copies the whole frame at once — no flicker.
- **Memory:** accounting from multiboot lower/upper mem; a bump-heap above the kernel image (`mem.c`).
- **Interrupts:** 100 Hz timer (IRQ0), keyboard (IRQ1), PS/2 aux mouse (IRQ12).
- **CPUID:** vendor id and brand string detection (`cpuid.c`).
- **VM:** hypervisor presence detection (`vm.c`).
- **Disks:** IDE and SATA probing via PCI class 01/06 scan; BPB/dirent/FAT structures, FAT16/FAT32 cluster-chain reading (`drive.c`).
- **VFS:** a tree-shaped virtual file system (`vfs_node_t`); at boot a root, a shared `/examples` folder, and mount points for detected drives are mounted (`vfs.c`).
- **Resilience:** a watchdog — after 5 s of hang it paints an RSOD (Red Screen of Death) and reboots; a second crash-test branch sends the kernel into `cli; hlt` forever (`crash.c`, `rsod.c`, `timer.c`).

### "Lightning" Shell
- **Desktop:** apps list on the left, a SYSTEM window on the right with live values (CPUID/VM/MEM).
- **Applications:**
  - **Settings** — theme (Light/Dark/Custom RGB), language (EN/ZH), Crash System and Crash VMS buttons.
  - **File Manager** — dual-pane: drives | files, byte sizes.
  - **MemLog** — 256 rows of the memory map.
  - **SysLog** — kernel event log.
  - **About** — about the system.
- **Themes:** Dark by default. The palette is strict monochrome grey with a single accent `#00E5E5` (window frames, boot progress bar, selected rows, buttons). A Custom theme lets you set any RGB (0–255 per channel); the accent and frame follow the chosen colour.
- **Localisation:** all UI strings are stored in parallel EN/ZH tables (`lstr_t { en, zh }`), toggled by a `LANG_EN`/`LANG_ZH` switch in `gui.c`. The ASCII font is the authentic IBM VGA 8×16 ROM (generated from the Linux kernel's `lib/fonts/font_8x16.c`); 16×16 CJK glyphs are generated from GNU Unifont and activated for translated labels.
- **Mouse:** PS/2 aux, 3-button + wheel (Intellimouse), absolute cursor position, hit-testing of buttons and app rows.
- **Keyboard:** a Shift layer — `1`→`!`, `;`→`:` etc.; a text field on the desktop exercises the Shift layer live.

---

## Architecture and Source Layout

```
kernel/
  boot/entry.asm        Multiboot header + _start
  boot/isr.asm          ISR/IRQ stubs, load_gdt/load_idt
  link.ld               linker script (.multiboot first, image at 1M)
  include/              zk_types.h, zk_port.h, zk_util.h, zk_video.h, zk_core.h
  src/
    kmain.c             C entry point: sequential subsystem init
    idt.c               GDT/IDT/PIC + ISR registration
    video.c             VBE/BGA + VGA fallback, drawing primitives, fb_present
    mem.c               RAM accounting + bump heap
    timer.c             IRQ0 100 Hz + watchdog expiry path (RSOD)
    keyboard.c          IRQ1, scan codes, Shift layer
    mouse.c             PS/2 aux, IRQ12, 3/4-byte packet parsing
    cpuid.c             vendor + brand string
    vm.c                hypervisor detection
    drive.c             IDE/SATA probing, FAT16/FAT32
    vfs.c               virtual FS, /examples, drive mounting
    gui.c               "Lightning" shell: themes, windows, apps, mouse (~1037 lines)
    crash.c             crash_arm_watchdog / crash_vms_hang
    rsod.c              Red Screen of Death rendering
    font8x16.c          ASCII font (generated)
    font_cjk.c          CJK font (generated)
    util.c              zk_utoa, zk_memset, dbg_puts (via port 0xE9)
scripts/
  env.sh                sets PATH/LD_LIBRARY_PATH for the local toolchain
  build/iso.sh          production ISO build (grub-mkimage + xorriso)
  build/demo_iso.sh     demo ISO build (auto-cycling via -DZK_DEMO)
  build/gen_font.py     generates 8x16 ASCII font from IBM VGA ROM
  build/gen_cjk_font.py generates 16x16 CJK font from GNU Unifont
  fetch_debs.sh         download/extract .deb into a local prefix (no root)
  capture_qemu.py       headless QEMU screen capture
```

Codebase size: ~3355 lines (C + assembly).

---

## Build Requirements

The bare-metal i386 toolchain:

- **gcc** with `-m32` support (the `gcc-multilib` package)
- **nasm** (assembler, elf32 format)
- **ld** (binutils, `elf_i386` mode)
- **xorriso** (ISO assembly)
- **grub-mkimage** + the `grub-pc-bin` modules (the `/usr/lib/grub/i386-pc` directory)
- **qemu-system-i386** (to run and test)
- **python3** + **Pillow** (only to regenerate the fonts)

In this environment the tools are installed locally under `tools/` (unpacked from `.deb` via `scripts/fetch_debs.sh`, no root). The `scripts/env.sh` script sets `PATH`, `LD_LIBRARY_PATH` and `QEMU_BIOS_DIR` so that `gcc`, `nasm`, `grub-mkimage`, `xorriso` and `qemu-system-i386` resolve to the local binaries.

On a regular Ubuntu/Debian box everything installs in one command:

```bash
sudo apt install gcc-multilib nasm xorriso grub-pc-bin grub-common qemu-system-x86 python3-pil
```

---

## How to Compile

### 1. Get the sources

```bash
# either unpack the ZIP, or work from the repository
unzip ZiphuOS-1.0-src.zip
cd my-project
```

### 2. Activate the toolchain

If the tools are installed system-wide (the apt variant) you can skip this step. If you use the local toolchain under `tools/`:

```bash
source scripts/env.sh
```

After this `which grub-mkimage` should point to `tools/iso/root/usr/bin/grub-mkimage`.

### 3. Build the kernel

```bash
cd kernel
make            # -> build/ziphuos.bin (multiboot ELF, ~52 KB)
```

`make` compiles every `src/*.c` with freestanding flags (`-m32 -ffreestanding -fno-pie -fno-builtin -mno-sse -O2`), assembles `boot/*.asm` with nasm, and links everything with `link.ld` into a flat ELF32.

### 4. Build the bootable ISO

```bash
cd /home/z/my-project
bash scripts/build/iso.sh     # -> kernel/build/ziphuos.iso (~566 KB)
```

What `iso.sh` does:
1. Copies only `ziphuos.bin` into the ISO tree (GRUB modules are NOT copied to disk — `core.img` is self-contained).
2. Generates an `embed.cfg` with the embedded menu and bakes it into the core image.
3. `grub-mkimage` builds a minimal core image with the modules `biosdisk iso9660 multiboot boot`.
4. The El Torito cdboot stub (`cdboot.img`) is prepended to `core.img` — yielding `eltorito.img`.
5. `xorriso` packs an El Torito ISO without Rock Ridge/Joliet (GRUB reads raw ISO9660; the extra trees would only bloat the image).

> **A note on size:** the `xorriso --no-pad` flag trims ~300 KB of trailing zeros, but on a small ISO SeaBIOS then mis-loads the kernel payload — the kernel starts, but its data arrives corrupted and the screen stays black. For this reason the trailing pad is left at the default; the result is ~566 KB.

### 5. Run

```bash
qemu-system-i386 -cdrom kernel/build/ziphuos.iso -m 64M
```

For a headless test with a screen capture:

```bash
qemu-system-i386 -cdrom kernel/build/ziphuos.iso -m 64M \
  -vga std -display none -monitor stdio -serial file:/tmp/ser.log \
  -debugcon file:/tmp/dbg.log
# in the monitor: screendump /tmp/screen.ppm
```

Kernel debug output goes to port 0xE9 (`-debugcon`): the `kmain entry`, `video kind`, `irq_count` logs and so on.

### 6. (Optional) Demo build with auto-cycling apps

The demo variant (`-DZK_DEMO`) automatically cycles through every app on the desktop every 200 ms — handy for a video/presentation without manual input.

```bash
cd kernel
# rebuild gui.c with -DZK_DEMO and link the demo kernel
OBJS=$(ls build/*.o | grep -vE "gui(_demo)?\.o$" | tr '\n' ' ')
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector \
    -fno-asynchronous-unwind-tables -Wall -Wextra -O2 -std=gnu11 -Iinclude \
    -fno-builtin -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
    -fno-omit-frame-pointer -DZK_DEMO -c src/gui.c -o build/gui_demo.o
ld -m elf_i386 -T link.ld -nostdlib --oformat elf32-i386 \
   -o build/ziphuos_demo.bin $OBJS build/gui_demo.o

cd /home/z/my-project
bash scripts/build/demo_iso.sh   # -> kernel/build/ziphuos_demo.iso (~570 KB)
qemu-system-i386 -cdrom kernel/build/ziphuos_demo.iso -m 64M
```

### 7. Regenerating the fonts (only when the glyph set changes)

```bash
python3 scripts/build/gen_font.py      # IBM VGA 8x16 -> src/font8x16.c
python3 scripts/build/gen_cjk_font.py  # GNU Unifont 16x16 -> src/font_cjk.c
```

Then rebuild the kernel (`make`).

---

## Controls

### Keyboard (desktop)
- `j` / `k` — move through the apps list
- `Enter` — open the selected app
- `Esc` — back to the desktop

### Settings
- `1` — Light theme, `2` — Dark theme
- `e` — English, `z` — Chinese
- `s` — Crash System (watchdog → RSOD → reboot)
- `v` — Crash VMS (kernel hang)
- the RGB sliders tune the Custom accent with the mouse

### File Manager
- `h` / `l` — previous/next drive
- `j` / `k` — move through files
- `Enter` — enter a directory

### Mouse
- Cursor movement; left button — click on app rows, buttons and RGB sliders in Settings.

---

## Themes and Localisation

- **Default theme:** Dark. Toggled in Settings or with the `1`/`2` keys. A Custom theme inherits the Dark base and overrides the accent/frame with the chosen RGB.
- **Accent `#00E5E5`** is used consistently: window frames, the boot progress bar, selected rows and hover buttons.
- **Localisation:** parallel EN/ZH string tables; switching with `e`/`z` in Settings. CJK glyphs are loaded from `font_cjk.c`, ASCII from `font8x16.c`; unknown code points render as `?`.

---

## Screenshots

| File | What it shows |
|---|---|
| `screens/01_boot.png` | boot screen: Ziphu.OS wordmark + progress bar |
| `screens/02_desktop.png` | desktop: apps list + SYSTEM window (CPUID/VM/MEM) |
| `screens/03_settings.png` | Settings: theme/language + crash-test buttons |
| `screens/04_fileman.png` | File Manager: dual-pane drives | files |
| `screens/05_memlog.png` | MemLog: 256 rows of the memory map |
| `screens/06_syslog.png` | SysLog: kernel event log |
| `screens/07_about.png` | About |
| `screens/A_rgb_pink.png` | Custom Pink accent applied to the frames |
| `screens/B_zh_settings.png` | Settings in Chinese |
| `screens/C_desktop_shift.png` | the Shift layer in the desktop text field |
| `screens/D_desktop_zh.png` | the apps list in Chinese |

---

## Technical Notes

- **Freestanding:** the kernel uses no libc. `zk_utoa`, `zk_memset`, `zk_memcpy` are implemented in `util.c`; debug output is a direct write to port `0xE9`.
- **No SSE:** compiled with `-mno-sse -mno-mmx -mno-red-zone` — the kernel does not save FPU context and runs in a pure i386 legacy mode.
- **Back buffer:** in LFB32 mode every frame is composed in an off-screen buffer (allocated through `kmalloc`); `fb_present()` is the only path that writes the visible LFB; in VGA mode drawing is direct.
- **El Torito:** the ISO is registered via `xorriso -as mkisofs` with `-no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info`; the boot image is `cdboot.img + core.img`.
- **Image size:** the production ISO is ~566 KB, the demo ISO ~570 KB. The shrink was achieved by not copying 276 GRUB modules to disk, keeping the `core.img` module set minimal, and dropping Rock Ridge/Joliet.

# Ziphu.OS 1.0 — OpenZK（Open-ZiphuKernel）

**Ziphu.OS** 是一款面向 i386 架构的 32 位单体式 Hobby 操作系统，从零开始以 freestanding 内核（不依赖宿主 libc）的方式编写，通过 GRUB 按 Multiboot 1 协议加载。内核之上运行「闪电」（Молния）图形 shell：一套单色桌面、配以霓虹青绿色窗框，支持鼠标、主题、中英双语界面（EN/ZH）以及一组系统应用。

成品可启动 ISO 镜像仅 **~566 KB**，其中只包含内核（~52 KB）与一个内嵌配置、自包含的 GRUB core 镜像。

---

## 功能特性

### 内核
- **启动：** Multiboot 1 协议；内核按 `link.ld` 链接至物理地址 1 MiB。
- **体系结构：** GDT、IDT、PIC 重映射至向量 32/40，ISR 桩用汇编手写（`boot/isr.asm`）。
- **显示：** 主模式为 VBE/BGA 线性帧缓冲 640×480×32（通过 Bochs VGA 设备 1234:1111 的 PCI BAR0 发现 LFB）；若不存在则回退到标准 VGA mode 13h（256 色，2× 缩放）。双缓冲：所有绘制都写入离屏 back buffer，由 `fb_present()` 整帧拷出——无闪烁。
- **内存：** 依据 multiboot lower/upper mem 计账；内核镜像之上是一个 bump 堆（`mem.c`）。
- **中断：** 100 Hz 定时器（IRQ0）、键盘（IRQ1）、PS/2 aux 鼠标（IRQ12）。
- **CPUID：** 检测 vendor id 与 brand string（`cpuid.c`）。
- **VM：** 检测虚拟机管理程序是否存在（`vm.c`）。
- **磁盘：** 通过 PCI class 01/06 扫描探测 IDE 与 SATA；含 BPB/dirent/FAT 结构，可读取 FAT16/FAT32 cluster chain（`drive.c`）。
- **VFS：** 树状虚拟文件系统（`vfs_node_t`）；启动时挂载 root、共享目录 `/examples` 及被探测到的磁盘挂载点（`vfs.c`）。
- **容错：** 看门狗——卡死 5 秒后绘制 RSOD（Red Screen of Death）并重启；第二条崩溃测试分支把内核送入 `cli; hlt` 永久循环（`crash.c`、`rsod.c`、`timer.c`）。

### 「闪电」Shell
- **桌面：** 左侧为应用列表，右侧为 SYSTEM 窗口，实时显示 CPUID/VM/MEM 数值。
- **应用：**
  - **Settings（设置）**——主题（Light/Dark/自定义 RGB）、语言（EN/ZH）、Crash System 与 Crash VMS 按钮。
  - **File Manager（文件管理）**——双栏：磁盘 | 文件，按字节显示大小。
  - **MemLog（内存日志）**——256 行内存映射。
  - **SysLog（系统日志）**——内核事件日志。
  - **About（关于）**——系统信息。
- **主题：** 默认 Dark。调色板为严格的单色灰，仅以 `#00E5E5` 作为唯一强调色（窗框、启动进度条、选中行、按钮）。Custom 主题允许设置任意 RGB（每通道 0–255），强调色与窗框跟随所选颜色。
- **本地化：** 所有 UI 字符串以并行的 EN/ZH 表存放（`lstr_t { en, zh }`），由 `gui.c` 中的 `LANG_EN`/`LANG_ZH` 开关切换。ASCII 字体为原版 IBM VGA 8×16 ROM（由 Linux 内核 `lib/fonts/font_8x16.c` 生成）；16×16 CJK 字形由 GNU Unifont 生成，用于翻译后的标签。
- **鼠标：** PS/2 aux，三键 + 滚轮（Intellimouse），绝对光标位置，对按钮与应用行做命中测试。
- **键盘：** Shift 层——`1`→`!`、`;`→`:` 等；桌面上的文本框实时演示 Shift 层。

---

## 架构与源码结构

```
kernel/
  boot/entry.asm        Multiboot 头 + _start
  boot/isr.asm          ISR/IRQ 桩，load_gdt/load_idt
  link.ld               链接脚本（.multiboot 在前，镜像定位于 1M）
  include/              zk_types.h, zk_port.h, zk_util.h, zk_video.h, zk_core.h
  src/
    kmain.c             C 入口：顺序初始化各子系统
    idt.c               GDT/IDT/PIC + ISR 注册
    video.c             VBE/BGA + VGA 回退，绘图原语，fb_present
    mem.c               RAM 账务 + bump 堆
    timer.c             IRQ0 100 Hz + 看门狗到期路径（RSOD）
    keyboard.c          IRQ1，扫描码，Shift 层
    mouse.c             PS/2 aux，IRQ12，3/4 字节包解析
    cpuid.c             vendor + brand string
    vm.c                虚拟机管理程序检测
    drive.c             IDE/SATA 探测，FAT16/FAT32
    vfs.c               虚拟 FS，/examples，磁盘挂载
    gui.c               「闪电」shell：主题、窗口、应用、鼠标（约 1037 行）
    crash.c             crash_arm_watchdog / crash_vms_hang
    rsod.c              Red Screen of Death 绘制
    font8x16.c          ASCII 字体（生成）
    font_cjk.c          CJK 字体（生成）
    util.c              zk_utoa, zk_memset, dbg_puts（经端口 0xE9）
scripts/
  env.sh                为本地工具链设置 PATH/LD_LIBRARY_PATH
  build/iso.sh          生产 ISO 构建（grub-mkimage + xorriso）
  build/demo_iso.sh     demo ISO 构建（经 -DZK_DEMO 自动循环）
  build/gen_font.py     由 IBM VGA ROM 生成 8x16 ASCII 字体
  build/gen_cjk_font.py 由 GNU Unifont 生成 16x16 CJK 字体
  fetch_debs.sh         下载/解包 .deb 到本地 prefix（无需 root）
  capture_qemu.py       headless QEMU 截屏
```

代码库规模：约 3355 行（C + 汇编）。

---

## 构建依赖

裸机 i386 工具链：

- **gcc**，需支持 `-m32`（`gcc-multilib` 包）
- **nasm**（汇编器，elf32 格式）
- **ld**（binutils，`elf_i386` 模式）
- **xorriso**（ISO 打包）
- **grub-mkimage** + `grub-pc-bin` 模块（`/usr/lib/grub/i386-pc` 目录）
- **qemu-system-i386**（运行与测试）
- **python3** + **Pillow**（仅用于重新生成字体）

在本环境中，工具被本地安装在 `tools/` 下（经 `scripts/fetch_debs.sh` 从 `.deb` 解包，无需 root）。`scripts/env.sh` 脚本设置 `PATH`、`LD_LIBRARY_PATH` 与 `QEMU_BIOS_DIR`，使 `gcc`、`nasm`、`grub-mkimage`、`xorriso`、`qemu-system-i386` 都解析到本地二进制。

在普通 Ubuntu/Debian 上，一条命令即可装齐：

```bash
sudo apt install gcc-multilib nasm xorriso grub-pc-bin grub-common qemu-system-x86 python3-pil
```

---

## 如何编译

### 1. 获取源码

```bash
# 解压 ZIP，或直接在仓库中工作
unzip ZiphuOS-1.0-src.zip
cd my-project
```

### 2. 激活工具链

若工具为系统安装（apt 方案），可跳过此步。若使用 `tools/` 下的本地工具链：

```bash
source scripts/env.sh
```

之后 `which grub-mkimage` 应指向 `tools/iso/root/usr/bin/grub-mkimage`。

### 3. 构建内核

```bash
cd kernel
make            # -> build/ziphuos.bin（multiboot ELF，约 52 KB）
```

`make` 以 freestanding 标志（`-m32 -ffreestanding -fno-pie -fno-builtin -mno-sse -O2`）编译所有 `src/*.c`，用 nasm 汇编 `boot/*.asm`，再按 `link.ld` 链接为扁平 ELF32。

### 4. 构建可启动 ISO

```bash
cd /home/z/my-project
bash scripts/build/iso.sh     # -> kernel/build/ziphuos.iso（约 566 KB）
```

`iso.sh` 所做的事：
1. 仅把 `ziphuos.bin` 拷入 ISO 树（GRUB 模块**不**拷到磁盘——`core.img` 是自包含的）。
2. 生成内嵌菜单的 `embed.cfg` 并烘入 core 镜像。
3. `grub-mkimage` 用最小模块集 `biosdisk iso9660 multiboot boot` 构建 core 镜像。
4. El Torito cdboot 桩（`cdboot.img`）前置到 `core.img`——得到 `eltorito.img`。
5. `xorriso` 不带 Rock Ridge/Joliet 打包 El Torito ISO（GRUB 读原始 ISO9660，多余目录树只会撑大镜像）。

> **关于体积的说明：** `xorriso --no-pad` 标志可裁掉约 300 KB 尾部零填充，但在小 ISO 上 SeaBIOS 会错误加载内核 payload——内核启动了，但数据到达时已损坏，屏幕保持黑屏。因此尾部填充保留默认值；最终约 566 KB。

### 5. 运行

```bash
qemu-system-i386 -cdrom kernel/build/ziphuos.iso -m 64M
```

带截屏的 headless 测试：

```bash
qemu-system-i386 -cdrom kernel/build/ziphuos.iso -m 64M \
  -vga std -display none -monitor stdio -serial file:/tmp/ser.log \
  -debugcon file:/tmp/dbg.log
# 在 monitor 中：screendump /tmp/screen.ppm
```

内核调试输出走端口 0xE9（`-debugcon`）：`kmain entry`、`video kind`、`irq_count` 等日志。

### 6.（可选）带应用自动循环的 demo 构建

demo 变体（`-DZK_DEMO`）每 200 ms 自动在桌面切换所有应用——便于无需手动输入的视频/演示。

```bash
cd kernel
# 用 -DZK_DEMO 重新编译 gui.c 并链接 demo 内核
OBJS=$(ls build/*.o | grep -vE "gui(_demo)?\.o$" | tr '\n' ' ')
gcc -m32 -ffreestanding -fno-pie -fno-stack-protector \
    -fno-asynchronous-unwind-tables -Wall -Wextra -O2 -std=gnu11 -Iinclude \
    -fno-builtin -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
    -fno-omit-frame-pointer -DZK_DEMO -c src/gui.c -o build/gui_demo.o
ld -m elf_i386 -T link.ld -nostdlib --oformat elf32-i386 \
   -o build/ziphuos_demo.bin $OBJS build/gui_demo.o

cd /home/z/my-project
bash scripts/build/demo_iso.sh   # -> kernel/build/ziphuos_demo.iso（约 570 KB）
qemu-system-i386 -cdrom kernel/build/ziphuos_demo.iso -m 64M
```

### 7. 重新生成字体（仅当字形集合变化时）

```bash
python3 scripts/build/gen_font.py      # IBM VGA 8x16 -> src/font8x16.c
python3 scripts/build/gen_cjk_font.py  # GNU Unifont 16x16 -> src/font_cjk.c
```

随后重新构建内核（`make`）。

---

## 操作

### 键盘（桌面）
- `j` / `k`——在应用列表中移动
- `Enter`——打开选中应用
- `Esc`——返回桌面

### Settings（设置）
- `1`——Light 主题，`2`——Dark 主题
- `e`——英文，`z`——中文
- `s`——Crash System（看门狗 → RSOD → 重启）
- `v`——Crash VMS（内核卡死）
- RGB 滑块用鼠标调节 Custom 强调色

### File Manager（文件管理）
- `h` / `l`——上一个/下一个磁盘
- `j` / `k`——在文件中移动
- `Enter`——进入目录

### 鼠标
- 光标移动；左键——点击应用行、按钮以及 Settings 中的 RGB 滑块。

---

## 主题与本地化

- **默认主题：** Dark。可在 Settings 中切换，或用 `1`/`2` 键。Custom 主题继承 Dark 基底，并以所选 RGB 覆盖强调色/窗框。
- **强调色 `#00E5E5`** 一致使用：窗框、启动进度条、选中行与悬停按钮。
- **本地化：** 并行 EN/ZH 字符串表；在 Settings 中用 `e`/`z` 切换。CJK 字形取自 `font_cjk.c`，ASCII 取自 `font8x16.c`；未知码点渲染为 `?`。

---

## 截图

| 文件 | 内容 |
|---|---|
| `screens/01_boot.png` | 启动画面：Ziphu.OS 字标 + 进度条 |
| `screens/02_desktop.png` | 桌面：应用列表 + SYSTEM 窗口（CPUID/VM/MEM） |
| `screens/03_settings.png` | 设置：主题/语言 + 崩溃测试按钮 |
| `screens/04_fileman.png` | 文件管理：双栏 磁盘 | 文件 |
| `screens/05_memlog.png` | 内存日志：256 行内存映射 |
| `screens/06_syslog.png` | 系统日志：内核事件日志 |
| `screens/07_about.png` | 关于 |
| `screens/A_rgb_pink.png` | 自定义粉色强调色应用到窗框 |
| `screens/B_zh_settings.png` | 中文 Settings |
| `screens/C_desktop_shift.png` | 桌面文本框中的 Shift 层 |
| `screens/D_desktop_zh.png` | 中文应用列表 |

---

## 技术说明

- **Freestanding：** 内核不使用 libc。`zk_utoa`、`zk_memset`、`zk_memcpy` 在 `util.c` 中实现；调试输出直接写端口 `0xE9`。
- **无 SSE：** 以 `-mno-sse -mno-mmx -mno-red-zone` 编译——内核不保存 FPU 上下文，运行于纯 i386 legacy 模式。
- **Back buffer：** LFB32 模式下每帧都在离屏缓冲中合成（经 `kmalloc` 分配）；`fb_present()` 是唯一写可见 LFB 的路径；VGA 模式下为直写。
- **El Torito：** ISO 经 `xorriso -as mkisofs` 以 `-no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info` 注册；启动镜像为 `cdboot.img + core.img`。
- **镜像体积：** 生产 ISO 约 566 KB，demo ISO 约 570 KB。缩小靠的是不把 276 个 GRUB 模块拷到磁盘、保持 `core.img` 模块集最小化，以及放弃 Rock Ridge/Joliet。
