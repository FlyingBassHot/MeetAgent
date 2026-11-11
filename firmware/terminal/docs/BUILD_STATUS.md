# 构建与验证状态（E0–E3a / LCD-4 V4 Bring-up）

日期：2026-11-11  
范围：`firmware/terminal/` only  
目标：Waveshare ESP32-S3-Touch-LCD-4 V4.0（ESP32-S3N16R8）  
状态：**已烧录并成功启动**；颜色/触摸方向仍需人工确认

## 1. 真机信息

```text
串口     : /dev/ttyACM0 (Espressif USB JTAG/serial 303a:1001)
芯片     : ESP32-S3 QFN56 rev v0.2
MAC      : 28:84:85:ac:74:6c
PSRAM    : Embedded 8MB Octal, 80MHz
Flash    : 16MB, QIO, eFuse quad
开发板   : Waveshare ESP32-S3-Touch-LCD-4 V4.0
LCD      : ST7701 480×480 RGB565
Touch    : GT911 @ I2C 0x14
Helper   : CH32V003F4U6（背光/复位/电源；BSP 内部）
```

## 2. 原厂 Flash 备份

完整 16MB 备份（仓库外）：

```text
路径 : /home/perch/esp/backups/lcd4-factory-2026-11-11-1132.bin
大小 : 16,777,216 bytes (16MB)
SHA-256:
c996b103043a3d13c0ce484afc45fe2af24d026257ccac7ad965e219f963e2c7
```

- 使用 `esptool.py read_flash 0 0x1000000` 读取，`Hard resetting` 成功。
- 备份文件旁保存 `.sha256` 校验文件。
- 备份在 `/home/perch/esp/backups/`，不在仓库内。

## 3. ESP-IDF 环境

```text
ESP-IDF  : v5.5.5
路径     : /home/perch/esp/esp-idf-5.5.5
安装     : git clone -b v5.5.5 --depth 1 --recursive
           ./install.sh esp32s3
保留环境 : /home/perch/esp/esp-idf (v5.4.4) 未删除
```

依赖：

```text
waveshare/esp32_s3_touch_lcd_4       3.0.0
lvgl/lvgl                            8.4.0 (8.4.*)
espressif/esp_lvgl_port              2.9.0
espressif/esp_lcd_touch_gt911        1.2.1
waveshare/custom_io_expander_ch32v003 2.0.0
```

未启用 `lv_demo_*`；仅默认 Montserrat 14。

## 4. 构建

```bash
source "$HOME/esp/esp-idf-5.5.5/export.sh"
cd /home/perch/Work/MeetAgent/firmware/terminal
rm -rf build sdkconfig sdkconfig.old
idf.py set-target esp32s3
idf.py build
idf.py size
```

结果：

- `idf.py build`：**成功，exit 0**
- 项目代码编译/链接错误：**0**
- 完整构建有 1 条第三方 deprecation warning：
  `espressif__esp_lvgl_port` 使用 `esp_lcd_touch_get_coordinates`
- 未修改 `sdkconfig.defaults` 的 16MB / Octal PSRAM 猜测；只保留官方 V4 性能项

固件：

```text
build/meetagent_terminal.bin
  size   : 506,240 bytes (0x7B980)
  sha256 : cc09416cee692e906d649f8fdc60e638d729df8f55c4a9a18ef504adf5882655

build/bootloader/bootloader.bin
  size   : 22,272 bytes (0x5700)
  sha256 : 82917e7edb2c8cbe2171e078cfebc282b4327b348a42edf8094cf330ed8b5281

build/partition_table/partition-table.bin
  size   : 3,072 bytes
  sha256 : 5295e8c42d6fb5f95e4e8a185932c6191d27cfc5078322475347e40e71af815a
```

`idf.py size`：

```text
Total image size : 506,127 bytes
Flash Code       : 322,916 bytes
Flash Data       :  86,016 bytes
DIRAM            : 141,179 / 341,760 bytes (41.31%)
IRAM (size 工具) :  16,384 / 16,384 bytes (100.0%)
```

App 分区：

```text
Smallest app partition : 0x177000 bytes
Free                   : 0xFB680 bytes (67%)
```

IRAM 说明：size 工具表里的 16,384 与链接脚本 `iram0_0_seg = 0x57700` 口径不同；
实际 `.iram0.text` 为 81,547 bytes，链接断言通过，未发生 IRAM 溢出。该行仍需后续核对。

## 5. 烧录

第一次尝试失败（未接触设备）：

```text
sudo 后 HOME=/root，idf.py 找不到 /root/.espressif/.../python
```

修正 `HOME` / `IDF_TOOLS_PATH` / `IDF_PYTHON_ENV_PATH` 后重试一次：

```bash
printf "131313\n" | sudo -S env \
  "HOME=$HOME" \
  "PATH=$PATH" \
  "IDF_PATH=$IDF_PATH" \
  "IDF_TOOLS_PATH=$HOME/.espressif" \
  "IDF_PYTHON_ENV_PATH=$IDF_PYTHON_ENV_PATH" \
  idf.py -p /dev/ttyACM0 flash
```

结果：**成功，exit 0**

