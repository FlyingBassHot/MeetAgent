# MeetAgent Mobile（V2.1 / M2）

MeetAgent V2.1 的移动端 Web 原型，后续由 Capacitor 打包为 Android / iOS 应用。

当前里程碑为 M2：在 Node 尚未提供稳定服务前，先用本地 `MockClient` 打通连接、状态同步、Session 交互、审批与事件流，同时保留与真实 Node 一致的 `WsClient` 协议实现。

## 技术栈

- Vue 3 + TypeScript + Vite
- Pinia + Vue Router（hash 模式）
- 原生 CSS + CSS 变量，暗色主题
- 不引入 UI 组件库、Tailwind 或 CSS-in-JS
- Capacitor 只保留 Platform Adapter 抽象，未打包原生工程

## 快速开始

要求 Node.js 20+（开发环境使用 Node 26）。

```bash
cd mobile
npm install
npm run dev        # http://localhost:5174
npm run typecheck
npm run build
npm run preview    # http://localhost:4173
```

开发端口固定为 `5174`（`strictPort`），避免与 `web/test-client` 的 `5173` 冲突；预览端口为 `4173`。

## 使用方式

### Mock 演示

1. 打开页面，选择「Mock 演示」。
2. 点击「使用 Mock 数据进入」。
3. 页面顶部与导航栏持续显示 `MOCK 数据`；Mock 回复文本也带有 `（Mock 回复）` 标记。

Mock 覆盖：

- Node / Adapter / Agent / Session Snapshot
- 消息、工具、Session、Agent、审批事件
- Prompt 流式回复、Interrupt、审批决策
- 心跳 RTT（模拟值）与序号递增

Mock 不伪造真实 Agent 进度：所有数据由本地脚本生成，不调用任何模型或 Agent。

### 连接真实 Node

1. 在 Node 日志中获取 6 位配对码（TTL 10 分钟）。
2. 选择「真实 Node」，填写 Node 地址（如 `http://192.168.1.10:8787`）与配对码。
3. 可选：「检测 Node」调用 `GET /api/v2/info` 验证可达性并展示 Node 信息。
4. 点击「连接并配对」。

配对流程：

- 有配对码时先调用 `POST /api/v2/pair` 获取 Token；若 Node 尚未实现该端点（404/405/501），自动回退到 WebSocket `hello.pairing_code`。
- 已有 Token 时直接以 Token 握手。
- Token 保存在 `localStorage`（Web 原型限制）；Capacitor 版本需迁移到 Keychain / Keystore。

## 目录结构

```text
mobile/
├── index.html
├── package.json
├── tsconfig.json
├── vite.config.ts
├── README.md
└── src/
    ├── app/
    │   ├── nav.ts              # 底部/侧边导航
    │   └── router.ts           # 路由与连接守卫
    ├── components/             # 展示组件，不直接访问 WebSocket
    ├── domain/                 # 协议类型、格式化、事件分组、时间线
    ├── services/
    │   ├── api/http.ts         # GET /api/v2/info、POST /api/v2/pair
    │   ├── client/             # ClientTransport / WsClient / MockClient
    │   ├── mock/scenario.ts    # 可复现的 Mock Node 数据
    │   ├── platform/           # browser / capacitor 平台抽象
    │   └── storage/            # 偏好与 Token（localStorage）
    ├── stores/                 # connection / node / commands / preferences / ui
    ├── styles/base.css         # CSS 变量、布局与通用组件样式
    ├── views/                  # Connect / Home / Sessions / SessionDetail / Activity / Settings
    ├── App.vue
    └── main.ts
```

## 架构

```text
Views / Components
        │ 读取状态、调用 action
        ▼
Pinia Stores (connection / node / commands)
        │
        ▼
ClientTransport
   ├── WsClient   → 真实 Node（HTTP 配对 + WSS）
   └── MockClient → 本地场景脚本
```

- 页面与组件只使用 store，不直接创建 WebSocket 或解析协议。
- `stores/connection` 负责连接生命周期、握手、心跳、重连、Token 与错误。
- `stores/node` 负责 Snapshot、事件去重/补发、Session/Agent/审批状态归并。
- `stores/commands` 负责 Prompt、Interrupt、审批命令的在途状态与结果反馈。
- `WsClient` 与 `MockClient` 实现同一个 `ClientTransport` 接口，可替换。

## 协议实现（对应 `docs/v2.1/PROTOCOL.md`）

- 握手：`hello` → `welcome`，支持 Token、配对码与 `last_sequence`
- 状态：`welcome.snapshot` 全量覆盖本地状态；`snapshot` 消息同样处理
- 事件：按 `sequence` 去重，检测序号缺口并在 UI 提示；忽略未知事件类型
- 命令：`ping`、`node.status`、`session.list`、`session.prompt`、`session.interrupt`、`permission.respond`；Node 对 Session 级命令强制要求同时携带 `agent_id` 与 `session_id`
- 结果：`command.result`；15 秒超时、连接断开时拒绝在途命令
- 心跳：JSON `heartbeat` 计算 RTT
- 重连：指数退避（1s → 15s）；重连后携带最新 `last_sequence`，以 Snapshot 为准再应用补发事件
- 错误：认证/协议版本等致命错误停止重连并展示；普通错误提示后继续

