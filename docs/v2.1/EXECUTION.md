# MeetAgent V2.1 执行计划（技术总监版）

> 角色：主会话担任技术总监，负责 Core/Node、协议、集成、验收与提交。
> 模块负责人：`omp` 负责 Mobile；`pi` 负责 ESP32 Terminal。
> 依据：`MeetAgentV2.1.md`、`TerminalUi.md`。
> 原则：Node-Centric、Multi-Client、Plugin-Based、LAN First、Cloud Optional。

---

## 1. 模块边界

| 模块 | 负责人 | 目录 | 说明 |
|---|---|---|---|
| Core / Node | 主会话 | `cmd/node`, `internal/`, `pkg/`, `configs/`, `scripts/`, `docs/v2.1/` | 协议、Node 服务、Adapter、配对、事件、命令、测试 |
| Mobile | `omp` | `mobile/` | Vue 3 + TS + Vite + Pinia + Router，后续 Capacitor |
| Terminal | `pi` | `firmware/terminal/` | ESP-IDF + FreeRTOS + LVGL + ESPUI |

Terminal 目标硬件：**Waveshare ESP32-S3-Touch-LCD-4 V4**（480×480 RGB565、GT911 五点触摸、CH32V003 helper、ESP32-S3N16R8）。开发顺序为 **BSP First**：优先复用官方 BSP `waveshare/esp32_s3_touch_lcd_4` → 点亮 LCD/触摸/背光 → 480×480 布局 → Wi-Fi/WSS。

规则：

- Mobile / Terminal 只能改自己的目录；不得修改 `internal/`、`pkg/`、`cmd/`、`configs/`、`web/test-client/`、根文档。
- 协议以 `pkg/clientproto/protocol.go` 与 `docs/v2.1/PROTOCOL.md` 为准；需要变更时先报告，由技术总监统一改。
- 不要 `git commit` / `git push`；由技术总监统一验收和提交。
- 每个里程碑完成后停下汇报：改动文件、构建结果、截图/日志、阻塞项。
- 不实现语音/STT；按用户要求先搭建大框架，语音后续单独立项。

---

## 2. 里程碑

### M0：公共架构与协议（进行中）

负责人：主会话

- [x] 版本统一为 V2.1；打 `v1.0-phase3` tag
- [x] 共享 `eventcache` 移入 `internal/eventcache`
- [x] `pkg/clientproto` 定义 V2.1 客户端协议
- [x] Adapter Manifest 与 Mock Adapter
- [x] Node Standalone 协议文档（`docs/v2.1/PROTOCOL.md`）
- [x] 协议/配对/事件/命令测试
- [x] 模拟器 CLI（`cmd/simulator`）

验收：

- 两种客户端可以声明不同 capabilities
- Node 可独立运行，不依赖 Cloud
- Adapter 与客户端无直接依赖
- Mock Adapter 可生成 session / message / tool / permission 事件

### M1：Node Core 改造

负责人：主会话

- [x] Node 本地 HTTP + WSS 服务（默认 `0.0.0.0:8787`）
- [x] 配对：6 位配对码、TTL、Token 持久化、撤销端点
- [x] Client Registry：client_id / type / capabilities / permissions / last_sequence
- [x] Snapshot + Event Stream + Command Gateway
- [x] 命令幂等、超时、能力/权限校验
- [x] OpenCode Adapter 接入
- [x] mDNS `_meetagent._tcp`
- [x] Cloud 作为可选 relay（默认关闭；保留 v1 代码）

验收：

- 不启动 Cloud 也能用 Node
- Mobile 模拟器与 Terminal 模拟器可同时连接
- 两个客户端收到同一业务状态
- 客户端身份可单独撤销
- `go test ./... -race`、`make build` 通过

### M2：双客户端并行开发

负责人：`omp` / `pi`，主会话提供 Mock Node 与协议

Mobile：

- Vue3 工程、连接/配对、Home、Sessions、Activity、Settings
- 协议 Client、状态同步、Mock 数据
- 验收：Mobile Web 原型可运行，页面与通信层分离，连接状态可见

Terminal：

- 目标硬件：Waveshare ESP32-S3-Touch-LCD-4 V4（480×480 RGB LCD + GT911 + CH32V003 helper）
- 优先使用官方 BSP 组件；ESP-IDF 工程、板级 BSP bring-up（LCD 点亮、触摸坐标、背光）、LVGL、ESPUI E0/E1
- 480×480 主题布局适配（替代旧的 320×480 竖屏假设）
- Wi-Fi 配网、mDNS 发现、WSS 原型、Mock 事件显示
- 验收：终端能显示 Mock 状态；LCD/触摸/背光正常；主题错误可回退

### M3：真实 Agent 集成

- OpenCode 真实 Session / Event / Prompt / Interrupt / Permission
- Mobile：完整交互
- Terminal：状态、活跃 Session、关键事件、快捷 Interrupt、审批摘要

### M4：双端统一与稳定性

- 双端同时在线、同一 Session 订阅、跨端状态更新
- 断线、Node 重启、Sequence 补偿、命令重复提交、权限单决策

### M5：移动端远程

- Tailscale、多网络切换、Capacitor、系统通知基础

### M6：增强

- 语音/STT、Cloud Relay、第二 Adapter、外部插件、OTA

---

## 3. 当前接口约定

- Node 本地端口：`8787`
- HTTP：
  - `GET /healthz`
  - `GET /api/v2/info`
  - `POST /api/v2/pair`
  - `GET /api/v2/snapshot`（Bearer）
- WebSocket：`/ws`
- 协议：`pkg/clientproto/protocol.go`
- 配对：Node 启动时打印 6 位配对码，TTL 10 分钟
- 客户端类型：`mobile` / `terminal` / `web` / `desktop`
- 能力：`ui.full`、`ui.compact`、`display.touch`、`input.text`、`input.button`、`input.encoder`、`input.voice`、`notification.*`
- 权限：`session.read`、`session.control`、`permission.respond`、`node.manage`
- 命令：`ping`、`node.status`、`session.list`、`session.prompt`、`session.interrupt`、`permission.respond`

---

## 4. 协作方式

1. 技术总监先完成 M0/M1 的可运行 Node 与 Mock Adapter。
2. Mobile / Terminal 先用 Mock/协议文档并行搭建，不等待 Node 全部完成。
3. 每个模块完成一个里程碑后停下汇报；技术总监验收、提交、下发下一阶段。
4. 跨模块问题通过技术总监协调，不直接改对方目录。
5. 所有验收以真机/真实构建为准，不接受“应该可以”。