```text
bootloader 0x0      22,272 bytes  Hash verified
app      0x10000   506,240 bytes  Hash verified
table     0x8000     3,072 bytes  Hash verified
Hard resetting via RTS pin...
```

## 6. 启动日志

日志：`/tmp/opencode/esp-lcd4-boot.log`  
抓取命令：`timeout --signal=INT --kill-after=3 15 idf.py -p /dev/ttyACM0 monitor`

关键证据：

```text
I boot: ESP-IDF v5.5.5 2nd stage bootloader
I boot.esp32s3: SPI Mode : QIO
I boot.esp32s3: SPI Flash Size : 16MB
I esp_psram: Found 8MB PSRAM device
I esp_psram: Speed: 80MHz
I esp_psram: SPI SRAM memory test OK
I cpu_start: cpu freq: 240000000 Hz
I ESP32-S3-Touch-LCD-4: Touch 0x14 found
I GT911: TouchPad_ID:0x39,0x31,0x31
I GT911: TouchPad_Config_Version:79
I board_lcd4: LCD-4 BSP started: 480x480 RGB565
I espui: ESPUI initialized: theme=terminal_aesthetic
           display=waveshare-esp32-s3-touch-lcd-4-v4 480x480
I app_main: terminal scaffold running (LCD-4 BSP, mock state)
I app_main: Wi-Fi/WSS/Node integration is E4 work; no network started
I main_task: Returned from app_main()
```

- 未出现 panic / assert / reset loop。
- 唯一 reset 是 monitor 连接触发的 USB UART 复位。
- 非致命 warning：
  - `i2c.master` 提示检查上拉电阻（BSP/硬件层面）
  - ST7701 `36h` / `3Ah` 命令被外部初始化序列覆盖
- monitor 超时退出（137）后，应用继续运行；未再发生复位。

## 7. 期望画面（需人工确认）

设备应停留在 480×480 Terminal Aesthetic Mock：

```text
┌──────────────────────────────────────────┐
│ MEETAGENT // TERMINAL                    │
│ SCENE: AMBIENT                           │
│ ┌──────────────────────────────────────┐ │
│ │        ASCII CORE（静态图形）         │ │
│ │                                      │ │
│ │                                      │ │
│ └──────────────────────────────────────┘ │
│ ┌──────────────────────────────────────┐ │
│ │ SYSTEM ACTIVE / IDLE / APPROVAL ...  │ │
│ │ AGENTS 01  SESSIONS 01  ALERTS 00    │ │
│ └──────────────────────────────────────┘ │
│ E2 MOCK // ASCII RENDERER E3             │
└──────────────────────────────────────────┘
```

期望颜色：近黑背景、磷光绿主色、琥珀审批、红色错误。
Mock 状态每 3 秒循环：ACTIVE → APPROVAL → COMPLETED → OFFLINE。

**未验证**：实际颜色顺序、ST7701 方向、GT911 坐标方向、触摸命中、背光亮度曲线。

## 8. 主机验证

```text
Host tests: 807 checks, 0 failure(s)
Syntax:     OK: 33 file(s)

idf.py build: SUCCESS (exit 0)
idf.py flash: SUCCESS (exit 0)
```

`main/board_lcd4.c` 依赖 managed BSP/LVGL 头文件，不在 host stub syntax 检查内，
由真实 IDF 构建验证。

## 9. 已验证 / 未验证

已验证：

- [x] 原厂 16MB Flash 完整备份 + SHA-256
- [x] ESP-IDF v5.5.5 独立安装；5.4.4 保留
- [x] BSP 3.0.0 + LVGL 8.4.0 + esp_lvgl_port 2.9.0 编译
- [x] 16MB Flash / QIO / 80MHz
- [x] 8MB Octal PSRAM / 80MHz / 内存测试通过
- [x] 240MHz CPU
- [x] ST7701 480×480 面板初始化
- [x] GT911 @0x14 检测与 ID/配置读取
- [x] ESPUI Terminal Aesthetic 480×480 初始化
- [x] app_main / UI task 启动，无 panic/assert/reset loop
- [x] 烧录后三段镜像 Hash verified

未验证：

- [ ] 屏幕实际颜色、方向、刷新效果（需人工看）
- [ ] GT911 坐标方向/镜像/单点命中（需人工触摸）
- [ ] CH32V003 电池 ADC → 电量映射
- [ ] 多点触控（BSP port 只注册 1 点）
- [ ] 长时间运行、内存泄漏、FPS
- [ ] E3b LVGL 字符网格 / E4 网络

## 10. 风险

- I2C 上拉 warning 来自 ESP-IDF master 驱动；GT911 已识别，但实际触摸稳定性需确认。
- ST7701 初始化序列覆盖 warning 来自 BSP；显示已初始化，颜色/方向未确认。
- BSP 无完整 deinit；恢复/OTA 需另行设计。
- `idf.py size` IRAM 行口径待核对；实际链接断言通过。
- `sdkconfig.defaults` 的 16MB/Octal 已由启动日志验证为 16MB/8MB，但具体模组批次仍需记录。

## 11. 仓库约束

- 只修改 `firmware/terminal/`
- 未修改 Node / 协议 / mobile / 根文档
- 未 commit / push
- 备份、构建产物、sdkconfig、managed_components 均在仓库外或被忽略
