# 构建与验证状态（E0–E3a / LCD-4 V4 Bring-up）

日期：2026-11-11  
范围：`firmware/terminal/` only  
目标：Waveshare ESP32-S3-Touch-LCD-4 V4.0（ESP32-S3N16R8）  
状态：**E4a Wi-Fi + E4b-1 WebSocket + E4b-2 事件 apply/replay + E4b-3 安全命令链路已验证；GT911 触摸已确认**；屏幕颜色/背光仍需人工确认

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
  size   : 1,246,640 bytes (0x1305B0)
  sha256 : b7c12bd46218bc394792ffe46cebd1b4d8523fbcab8ec61df2d5a292eb375d0e

build/bootloader/bootloader.bin
  size   : 22,272 bytes (0x5700)
  sha256 : 82917e7edb2c8cbe2171e078cfebc282b4327b348a42edf8094cf330ed8b5281

build/partition_table/partition-table.bin
  size   : 3,072 bytes
  sha256 : 5295e8c42d6fb5f95e4e8a185932c6191d27cfc5078322475347e40e71af815a
```

`idf.py size`：

```text
Total image size : 1,246,519 bytes
Flash Code       : 925,412 bytes
Flash Data       : 184,660 bytes
DIRAM            : 199,431 / 341,760 bytes (58.35%)
IRAM (size 工具) :  16,384 / 16,384 bytes (100.0%)
```

App 分区：

```text
Smallest app partition : 0x177000 bytes
Free                   : 0x46A50 bytes (19%)
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
- 屏幕方向/触摸已由用户实机确认；颜色/背光仍需人工确认。

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

**待确认**：实际颜色顺序、背光亮度曲线；触摸/180° 已实机确认。

## 8. 主机验证

