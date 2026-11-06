# MeetAgent Mobile（V2.1 / M5b）

MeetAgent V2.1 的移动端 Web 原型，后续由 Capacitor 打包为 Android / iOS 应用。

当前进度：M5b Android Debug APK 构建完成（Capacitor 7.6.10 + JDK 21 + SDK 35）。
Web、Mock、局域网 Node、自签 WSS Node 与 Android WebView 共用同一套 `ClientTransport` 与协议类型。

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
├── capacitor.config.ts       # appId / appName / webDir
├── README.md
├── docs/REMOTE.md            # Tailscale / WSS / 证书 / Capacitor 说明
├── docs/ANDROID.md           # Android SDK / Gradle / APK / 签名说明
├── android/                  # Capacitor Android 工程
└── src/
    ├── app/
    │   ├── nav.ts              # 底部/侧边导航
    │   └── router.ts           # 路由与连接守卫
    ├── components/             # 展示组件，不直接访问 WebSocket
    ├── domain/                 # 协议类型、格式化、事件分组、时间线
    ├── services/
    │   ├── api/http.ts         # GET /api/v2/info、POST /api/v2/pair
    │   ├── client/             # ClientTransport / WsClient / MockClient
    │   ├── connectivity/       # Web Locks 单连接守卫 + BroadcastChannel 接管
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

## M4 单连接守卫与多客户端

已验证：

- 单标签页保护：同 Origin 第二个标签页使用 Web Locks 获锁失败，显示「已在另一个标签页连接」，不建立 Node WebSocket；Node 日志无替换连接
- 接管：点击「在此标签页接管」通过 BroadcastChannel 通知旧标签页；旧标签页主动断开并释放锁，新标签页获得锁后使用已保存 Token 重连；Node 侧仅发生一次 disconnect
- 多客户端：使用独立 Chromium profile 分别配对，两个 client_id / Token 同时在线；A 发送 Prompt 后 B 实时收到相同 `message` / `tool` 事件（两端同一 `sequence`）
- 跨客户端 Interrupt：A Interrupt 后，B 的 Session 状态同步回到 idle
- 审批一致性：同一权限请求在 A、B 两端同时出现；A 批准后 B 的待审批卡片自动消失
- 重复决策：独立探针客户端对已处理权限再次提交决策，Node 返回失败（`PermissionNotFoundError`），未产生第二次执行
- Node 重启恢复：受控重启 Node 后，两端自动重连，Snapshot 恢复 41 个 Session；事件序号重置为 46，UI 未报告序号缺口；测试 Session 状态为 idle
- 全程仅对测试 Session `ses_e5a1c7989ffefcU1XdrTiq6WXX` 发送 Prompt，未触碰主会话 `ses_e5e6e776fffeeeCdeah5WtPEhJ`

## M5a 远程连接与网络切换

详见 [`docs/REMOTE.md`](docs/REMOTE.md)：Tailscale、反向代理 WSS、证书信任与 Capacitor 环境说明。

已验证：

- 网络状态：监听 `window online/offline`；离线时顶部显示「网络离线」并全局提示，暂停 WebSocket 与指数退避重试
- 恢复网络：取消旧退避计时并立即重连（实测 6 ms 内恢复在线），不会与原有重连叠加
- 连接诊断（Settings）：显示 `navigator.onLine`、WebSocket scheme（`ws` / `wss`）、连接阶段、RTT 与最后错误
- WSS/TLS：使用自签证书 Node `https://127.0.0.1:8789`，Chromium `--ignore-certificate-errors` 下完成配对、Snapshot、Prompt 与事件流；客户端派生 `wss://127.0.0.1:8789/ws`
- TLS Mock Node：Agent `TLS Mock`、Session `tls-session`，Prompt 与事件正常
- 证书失败提示：未信任证书的浏览器无法完成 HTTPS 配对或 WSS 握手；UI 显示通用提示“自签证书请先在系统/浏览器中信任（浏览器不会暴露证书细节）”
- 未触碰 `8787` 真实 Node 与 OpenCode 主会话

## M5b Capacitor Android

已验证：