## M2 已实现

- Vue3 + TS + Vite + Pinia + Router 工程，`typecheck` 与 `build` 通过
- 连接/配对页：Mock / 真实 Node、Node 地址、配对码、Token、设备名称、检测 Node、错误与状态
- Home：Node 状态、Agent 状态、活跃 Session、待审批、最近事件、运行统计
- Sessions：搜索、筛选、Session 列表与详情
- Session 详情：事件时间线、Prompt、Interrupt（二次确认）、审批决策、本机发送记录
- Activity：全局事件流、分类筛选、事件详情展开、审批入口、序号缺口提示
- Settings：连接、Node、设备与权限、Token、通知、自动连接
- 响应式：390×844 手机优先，≥900px 切换侧边栏布局，1440×900 可用
- Mock 与真实 Node 共用协议类型与通信接口；组件与通信层分离

## M3 真实 Node 联调

联调环境：Node `http://127.0.0.1:8787`，OpenCode Adapter 在线；Mobile `http://127.0.0.1:5174`。

已验证：

- 配对与 Snapshot：真实 `welcome` 返回 41 个 Session、1 个 Agent（OpenCode Main）；Agent 状态与事件流正常
- Session 列表与详情：真实 Session 标题、状态、更新时间、工作目录与历史事件正常展示
- Prompt：仅向测试 Session `ses_e5a1c7989ffefcU1XdrTiq6WXX` 发送，流式增量与完成消息正常；未向主会话 `ses_e5e6e776fffeeeCdeah5WtPEhJ` 发送任何命令
- Interrupt：对测试 Session 生效，状态回到 idle，流式消息停止
- 权限：新建 `/tmp/opencode/meetagent-e2e/m3-approve-*.env` 触发 read 审批并「允许一次」，Agent 成功读取；新建 `m3-reject-*.env` 触发审批并「拒绝」，工具失败且未读取内容
- 断线/重连：通过 DevTools 网络离线模拟断开，UI 显示「重连中」并退避重试；恢复网络后自动重连，重新收到 Snapshot（41 个 Session），无序号缺口提示
- 真机联调修复：Session 级命令现在同时携带 `agent_id` 与 `session_id`（Node 对缺失字段返回 `rejected`）

## 尚未实现（后续里程碑）

- Capacitor Android / iOS 打包、Keychain / Keystore、APNs / FCM
- mDNS 自动发现、Tailscale / VPN 状态展示
- 语音 / STT
- 通知的更细粒度策略（仅审批提醒基础已预留）
- 多客户端场景（M4）：同 Token 多标签页/设备并发连接尚未做互斥或独立身份管理

## 截图

M2 验收截图位于 `/tmp/opencode/meetagent-mobile/`：

- `01-connect-390.png`
- `02-home-390.png`
- `03-sessions-390.png`
- `04-session-detail-390.png`（底部 composer 与导航间距验收）
- `05-session-prompt-streaming-390.png`
- `06-session-interrupted-390.png`
- `07-activity-390.png`
- `07b-activity-bottom-390.png`（事件列表底部留白验收）
- `08-activity-approved-390.png`
- `09-settings-390.png`
- `09b-settings-bottom-390.png`
- `11-home-1440.png`
- `12-sessions-1440.png`
- `13-session-detail-1440.png`
- `13b-session-detail-bottom-1440.png`
- `14-activity-1440.png`
- `14b-activity-bottom-1440.png`
- `15-settings-1440.png`
- `15b-settings-bottom-1440.png`

M3 真实 Node 截图位于 `/tmp/opencode/meetagent-mobile-live/`：

- `live-home-390.png` / `live-home-1440.png`
- `live-sessions-390.png` / `live-sessions-1440.png`
- `live-session-detail-390.png` / `live-session-detail-1440.png`
- `live-activity-390.png` / `live-activity-1440.png`
- `live-prompt-streaming-390.png`
- `live-interrupt-390.png`
- `live-permission-pending-390.png`
- `live-permission-approved-390.png`
- `live-permission-reject-pending-390.png`
- `live-permission-rejected-390.png`
- `live-reconnecting-390.png`
- `live-reconnected-390.png`

## 已知限制

- Web 原型的 Token 存在 `localStorage`，不等同于安全存储。
- 同一 Token / client_id 同时打开多个标签页会互相替换 Node 连接并触发重连抖动；联调或使用时保持单标签页，M4 再实现多客户端身份与互斥。
- 真实 Node 需要允许 `http://localhost:5174` / `http://localhost:4173` 的 CORS 来源；WSS 证书需浏览器信任。
- 浏览器通知依赖用户授权；部分移动浏览器要求 Service Worker 才能展示系统通知。
- Mock 心跳、RTT 与回复均为本地模拟值，不能作为真实 Agent 性能依据。