```text
Host tests: 807 checks, 0 failure(s)
Syntax:     OK: 32 file(s)

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
- [x] GT911 触摸事件/坐标实机确认（TL/TR/BL/BR/C + 180°）；临时 overlay 已移除
- [x] ESPUI Terminal Aesthetic 480×480 初始化
- [x] app_main / UI task 启动，无 panic/assert/reset loop
- [x] 180° 显示旋转调用已生效（日志确认），触摸变换走 LVGL indev
- [x] 烧录后三段镜像 Hash verified
- [x] E4b-1 WebSocket pairing/token/snapshot/heartbeat/reconnect

未验证：

- [ ] 屏幕实际颜色、刷新效果（方向/触摸已确认，颜色需人工看）
- [ ] 生产 UI 触摸交互（当前正式固件不轮询 touch HAL，Mock UI 无可点击控件）
- [ ] CH32V003 电池 ADC → 电量映射
- [ ] 多点触控（BSP port 只注册 1 点）
- [ ] 长时间运行、内存泄漏、FPS
- [x] E4b-2 event apply / replay / sequence 去重
- [x] E4b-3 安全命令链路（session.list completed + action result）
- [ ] E3b LVGL 字符网格 / interrupt/permission 真机链路

## 10. 风险

- I2C 上拉 warning 来自 ESP-IDF master 驱动；GT911 触摸已实机确认，长期稳定性仍需观察。
- E4b-1 token/pairing code 存 NVS 明文；生产前需 NVS/Flash encryption。
- ST7701 初始化序列覆盖 warning 来自 BSP；显示已初始化，方向已确认，颜色未校准。
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
sha256  : 8d416686f14fe37367759687078d236f54827ecda871df210113e7e03fcca60c
Total image size : 1,246,519 bytes
Flash Code       : 925,412 bytes
Flash Data       : 184,660 bytes
DIRAM            : 199,431 / 341,760 bytes (58.35%)
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

### 11.6 Touch 验收（2026-11-11）

临时 Touch Test overlay（已移除）：

```text
测试固件 hash: 76de71fdfd5d6371e6498486eee4861a4bb825262ea0ba5f00fecbe757495752
正式固件 hash: 8d416686f14fe37367759687078d236f54827ecda871df210113e7e03fcca60c
测试日志     : /tmp/opencode/touch-test-boot.log
恢复日志     : /tmp/opencode/touch-revert-boot.log
```

实机证据：

```text
I (26117) touch_test: TOUCH x=46 y=125
I (26244) touch_test: TOUCH release
```

- `x=46 y=125` 落在 TL 标记范围（x=12..76, y=104..140）。
- 用户人工确认 TL/TR/BL/BR/C 五个标记均跟随手指，180° 映射正确。
- 测试 overlay 已删除：`main/touch_test.c/.h`、app_main 调用、CMake 条目。
- 正式 E4a 固件已 app-flash 恢复；启动日志无 `touch_test`、无 panic。
- Wi-Fi 仍连接手机热点（STA 172.20.10.2），Node `developer-machine/2.1.0` 可达。
- 注意：正式固件目前仍只在启动时 `touch->init()`，生产 Mock UI 不轮询 touch HAL，
  也没有可点击控件；触摸交互待 UI 功能接入。

### 11.7 E4b-1 WebSocket（2026-11-11）

- 依赖：`espressif/esp_websocket_client` 1.8.0
- build/app hash：`b7c12bd4...eb375d0e`，1,246,640 bytes
- NVS pre-E4b backup：`/tmp/opencode/nvs-pre-e4b.bin`（24,576 B，
  SHA-256 `e5159daf...eb0a241`）
- 首次 pairing：`e4b1-firstpair2.log`
  - `auth: token=empty pairing_code=set last_sequence=0`
  - `hello: auth=pairing_code last_sequence=0`
  - welcome 50,341 bytes，parse 10ms，events_skipped=100
  - client_id=cli_b8aaf428f6ad3073
  - sessions 8/42，permissions=session.read/session.control/permission.respond
  - pending=0，seq=1411
  - internal_free=65,895 B，psram_free=6,516,920 B
  - heartbeat RTT=59ms
- token 重启：`e4b1-token-reboot.log`
  - `auth: token=set`，hello token，welcome 50,287 B，parse 10ms
  - 无需 pairing_code
- Node 重启：
  - WS error/disconnected → reconnect in 1s
  - token 仍有效，welcome 32,615 B，parse 9ms，events_skipped=45
  - `replay_truncated=1`，snapshot seq=45 已应用
  - heartbeat RTT=53ms
- `/status` 仍为 connected / node_reachable=true；STA 172.20.10.2
- 无 panic/assert/OOM
- E4b-1 只观察 event；`send_action` 未实现，只日志一次
- E4b-2 已补齐 event apply/replay；command gateway 在 E4b-3 实现，interrupt/permission 真机与无效 token 重试留待 E4b-4

### 11.8 E4b-1 review fixes（2026-11-11）

- `process_event()` 改从 `root.payload` 读 `type/sequence/agent_id/session_id/event_id`；
  真机日志已显示真实事件类型与序号（如 `message.delta seq=120`）。
- `process_error()` 改从 `payload` 读 `code/message`；临时 bogus token（无 pairing）
  实测：
  ```text
  server error code=auth_failed message=unknown or revoked client token
  token authentication failed; token cleared
  no pairing_code available; backoff
  ```
- `wait_for_wifi()` 只在首次进入时设置 `WIFI_CONNECTING`；真机日志只有
  wifi_connecting → node_connecting → online 三次状态回调，无每秒刷屏。
- `meet_net_client_stop()` 改为只请求停止并唤醒；WS/buffer/event group 均由
  `net_task` 清理，等待最多 2s，不再从调用者任务调用 `cleanup_ws()`。
- 修正后 app hash：`b7c12bd4...eb375d0e`，1,246,640 bytes。
- 临时 auth 测试使用 bogus token，测试后从
  `/tmp/opencode/nvs-e4b1-valid-token.bin`（24,576 B，
  SHA-256 `66464b51...aa295dd`）恢复真实 token；token welcome + heartbeat
  52/51ms 正常。
- 无 panic/assert/OOM；主机网络未切换。

### 11.9 E4b-2 事件 apply / replay（2026-11-11）

- State Adapter 实现真实 `apply_event`：sequence 去重、agent/session/permission
  映射、`tool.failed`/`message.completed` 计数（上限 99）、未知事件只推进序号。
- 事件 payload 用无分配 JSON 字符串提取器读取；`components/state` 仍不依赖
  cJSON。ViewModel 增加 transient error/completion 计数。
- `process_event()` 解析内层 payload → apply → 250ms/4Hz 脏标记 post；
  `message.delta` 不 post；sequence gap 告警；`s_last_sequence` 与 adapter
  同步，断开时写 NVS。
- Snapshot session active-first 截断，仍为 8 条总容量；`sessions_total` 保留
  42 条统计。
- 主机测试：`make test` **974 checks, 0 failures**；`syntax_check.sh` 32 files OK。
- 最终固件：1,250,736 B，SHA-256
  `084a8210...029b9270`，app 剩余 19%，DIRAM 58.36%。
  （事件计时日志来自功能等价的 `487b117a...` 构建；后续仅改日志文案与
  并发锁，最终构建启动/事件验证 `last_sequence=114`、snapshot seq=114。）
- 真机实时事件（`/tmp/e4b2-final.log`）：
  - boot `last_sequence=52`，welcome 35,203 B / 11 ms / 52 events；
  - snapshot 1 agent / 8 of 42 sessions / seq 52；
  - seq 53–59 apply：agent/session 状态、`message.completed`，
    `active 1→0`，`alerts=1`，`took_us` 102–301；
  - internal_free=65,763 B，psram_free=6,516,232 B；heartbeat 77/131 ms。
- Node 重启（`/tmp/e4b2-node-restart.log`）：`replay_truncated=1`，
  welcome 32,615 B / 9 ms / 45 events，seq 243→45，后续 seq 46–52 apply；
  无 panic/assert/OOM。
- 未做：真机 permission 审批、4Hz 高频压力、主动 sequence gap、心跳 3 次
  失联、无效 token + pairing retry、多 Agent 全局 active-first、UI 人工视觉确认。
- E4b-3 已实现安全命令发送；`send_action` 不支持 session.prompt，interrupt/permission 真机未验证。

### 11.10 E4b-2 并发修正（2026-11-11 review）

- 问题：WS 回调（welcome/event）与 heartbeat 的 post 同时访问
  `s_model` / `s_model_dirty` / `s_last_post_ms`，存在数据竞争。
- 修正：新增 `s_model_lock`（start 创建、cleanup 删除）；
  `post_model_locked()` 需持锁，`post_model_if_due()` 取锁后调用；
  `process_welcome` / `process_event` 在 apply + dirty + post + 计数期间持锁；
  heartbeat 通过 wrapper 取锁。Snapshot post 失败改由 dirty + heartbeat 重试。
- 生命周期：`net_task_cleanup()` 先释放 `s_lifecycle_lock` 再置
  `s_task = NULL`，避免 stop 在 cleanup 持锁时删除 lifecycle mutex。
- 最终固件：1,250,736 B，SHA-256 `084a8210...029b9270`，app 剩余 19%，
  DIRAM 58.36%。
- 验证 `/tmp/opencode/e4b2fix-verify.log`：welcome 54,301 B / 11 ms，
  snapshot 8/42 seq=114，事件 115–121 apply，`took_us` 99–341，
  heartbeat 62/13 ms，无 panic/OOM。
- Host：`make test` 974 checks；`syntax_check.sh` 32 files OK。
- stop 竞态为代码审查修正，尚无真机 stop 入口压测。

### 11.11 E4b-3 命令/审批发送（2026-11-11）

- 新增纯逻辑 `command_table.c`：固定 4 条 pending command，支持 add/take、
  15s 超时（wrap-safe）和断线 drain。
- `net_client` 新增 `s_cmd_lock`，在 start 创建、cleanup 删除；命令 result 匹配
  command_id，映射 completed/rejected/timeout/failed 到 ESPUI action result。
- `send_action` 支持 interrupt / permission.respond / refresh(session.list)；
  session/agent 只在 `s_model_lock` 内复制，释放后 build/send。不支持 prompt。
- heartbeat 250ms tick 检查超时；WS 断开把未完成命令标记 FAILED connection
  lost，不跨重连重试；app_main 回调 `espui_post_action_result()`。
- 主机测试：**1024 checks, 0 failures**；syntax **33 files OK**。
- 最终固件：1,255,696 B，SHA-256
  `84374daa...b7891aac`，app 剩余 18%，DIRAM 200,247 B (58.59%)。
- 临时诊断构建（hash `6448647b...`）发送一次 REFRESH_STATE：
  `command result ... type=session.list status=completed sessions=42`，
  app action result status=OK；诊断已移除，最终固件无触发逻辑。
- 最终回归：welcome 49,958 B/10ms，snapshot 8/42 seq=218，
  event #219–225 apply，heartbeat 61/12ms，无 panic/OOM。
- 未验证：interrupt 真机、permission.respond 真机、超时/断线真实网络故障、
  footer 视觉截图；均未强行构造或删除用户 session。

### 11.12 PHOSPHOR P0/P1 静态 Core（2026-11-11）

- P0 审计：IDF 5.5.5 / LVGL 8.4.0 / BSP 3.0.0 / esp_lvgl_port 2.9.0；
  480×480 RGB565，180° 旋转与 GT911 路径不变；审计见 `docs/PHOSPHOR_AUDIT.md`。
- P1 纯模块：`components/espui/phosphor/`，36×24 Glyph Frame、三轨
  Orbit、Nucleus `>_`、2 Satellite、8×12 1bpp atlas、Palette、Rasterizer。
- PC SDL2：`tools/phosphor_sim/`；截图 `docs/phosphor/p1_static.png`
  （480×480，SHA-256 `9f998079...f34f59`）。
- ESP32：`phosphor_lvgl_port.c` 在 `espui_start()` 后创建 full-screen
  top-layer overlay + 288×288 PSRAM canvas（165,888 B，一次分配），
  使用 display lock；旧 terminal_aesthetic 保留 fallback。
- 最终固件：1,264,656 B，SHA-256
  `0b0dae60...e7199`，app 剩余 18%，DIRAM 58.6%。
- 真机启动：`P1 static canvas ready`；internal_free=64,835 B，
  psram_free=6,345,252 B；snapshot 8/42；heartbeat 195ms；
  无 panic/assert/OOM。
- Host：`make test` **1526 checks**；syntax **41 files OK**。
- P1 只静态；动画/状态切换/性能测量未开始，P2 待办见 `docs/PHOSPHOR_P1.md`。
- 实屏颜色/可读性/中心位置需用户确认。

### 11.13 PHOSPHOR P1.1 可读性迭代（2026-11-11）

- 实机反馈：P1.0 字符太小、三条轨道挤在中央。
- P1.1 保留 36×24 网格：cell 12×18、canvas 432×432、位置 (24,24)，
  canvas 中心局部 (216,216)；atlas 重新生成 12×18（JetBrains Mono OFL）。
- Orbit 半径 78/126/168；去掉 halo，源 64 点 + DDA 连续段；Inner `.,:`
  Secondary，Middle `-=/\` Primary，Outer `+*#` Secondary/Highlight，
  Warning 处保留缺口。