- `npm install` 安装 Capacitor Core / CLI / Android **7.6.10**
- `capacitor.config.ts`：`com.flyingbasshot.meetagent` / `MeetAgent Mobile` / `webDir: dist`
- `npm run build && npx cap add android && npx cap sync android`
- JDK 21 + Android SDK 35 + Gradle 8.11.1 + AGP 8.7.2 执行 `./gradlew assembleDebug`
- `BUILD SUCCESSFUL`，Debug APK：`android/app/build/outputs/apk/debug/app-debug.apk`（约 4.0 MB）
- aapt 检查：`package=com.flyingbasshot.meetagent`、`versionCode=1`、`versionName=1.0`、`minSdk=23`、`targetSdk=35`
- Debug 专用明文配置：`android/app/src/debug/AndroidManifest.xml` + `network_security_config.xml`，允许 LAN / Tailscale `100.64.0.0/10` 的 HTTP/WS 调试
- `.gitignore` 已忽略 `local.properties`、`.gradle/`、`build/`、`app/build/` 与 APK

未连接设备/模拟器，因此未执行安装与真机运行。

## 尚未实现（后续里程碑）

- Android Release 签名与渠道发布、iOS 打包
- Keychain / Keystore、APNs / FCM
- Tailscale 实网部署与多网络切换（仅提供部署文档）
- 语音 / STT
- 通知的更细粒度策略（仅审批提醒基础已预留）
- 跨设备客户端身份与权限管理（当前每个设备/Profile 独立配对）

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

M4 截图位于 `/tmp/opencode/meetagent-mobile-live/`：

- `m4-tab-guard-390.png`（第二标签页被单连接守卫拦截）
- `m4-tab-takeover-390.png`（旧标签页显示已被接管）
- `m4-tab-takeover-active-390.png`（新标签页接管后在线）
- `m4-client-a-prompt-390.png` / `m4-client-b-prompt-390.png`（跨客户端 Prompt 事件）
- `m4-client-b-interrupt-390.png`（B 看到 Interrupt 后 idle）
- `m4-permission-both-a-390.png` / `m4-permission-both-b-390.png`（两端待审批）
- `m4-permission-cleared-b-390.png`（A 批准后 B pending 消失）
- `m4-node-restart-c1.png` / `m4-node-restart-c2.png`（Node 重启后两端恢复）

M5a 截图位于 `/tmp/opencode/meetagent-mobile-live/`：

- `m5-tls-home-390.png`
- `m5-tls-sessions-390.png`
- `m5-tls-session-detail-390.png`
- `m5-tls-prompt-390.png`
- `m5-tls-diagnostics-390.png`（wss、navigator.onLine、最后错误）
- `m5-network-offline-390.png`（网络离线提示）
- `m5-network-recovered-390.png`（恢复后在线）
- `m5-tls-cert-error-390.png`（自签证书未信任的配对提示）
- `m5-tls-wss-error-390.png`（WSS 握手失败提示）

## 已知限制

- Web 原型的 Token 存在 `localStorage`，不等同于安全存储。
- 单连接守卫使用 Web Locks + BroadcastChannel，作用范围为同 Origin、同浏览器 Profile；不同 Profile / 不同设备使用独立 Token，可同时在线。
- 接管依赖旧标签页响应 BroadcastChannel；旧标签页被浏览器冻结且不响应时，接管会在 6 秒后失败并提示，避免强制抢占造成双连接。
- Web Locks 不可用的环境退化为允许连接；Capacitor 单 WebView 场景不受影响。
- 自签证书只用于本机测试；正式远程部署需使用受信任 CA 或 Tailscale 证书。浏览器不会向页面暴露证书细节，UI 只能给出通用信任提示。
- Android Debug 包允许明文 HTTP/WS，仅用于 LAN / Tailscale 开发；Release 包必须使用 HTTPS/WSS。
- 本机无连接设备/模拟器，APK 未安装运行；未配置 Release 签名与 iOS 工程。
- 开发模式使用 Vite HMR 时，长时间断网可能让开发服务器连接丢失并触发页面重载；生产构建与 Capacitor 不受此影响。
- 真实 Node 需要允许 `http://localhost:5174` / `http://localhost:4173` 的 CORS 来源；WSS 证书需浏览器信任。
- 浏览器通知依赖用户授权；部分移动浏览器要求 Service Worker 才能展示系统通知。
- Mock 心跳、RTT 与回复均为本地模拟值，不能作为真实 Agent 性能依据。
