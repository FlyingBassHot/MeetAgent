# ESP32 Terminal / ESPUI 模块任务书（负责人：pi）

> 依据：`MeetAgentV2.1.md`、`TerminalUi.md`、`docs/v2.1/EXECUTION.md`、`docs/v2.1/PROTOCOL.md`
> 目录：只允许修改 `firmware/terminal/`
> 不要修改 Node/Go/协议/configs/web/test-client/根文档；不要 commit/push。

## 目标

建立 MeetAgent Terminal 固件工程与 ESPUI 基础设施。首版是**局域网常驻终端**，不是手机缩小版：

- 低信息密度、快速感知
- 旋钮/按键为主，触摸为辅助
- 状态、活跃 Session、关键事件、Interrupt、审批摘要
- 不实现完整 Prompt 聊天、文件浏览、长历史、本地 AI

目标硬件：**Waveshare ESP32-S3-Touch-LCD-3.5B**，320×480 竖屏，ESP-IDF + FreeRTOS + LVGL。

## 当前环境

- 本机尚未安装 ESP-IDF（`idf.py` 不存在，`IDF_PATH` 为空）
- 先完成 E0/E1/E2 的代码与架构，能写的代码先写；不能编译时如实汇报阻塞
- 不要自行下载/安装 ESP-IDF（体积大、可能影响环境）；需要时由主会话统一处理
- 不要假设 GPIO/引脚；以开发板原理图和官方示例为准，先定义 HAL 接口

## 第一阶段（本次交付）

### E0：ESPUI 架构定义

- `docs/` 或 `README.md` 记录分层、Theme API、Manifest、ViewModel、Scene Manager
- 目录骨架：

```text
firmware/terminal/
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   └── app_main.c
├── components/
│   ├── espui/
│   │   ├── core/          # ViewModel / navigation / input router / action dispatcher
│   │   ├── runtime/       # theme manager / scene manager / animation scheduler / assets
│   │   ├── theme_api/     # espui_theme.h / manifest / scene / tokens
│   │   ├── themes/terminal_aesthetic/
│   │   └── hal/           # display / touch / input / power 接口
│   ├── protocol/          # V2.1 client protocol 类型与解析（先 stub）
│   ├── state/             # ViewModel / state adapter
│   └── networking/        # wifi / http / websocket（先 stub）
└── tests/
```

### E1：基础 UI Runtime

- ESP-IDF 工程初始化、LVGL 接入点
- HAL 接口：display/touch/encoder/button/power（实现可为空/模拟）
- UI Task + 事件队列（网络回调不能直接操作 LVGL）
- Theme Registry + 内置安全主题
- 一个可运行的 Mock ViewModel + 简单状态页面（不能编译时保留代码与说明）

### E2：主题框架（最小）

- Theme Manifest 结构、Design Tokens、Scene Manager、Motion Profile 类型
- 至少两个 Mock Theme 注册/切换（可用编译期注册）
- 主题加载失败回退安全主题
- 切换主题不丢 ViewModel

首版不要求完整 ASCII Renderer/六态动画；E3 后续再做。先让框架和数据流成立。

## 后续（等主会话下发）

- E3 Terminal Aesthetic：ASCII Renderer、六态动画、Ambient/Focus/Activity/Attention
- E4 接真实 Node：Snapshot/Event/Command
- E5 配置、亮度、夜间模式、长时间运行

## 协议

- 阅读 `docs/v2.1/PROTOCOL.md` 与 `pkg/clientproto/protocol.go`
- Terminal 首版只连 LAN；`ws://<node>:8787/ws`
- 客户端类型 `terminal`；capabilities：`ui.compact`、`display.touch`、`input.button`、`input.encoder`、`notification.local`
- 后续接 Node 时实现 hello/welcome/snapshot/event/command/command.result/heartbeat

## 验收与汇报

完成 E0/E1/E2 最小框架后停下汇报：

1. 目录树 + 关键头文件/源文件
2. 架构说明：Theme API、ViewModel、Scene、HAL 边界
3. 是否安装 ESP-IDF/能否编译；不能编译的具体阻塞
4. 已实现/未实现清单
5. 引脚/硬件假设（如有）
6. 下一步建议

不要自行开始 E3 动画或真实 Node 联调。