- Core shell 改为稀疏 `:`/`.`；ambient radial field 固定 14 点、Muted 低亮度；
  `>_` 保留 1× 2-cell，最后绘制，未做 2×。
- PC 新截图 `docs/phosphor/p1_static_v2.png`（480×480，SHA-256
  `fb1362ff...adb9`）；旧 `p1_static.png` 保留对比。
- 最终固件：1,265,600 B，SHA-256
  `d6659e69...4459a2`，app 剩余 18%，DIRAM 58.6%。
- 真机：`P1 static canvas ready: 432x432 at (24,24), PSRAM=373248 bytes`；
  internal_free=64,819 B，psram_free=6,135,800 B；snapshot 8/42；
  heartbeat 56ms；无 panic/OOM。
- Host：**1733 checks, 0 failures**；syntax **41 files OK**。
- 实屏颜色/可读性/轨道分离度仍待用户确认；P2 未开始。

### 11.14 PHOSPHOR P1.2 字体可读性（2026-11-11）

- 实机反馈：字体细、小、不明显；分布不是主要问题。
- P1.2 保留 432×432 / (24,24)：grid 24×16，cell 18×27；
  atlas 改为 JetBrains Mono Bold 18×27，threshold 45% + 1px dilation；
  可复现脚本 `tools/phosphor_atlas/generate_glyph_atlas.py`。
