# 构建与验证状态（E0–E3a / LCD-4 V4 Bring-up）

日期：2026-11-11  
范围：`firmware/terminal/` only  
目标：Waveshare ESP32-S3-Touch-LCD-4 V4.0（ESP32-S3N16R8）  
状态：**E4a Wi-Fi/STA/Node 已验证**；180° 旋转已修正；屏幕颜色/触摸命中仍需人工确认

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
  size   : 1,212,272 bytes (0x127F70)
  sha256 : 0183deddbe2cf84d7bed35f8221fcc74358e7b1f657b9cc17aeff4bfa30d157d

build/bootloader/bootloader.bin
  size   : 22,272 bytes (0x5700)
  sha256 : 82917e7edb2c8cbe2171e078cfebc282b4327b348a42edf8094cf330ed8b5281

build/partition_table/partition-table.bin
  size   : 3,072 bytes
  sha256 : 5295e8c42d6fb5f95e4e8a185932c6191d27cfc5078322475347e40e71af815a
```

`idf.py size`：

```text
Total image size : 1,212,151 bytes
Flash Code       : 900,468 bytes
Flash Data       : 175,236 bytes
DIRAM            : 195,895 / 341,760 bytes (57.32%)
IRAM (size 工具) :  16,384 / 16,384 bytes (100.0%)
```

App 分区：

```text
Smallest app partition : 0x177000 bytes
Free                   : 0x4F090 bytes (21%)
```

IRAM 说明：size 工具表里的 16,384 与链接脚本 `iram0_0_seg = 0x57700` 口径不同；
实际 `.iram0.text` 远小于 `iram0_0_seg`，链接断言通过，未发生 IRAM 溢出。该行仍需后续核对。

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

180° 旋转修正后再次烧录成功：

```text
app      0x10000   506,928 bytes  Hash verified
日志     : /tmp/opencode/esp-lcd4-rotate-boot.log
```

## 6. 启动日志

日志：`/tmp/opencode/esp-lcd4-rotate-boot.log`（旋转修正后）  
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
I board_lcd4: display rotated 180 degrees (touch transform follows LVGL indev)
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

### 180° 旋转修正

- 位置：`main/board_lcd4.c`，`bsp_display_start()` 成功后调用
  `bsp_display_rotate(s_display, LV_DISP_ROT_180)`（持 `bsp_display_lock`）。
- 触摸：LVGL 8.4 `indev_pointer_proc()` 对 `LV_DISP_ROT_180` 自动执行
  `x = hor_res - x - 1`、`y = ver_res - y - 1`；HAL 读取的是 indev 变换后的点。
- 启动日志已出现 `display rotated 180 degrees`，无 panic/assert。
- 实际屏幕方向与触摸命中仍需人工确认。

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
- [x] 180° 显示旋转调用已生效（日志确认），触摸变换走 LVGL indev
- [x] 烧录后三段镜像 Hash verified

未验证：

- [ ] 屏幕实际颜色、旋转后方向、刷新效果（需人工看）
- [ ] GT911 旋转后坐标方向/镜像/单点命中（需人工触摸）
- [ ] CH32V003 电池 ADC → 电量映射
- [ ] 多点触控（BSP port 只注册 1 点）
- [ ] 长时间运行、内存泄漏、FPS
- [ ] E3b LVGL 字符网格 / E4b WebSocket/pairing/snapshot

## 10. 风险

- I2C 上拉 warning 来自 ESP-IDF master 驱动；GT911 已识别，但实际触摸稳定性需确认。
- ST7701 初始化序列覆盖 warning 来自 BSP；显示已初始化，颜色/方向未确认。
- BSP 无完整 deinit；恢复/OTA 需另行设计。
- `idf.py size` IRAM 行口径待核对；实际链接断言通过。
- `sdkconfig.defaults` 的 16MB/Octal 已由启动日志验证为 16MB/8MB，但具体模组批次仍需记录。

## 11. E4a Wi-Fi provisioning（2026-11-11）

### 11.1 实现

- 新增 `components/networking/wifi_prov.c/.h`
- NVS namespace `meet_wifi`，keys `wifi_ssid` / `wifi_pass` / `node_url` / `pairing_code`
- 无配置：开放 SoftAP `MeetAgent-<MAC后4位>` + HTTP portal `192.168.4.1:80`
- 有配置：STA 最多 3 次 × 15s，失败回退 SoftAP
- STA 成功后启动 `:80 /status`，GET `<node_url>/api/v2/info`
- 断线监控：30s 轮询 IP，断线后重连；重连 3 次失败才回退 AP
- `/save` 在 STA 返回 403，仅 SoftAP 可配网
- 不打印 Wi-Fi 密码/pairing code；`/status` 不返回密码/token

### 11.2 构建/烧录

```text
ESP-IDF : v5.5.5
app bin : 1,212,272 bytes (0x127F70)
sha256  : 0183deddbe2cf84d7bed35f8221fcc74358e7b1f657b9cc17aeff4bfa30d157d
Total image size : 1,212,151 bytes
Flash Code       : 900,468 bytes
Flash Data       : 175,236 bytes
DIRAM            : 195,895 / 341,760 bytes (57.32%)
App partition    : 0x177000, free 0x4F090 (21%)
```

- `idf.py build` 成功，无项目 warning/error
- 先 NVS 备份：`/tmp/opencode/nvs-backup.bin`，24,576 bytes，
  SHA-256 `bff03118be311fd9574a34b5f294a62b81126c97b9266a6e30d3a17b7fa549f1`
- 生成 NVS 镜像后只写 `0x9000`；随后 `idf.py app-flash` 写 `0x10000`
- 未 erase-flash，未动分区表，原厂 16MB 备份未动

### 11.3 STA / Node 验证

验证方式：NVS 预置（主机全程保持手机热点 172.20.10.5，未切换 Wi-Fi）。

```text
ESP32 STA IP: 172.20.10.2
Node info   : developer-machine / 2.1.0 / protocol=1 / schema=1
```

ESP32 串口：

```text
wifi_prov: config: has_config=1 ssid='...' node_url='http://172.20.10.5:8787' ...
wifi_prov: STA got IP: 172.20.10.2
wifi_prov: Node reachable: name=developer-machine version=2.1.0 protocol=1 schema=1
```

主机验证：

```json
{"wifi_state":"connected","ip":"172.20.10.2","node_url":"http://172.20.10.5:8787","has_config":true,"node_reachable":true,"node_name":"developer-machine","node_version":"2.1.0","protocol_version":1,"schema_version":1}
```

- `/status` 无 password/pairing/token 字段
- STA `POST /save` → 403
- 重启（monitor DTR hard reset）后自动从 NVS 重连，再次 Node 可达
- 主机未使用校园网 SSID（需网页认证），未执行 nmcli 网络切换（收到约束后）
- ufw 已由用户放行 `172.20.10.0/28 → TCP 8787`

### 11.4 Portal 状态

- 在主机网络约束前，曾短暂连接 AP 读取 `/` 与 `/status`；**未提交 `/save`**，随后立即恢复手机热点。
- 最终 STA/Node 验证全部使用 NVS 预置，未再切换主机网络。
- AP portal `POST /save` 实机保存未验证；第二台设备步骤见 `docs/NETWORK.md`。

### 11.5 未验证 / 风险

- [ ] AP portal `/save` 实机保存（需第二台设备）
- [ ] 强制断线重连（代码有 30s 监控 + 重连循环；未主动断开热点）
- [ ] DHCP 短租约续租（lwIP 自动续租；未等租约到期）
- [ ] NVS/Flash 加密；当前密码以 NVS 明文保存
- [ ] 开放 AP 仅用于配网；若 STA 连续失败会保持 AP
- [ ] E4b WebSocket/pairing/snapshot

## 12. 仓库约束

- 只修改 `firmware/terminal/`
- 未修改 Node / 协议 / mobile / 根文档
- 未 commit / push
- 备份、构建产物、sdkconfig、managed_components 均在仓库外或被忽略
