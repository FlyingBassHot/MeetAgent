# 构建与验证状态（E0 / E1 / E2 / E3a / LCD-4 V4）

日期：2026-11-08  
范围：`firmware/terminal/` only  
目标：Waveshare ESP32-S3-Touch-LCD-4 V4.0（ESP32-S3N16R8）  
构建方式：真实 ESP-IDF 编译，**未烧录、未接串口**

## 1. 真实固件构建

### 1.1 环境与依赖

```text
ESP-IDF : v5.4.4 (/home/perch/esp/esp-idf)
目标    : esp32s3
工具链  : xtensa-esp32s3-elf
BSP     : waveshare/esp32_s3_touch_lcd_4 3.0.0
LVGL    : 8.4.0（固定 8.4.*）
port    : espressif/esp_lvgl_port 2.9.0
touch   : espressif/esp_lcd_touch_gt911 1.2.1
helper  : waveshare/custom_io_expander_ch32v003 2.0.0
```

`main/idf_component.yml` 声明 BSP 依赖；`components/espui/idf_component.yml`
把 LVGL 固定为 `8.4.*`。未启用 `lv_demo_*`、未加入额外字体。

### 1.2 命令

```bash
source "$HOME/esp/esp-idf/export.sh"
cd /home/perch/Work/MeetAgent/firmware/terminal
rm -f sdkconfig sdkconfig.old     # 让新的 sdkconfig.defaults 生效
idf.py set-target esp32s3
idf.py build
idf.py size
```

### 1.3 结果

```text
Project build complete.
```

- `idf.py build`：**成功**，exit 0
- 项目代码编译/链接错误：**0**
- 完整构建有一条第三方 deprecation warning：
  `espressif__esp_lvgl_port` 使用 `esp_lcd_touch_get_coordinates`（BSP/port 内部）
- 增量重编译：0 warning
- 未烧录、未连接串口
- 未修改 16MB / Octal PSRAM 的硬件猜测；只增加官方示例的性能项

### 1.4 固件大小

```text
build/meetagent_terminal.bin              502,272 bytes (0x7AA00)
build/bootloader/bootloader.bin            22,368 bytes (0x5760)
build/partition_table/partition-table.bin   3,072 bytes
```

`idf.py size`：

```text
Total image size : 502,162 bytes
Flash Code       : 319,184 bytes
Flash Data       :  85,428 bytes
DIRAM            : 141,523 / 341,760 bytes (41.41%)
IRAM (size 工具) :  16,383 / 16,384 bytes (99.99%)
```

App 分区：

```text
Smallest app partition : 0x177000 bytes
Free                   : 0xFC600 bytes (67%)
```

说明：`idf.py size` 的 IRAM 行 total 为 16,384，与链接脚本
`iram0_0_seg = 0x57700` 的口径不一致；实际链接的 IRAM 断言通过，构建未报溢出。
该行仍需真机/后续 size 工具核对。

## 2. 本次改动

- 接入官方 BSP 依赖（`main/idf_component.yml`），不复制 BSP 源码
- 新增 `main/board_lcd4.c` / `.h`：480×480、GT911 主触点、CH32 背光/复位、
  `bsp_display_lock/unlock`
- 保留 `espui_board_stub()` 供 host tests，并更新为 480×480
- Safe / Terminal 主题 Manifest 改为 `orientation=any`、最小 480×480
- 重做 480×480 方形布局（core/info 面板，不是横向拉伸旧布局）
- ASCII Renderer 默认网格 60×30；host CLI 同步
- `sdkconfig.defaults` 增加 240MHz、QIO、80MHz Octal PSRAM、
  fetch instructions/rodata、64B cache line、LVGL 性能项；不启用 demo/font 全家桶
- 更新 host tests、README、HARDWARE、THEME_API、ASCII_RENDERER、BUILD_STATUS

构建过程中**没有需要修复的编译/链接错误**。

## 3. 主机验证

```bash
cd firmware/terminal/tests/host
make test
```

```text
MeetAgent Terminal host tests
[ RUN ] test_ascii_renderer_run
[ RUN ] test_builtin_themes_run
[ RUN ] test_manifest_run
[ RUN ] test_motion_profiles_run
[ RUN ] test_runtime_helpers_run
[ RUN ] test_scene_manager_run
[ RUN ] test_state_adapter_run
[ RUN ] test_theme_manager_run
[ RUN ] test_tokens_run
[ RUN ] test_view_model_run

807 checks, 0 failure(s)
```

```bash
make syntax
```

```text
[syntax] OK: 33 file(s)
```

`main/board_lcd4.c` 依赖 managed BSP/LVGL 头文件，不在 host stub 语法检查内；
由真实 `idf.py build` 验证。

ASCII 预览：

```bash
make preview STATE=working FRAMES=2 DELAY=300
```

## 4. 未验证项 / 风险

- [ ] 未烧录，未接串口，未验证 bootloader/app 启动日志
- [ ] 未验证 480×480 实际颜色、ST7701 方向、背光极性/亮度曲线
- [ ] 未验证 GT911 触摸方向/镜像；当前只暴露主触点（硬件 5 点，port 只注册 1 点）
- [ ] 未验证 CH32V003 电池 ADC 映射（当前电量未知 255）
- [ ] 未验证 16MB Flash / Octal PSRAM 与实物一致
- [ ] 未验证 BSP 在 ESP-IDF 5.4.4 的运行时稳定性（官方示例推荐 5.5.5/6）
- [ ] 未验证 BSP LVGL draw buffer 的内存位置与余量
- [ ] `idf.py size` IRAM 行口径待核对
- [ ] E3b：ASCII 帧 → LVGL 字符网格、字体/颜色 Token 映射、局部刷新
- [ ] E4：Wi-Fi / mDNS / 配对 / WSS / 协议 JSON / Event 增量
- [ ] 真机 FPS、CPU、内存、长时间运行

## 5. 已实现清单

- [x] ESP-IDF 工程、ESPUI 分层、Theme API / Manifest / Tokens / ViewModel / Scene
- [x] Safe Fallback + Terminal Aesthetic E2 mock
- [x] E3a 纯逻辑 ASCII Renderer（字符网格/密度场/几何/六态/脉冲/Scheduler/Scene）
- [x] LCD-4 V4 BSP 适配（480×480、GT911 主触点、背光/复位、LVGL lock/unlock）
- [x] 480×480 Manifest / 方形主题布局 / ASCII 60×30
- [x] 真实 IDF v5.4.4 构建通过
- [x] Host tests 807/807、syntax 33/33
- [x] 文档：架构、Theme API、ViewModel、硬件、ASCII Renderer、构建状态

## 6. 未实现清单

- [ ] E3b LVGL 字符网格 / 字体 / Color Token 映射 / 局部刷新
- [ ] GT911 多点原始读取、触摸方向校准
- [ ] CH32 电池 ADC → 电量映射
- [ ] 真实 LCD/touch 真机验收、烧录
- [ ] E4 网络/协议/Command
- [ ] E5 设置持久化、亮度/夜间模式、OTA

## 7. 仓库约束

- 只修改 `firmware/terminal/`
- 未修改 Node / 协议 / mobile / 根文档
- 未执行 `git commit` / `git push`
- 未烧录、未接串口