- Nucleus 2×（4×2 cells，origin col10/12 row7），rasterizer 第二 pass
  清空 footprint 后绘制；中心 bbox (180..252,189..243)，中心 216,216。
- Inner `.,:;` Secondary；Middle `-=/\` Primary；Outer `+#%@` Primary；
  Satellite `*` Highlight / `?` Warning；主轨道 intensity 240–255。
- 新截图 `docs/phosphor/p1_static_v3.png`（480×480，2,946 B，
  SHA-256 `94f6b6d9...1d1965`）。
- 最终固件：1,267,312 B，SHA-256 `5c2bf6af...81f2b5`，app 剩余 17%，
  DIRAM 58.6%。
- 真机：432×432 PSRAM=373,248 B；internal_free=64,803 B，
  psram_free=6,134,568 B；snapshot 8/42；heartbeat 97ms；无 panic/OOM。
- Host：**2007 checks, 0 failures**；syntax **41 files OK**。
- 实屏笔画/颜色/轨道分离度待用户确认；P2 未开始。

### 11.15 PHOSPHOR P1.3 轨道可辨性（2026-11-11）

- 保持 24×16 / 18×27 Bold / 432×432 / (24,24)；不缩小字体。
- Orbit 半径 80/140/192；tilt 1.00/0.25/1.30；rotation -0.55/0.85/-0.65；
  phase 0.10/1.90/3.80。
- Inner Secondary/Muted 后侧（≥185），Middle Secondary 235，
  Outer Primary 250；Satellite `*` Highlight / `?` Warning。
- Middle 新增约 5.85 rad 缺口（半宽 0.23），Outer warning 缺口保留；Inner 完整。
- 移除 ambient radial field；无 halo；后侧不再整段切 Muted/125。
- 新截图 `docs/phosphor/p1_static_v4.png`（480×480，3,002 B，
  SHA-256 `b581b0ff...76fc`）。
- 最终固件：1,267,232 B，SHA-256 `b9156df1...0ef5`，app 剩余 17%，
  DIRAM 58.6%。
- 真机：432×432 PSRAM=373,248 B；internal_free=64,771 B，
  psram_free=6,134,568 B；snapshot 8/42；heartbeat 53ms；无 panic/OOM。
- Host：**2013 checks, 0 failures**；syntax **41 files OK**。
- 实屏轨道分离度/颜色仍待用户确认；P2 未开始。

### 11.16 PHOSPHOR P2 Glyph Renderer / P2 Static（2026-11-11）

- 新增 `glyph_ramp.c/.h`：`" .,:-=+*#%@"`，intensity 0..255 单调映射；
- P2 core density：中心半径 30–78，`smooth_falloff` + ramp，FIELD，
  不进 inner orbit，不形成 halo；无 ambient 随机点。
- Palette 8 个 RGB565 精确值 + scale565 单调；`LV_COLOR_16_SWAP=0`
  编译期检查。
- `glyph_frame` 优先级 NUCLEUS > SATELLITE > FRONT > BACK > FIELD；
  Rasterizer 普通 cell + Nucleus 2× second pass + 裁剪。
- 新截图 `docs/phosphor/p2_static.png`（480×480，2,873 B，
  SHA-256 `9d9fc20a...64117`）。
- 最终固件：1,267,472 B，SHA-256 `f18b6350...f10f34`，app 剩余 17%，
  DIRAM 58.6%。
- 真机：432×432 PSRAM=373,248；internal_free=62,887 B，
  psram_free=6,134,552 B；snapshot 8/42；heartbeat 101ms；无 panic/OOM；
  Wi-Fi/Node 在线。
- Host：**3541 checks, 0 failures**；syntax **42 files OK**。
- 实屏密度层次/颜色待用户确认；P3 未开始。

### 11.17 PHOSPHOR P3 Idle/Working（2026-11-11）

- 新增纯 `phosphor_motion` / `phosphor_lut`：Idle +0.08/-0.045/+0.025，
  Working +0.22/-0.55/+0.32；1s 过渡、450ms energy 低通、1024 点 LUT。
- Scene 动态相位/密度/亮度；Working 固定相位活动粒子（`+`/`%`），
  Satellite 位置固定；Nucleus 2× 稳定。
- ESP32 单刷新任务，demo 6s Idle ↔ 6s Working；display lock 内更新
  PSRAM canvas + invalidate，不 scene_commit。
- 实测：Idle 7 FPS（目标 6–8 达成）；Working 7 FPS（目标 10–15 未达成，
  432 canvas 全量渲染/刷新限制；P5 再优化，不虚报）。
- Host：**3613 checks, 0 failures**；syntax **45 files OK**。
- 截图：`docs/phosphor/p3_idle.png`、`p3_working.png`（480×480）。
- 最终固件：1,271,872 B，SHA-256 `b7c7bec7...777f8`，app 剩余 17%，
  DIRAM 204,423/341,760 (59.81%)。
- 真机 Wi-Fi/Node 在线，无 panic/OOM；实屏动画待用户确认。
- P4 未开始。

### 11.18 PHOSPHOR P3.1 二进制流/密度（2026-11-11）

- atlas 增加 `0/1`（29 glyphs，18×27 Bold+dilate，2,349 B）；
- Outer 采样 64→96；字符 `+ # % @ 0 1`；0/1 由 index+time hash
  确定性选择，Working 300ms bucket（≈3.3Hz，45%），Idle 800ms（12%）；
- 外围 4–8 个不规则稀疏 0/1 点（162–198 annulus，field 低亮度，无 halo）；
- Working speeds +0.25/-0.62/+0.36（约 +13%），density 0.85；
- 新截图 `p3_idle_v2.png` / `p3_working_v2.png`；
- 最终固件：1,272,736 B，SHA-256 `c487e15b...05e241b`，app 剩余 17%，
  DIRAM 204,423/341,760 (59.81%)。
- 真机：internal_free 45,255→65,135 B，PSRAM ~6,133,260 B；
  Idle 6–7 FPS，Working 7 FPS（仍低于 10–15 目标，P5 优化）；
  Wi-Fi/Node 在线；无 panic/OOM。
- Host：**3619 checks, 0 failures**；syntax **45 files OK**。
- P4 未开始。

### 11.19 PHOSPHOR P3.2 外圈副带/Inner 密度（2026-11-11）

- 删除零散外围 0/1 点；改为 Outer 连续内侧副带：半径 176、96 采样、
  同 tilt/rotation/phase、warning 缺口；字符 `0/1/+/%`，
  FIELD 优先级，Idle 135 / Working 195 强度，不覆盖主轨。
- Inner 半径 80，采样 64→96；字符 `: ;` 为主、少量 `+`，强度 200；
  Middle 不变。
- 主 Outer 仍 96 采样、`+#%@01`、Working 300ms bucket。
- 新截图 `p3_idle_v3.png` / `p3_working_v3.png`。
- 最终固件：1,273,152 B，SHA-256 `fec42554...e6573`，app 剩余 17%，
  DIRAM 204,423/341,760 (59.81%)。
- 真机：internal_free 45,207→65,099 B，PSRAM ~6,133,324 B；
  Idle 6–7 FPS，Working 7 FPS（仍低于 10–15 目标，P5 优化）；
  无 panic/OOM，Wi-Fi/Node 在线。
- Host：**3620 checks, 0 failures**；syntax **45 files OK**。
- P4 未开始。

### 11.20 PHOSPHOR P3.3 相邻 cell 加厚（2026-11-11）

- 删除外围零散点与 radius-176 副带；Outer 对每个主 cell 写朝中心相邻 cell
  （失败尝试垂直候选），Inner 写朝外相邻 cell；Inner 最后绘制。
- Outer warning gap 0.22→0.11 rad，Middle gap 0.23→0.04 rad；缺口用低亮度
  `0/1/./+` 连续填充；副带连续覆盖。
- Host 统计：Idle/Working Inner band=6、Outer band=23；
  changed pixels v3→v4 = 5,740/5,510（v2→v3=1,956/2,915）；
  annulus 黑像素比例下降（Inner 约 83.7→76.5%，Outer 95.7→95.0%）。
- 新截图 `p3_idle_v4.png` / `p3_working_v4.png`。
- 最终固件：1,273,072 B，SHA-256 `311428dc...6260`，app 剩余 17%，
  DIRAM 204,423/341,760 (59.81%)。
- 真机：internal_free 43,475→65,091 B，PSRAM ~6,133,308 B；
  Idle 6 FPS / Working 7 FPS（仍低于 Working 目标，P5 优化）；
  Wi-Fi/Node 在线；无 panic/OOM。
- Host：**3624 checks, 0 failures**；syntax **45 files OK**。
- P4 未开始。

### 11.21 PHOSPHOR P3.4 中心减量（2026-11-11）

- Core density 收窄到半径 48–64，1/17 cell 稀疏，强度 108–120；
  中心 <80 不再加 band FIELD；Inner band 110–150；Middle 215；
  Outer 主带/副带/0/1/缺口完全保持。
- 中心 radius<80 非空 cell：35→26（-25.7%），未变空；
  center 黑像素比例 Idle 71.07→76.27%，Working 72.85→74.51%。
- 新截图 `p3_idle_v5.png` / `p3_working_v5.png`。
- 最终固件：1,273,280 B，SHA-256 `eb8e1931...a2e356d2`，app 剩余 17%，
  DIRAM 204,423/341,760 (59.81%)。
- 真机：internal_free 49,599→65,119 B，PSRAM ~6,133,308 B；
  Idle 6–7 FPS，Working 7 FPS（仍低于 10–15 目标，P5）；
  Wi-Fi/Node 在线；无 panic/OOM。
- Host：**3624 checks, 0 failures**；syntax **45 files OK**。
- P4 未开始。

### 11.22 PHOSPHOR P3.5 Outer 字符串运输（2026-11-11）

- Outer 主带改为 `"0101+#%@=/<>"`（len 12），`idx=(i+bucket)%12`；
  0/1 约 33%、`%` 约 8%；不再 hash 闪变。
- 内侧 band 改为 `"0101+#%@"`（len 8），`idx=(i+bucket+3)%8`；
  0/1 约 50%、`%` 约 12.5%。
- bucket 仍 Working 300ms / Idle 800ms；字符整体平移形成数据串；
  半径/密度/缺口/Inner/Middle/P3.4 中心减量保持。
- 新截图 `p3_idle_v6.png` / `p3_working_v6.png`；
  v5→v6 changed pixels Idle 8,206 / Working 7,784。
- 最终固件：1,273,056 B，SHA-256 `8e6ab088...32ecd2`，app 剩余 17%，
  DIRAM 204,423/341,760 (59.81%)。
- 真机：internal_free 49,611→65,095 B，PSRAM ~6,133,276 B；
  Idle 7 FPS，Working 7 FPS（Working 仍低于目标，P5）；
  Wi-Fi/Node 在线；无 panic/OOM。
- Host：**3624 checks, 0 failures**；syntax **45 files OK**。
- P4 未开始。

### 11.23 PHOSPHOR P3.6 Outer 水层/漂浮物（2026-11-11）

- Outer 主带 pattern struct len20：Water 12（60%）Secondary 230，
  Objects 8（40%，两个 4-cell packet）Highlight 255；
  `idx=(i+bucket)%20`，bucket Working 300ms/Idle 800ms。
- Outer 内侧 band 仅水层 `01-=/` Secondary，不铺 Highlight；
  Middle 改 Primary 215；Inner 保持；P3.4 中心减量保持。
- 新截图 `p3_idle_v7.png` / `p3_working_v7.png`；
  v6→v7 changed pixels Idle 16,772 / Working 16,207。
- 最终固件：1,273,312 B，SHA-256 `c33dd717...eb05a7`，app 剩余 17%，
  DIRAM 204,423/341,760 (59.81%)。
- 真机：internal_free 49,607→65,131 B，PSRAM ~6,133,068 B；
  Idle 7 FPS，Working 7 FPS（Working 仍低于目标，P5）；
  Wi-Fi/Node 在线；无 panic/OOM。
- Host：**3624 checks, 0 failures**；syntax **45 files OK**。
- P4 未开始。

### 11.24 PHOSPHOR P4 六态重构（2026-11-11）

- 新增 `phosphor_state` resolver：offline 优先；approval/error 可叠加
  working；completed 900ms 去重 pulse；error 220ms glitch；4 satellite
  stable slot + overflow。
- `phosphor_demo`：Idle6→Working6→Approval6→Error6→Completed2→Offline4；
  demo 4 sessions：working 4 running，approval 2 running+2 waiting，
  error 2 running+1 error+1 idle。
- Visual 路径清除 P3 静态卫星；Approval 0.65rad 缺口 + 2 warning `?`；
  Error 5 个红色局部断裂 + glitch；Completed 3 层径向波 + 8 外轨粒子；
  Offline 轨道解体 + NO LINK / LAST SYNC；顶部 state label。
- 六态 goldens + `p4_contact.png`；pixel diff vs Working：
  Approval 5,144 / Error 3,240 / Offline 23,464 / Completed 25,595。
- 最终固件：1,281,328 B，SHA-256 `1ad08cf9...41993f3`，app 剩余 17%，
  DIRAM 204,623/341,760 (59.87%)。
- 真机：FPS ~7；Working 目标仍未达，P5；internal_free 49.4→64.9KB，
  PSRAM ~6,130,004 B；Wi-Fi/Node 在线；无 panic/OOM。
- Host：**3683 checks, 0 failures**；syntax **47 files OK**。
- P5/P6 未开始。

### 11.25 PHOSPHOR R1 静态六态重构（2026-11-11）

- 新增 `phosphor_core` direct state geometry；`phosphor_scene` visual 路径委托 core，
  不再是 P3.6 base + overlay。
- 六态几何：Idle/Working 水密度与 4 sat；Approval 0.65rad 缺口+2 `?`；
  Error 5 字符断裂；Completed 3 层波+8 外轨粒子；Offline 解体+NO LINK。
- R1.1 contact：Approval gap42 / Error break7；pixel diff vs Working：
  Idle4,104/Approval8,171/Error8,558/Offline19,901/Completed20,138。
- **只编译，不烧录**；设备保留 P4b。
- build-only app：1,284,080 B，SHA-256
  `0f65a53f...7b129d78`，app free 16%，DIRAM 59.87%。
- Host：**3693 checks, 0 failures**；syntax **48 files OK**。
- R2/motion/烧录未开始。

### 11.26 PHOSPHOR R2 motion + 烧录（2026-11-11）


- 在 R1.1 state-first core 上接 motion：Idle 0.08/-0.045/0.025（8s breath），
  Working 0.22/-0.55/0.32（5s），过渡 600ms，delta 200ms，LUT。
- Approval 1.35rad gap + 2 `?` + 1.75s edge breath；Error 220ms 一次性 glitch
  + 稳定 7 红字；Completed 900ms 0.96→0.90 收缩 + 三层波 + 8 粒子；
  Offline 550ms 降速后冻结，恢复回到 Idle/Working。
- app-flash 已执行（0x10000，保留 NVS）；app 1,284,512 B，
  SHA-256 `9afad977...e42c2bf`，free 16%，DIRAM 59.87%。
- 实机 70s：2 个完整周期，Offline→Idle 两次；Wi-Fi/Node/WS online；
  no panic/OOM；internal_free 55.7→64.9KB，PSRAM 6,129,748 B；
  Idle 6–7 FPS，Working 7 FPS（10–15 未达，留 P5）。
- Host：**3708 checks, 0 failures**；syntax **48 files OK**。
- P5/P6 未开始；未 commit/push。

### 11.27 PHOSPHOR R2.1 状态主导字符 + 单一主体色（2026-11-11）

- Approval/Error 在 core 末尾做状态主导 pass：除 label 行/2× `>_` 外，
  全部变为 `?`/`!` 并统一 Warning `0xF5AB` / Error `0xF36C`；
- 450ms 逐 cell 阈值转换；Approval 之后 1.75s 全局 intensity 呼吸；
  Error 220ms glitch 后稳定；
- 稳态整幅：Approval `?` 99/110 (90.0%)，Warning 110/110；
  Error `!` 101/109 (92.7%)，Error 109/109；Error 中无 `?`；
- r2b_contact + 六态 + 2 GIF；pixel diff Approval 23,893 / Error 20,973；
- **已 app-flash**：1,285,136 B，SHA-256
  `22685bdc...205f217`，free 16%，DIRAM 59.88%；
- 实机 70s：2 周期、Offline→Idle 两次；Wi-Fi/Node/WS online；无 panic/OOM；
  internal_free 55.7→64.9KB，PSRAM 6,129,748 B；Idle 6–7 / Working 7 FPS（P5）。
- Host：**3723 checks, 0 failures**；syntax **48 files OK**。
- P5/P6 未开始；未 commit/push。

### 11.28 PHOSPHOR P5 性能优化（2026-11-11）

- 新增 `glyph_dirty.c/.h`：上一帧比较、cell footprint dirty（含 2× Nucleus）、
  空 cell 清除、changed bbox、full/dirty 像素一致测试。
- 平台改 partial `lv_obj_invalidate_area`；bbox ≥70% fallback full；
  全量背景 32-bit 双像素填充；不再每帧全 canvas invalidate。
- Host：**3966 checks, 0 failures**；随机 80 帧 full/dirty 一致；
  2× Nucleus/边界/ghost/no-op 通过；syntax 51 files。
- 实机 Idle final：7 FPS，render 15.9–21.1ms（build 3.9–4.1，
  raster 11.9–16.9，invalidate 0.07），bbox 306–324，stack_hwm 1352。
- Working real P6（soak 中 activity=1）：**10–11 FPS**，render 19.5–24.8ms
  （build 4.4–4.9，raster 15.0–19.8），bbox 306–360；目标下限达成。
- app-flash final：1,289,984 B，SHA-256 `9af8a962...ad3928`，
  free 16%；final 恢复 P6 real ViewModel。
- 2h soak 已启动，报告时约 18s；完整结果待继续观察，未宣称通过。
- P5/P6 未改 Node/mobile/根文档；未 commit/push。

## 12. 仓库约束

- 只修改 `firmware/terminal/`
- 未修改 Node / 协议 / mobile / 根文档
- 未 commit / push
- 备份、构建产物、sdkconfig、managed_components 均在仓库外或被忽略
