# MeetAgent Debug Console（Web）

Phase 3 的浏览器调试控制台：通过 MeetAgent Cloud 连接远程机器上的 `meetagent-node`（OpenCode Adapter），
用于查看 Node / Agent / Session、发送 Prompt、中断执行、审批权限，并检查标准化事件流与通信日志。
桌面端是双栏工作台，手机端是「列表 → 全屏详情」，也可部署到公网后远程排障。

> 状态：**Phase 3 真机闭环 + Debug Console 信息架构重构（R1–R5）已完成**。
> 协议字段与 Go 侧行为冻结，前端只做展示、编排与本地 UI 状态。

## 信息架构

| 路由 | 页面 | 说明 |
| --- | --- | --- |
| `/console` | 总览 | 连接条、5 项健康指标、待审批权限、最近异常、Node 摘要（可下钻）、在途请求 |
| `/sessions` | 会话 | 桌面双栏工作台；手机列表 → 全屏详情。query `?node=&agent=&session=` |
| `/events` | 事件 | Tab 1「Agent 事件」，Tab 2「通信日志」；query `?tab=activity` |
| `/settings` | 设置 | 连接表单、自动连接、忘记设备、主题、协议说明、前端版本 |
| `/activity` | — | 兼容旧链接，重定向到 `/events?tab=activity` |

导航收敛为 4 项（总览 / 会话 / 事件 / 设置）；`<768px` 时侧边栏换成底部导航。
所有未知路径重定向到 `/console`。

## 连接门禁

- localStorage 中**不存在** `meetagent.test-client.v1` → 全屏 `ConnectionGate`，不显示控制台。
- 存在配置且 `autoConnect` 未关闭 → 页面挂载后自动 `connect()`；门禁页展示连接 / 注册 / 重连进度。
- 连接失败 → 门禁页显示错误与重试按钮，可直接编辑 URL / Device / Token。
- 连接成功后连接信息折叠：Header 只保留状态胶囊，详细连接信息在设置页 / 总览连接条。
- 曾经连接成功、随后断线 → 保留控制台与缓存数据，只显示重连横幅，不强制回门禁。
- 手动「断开」→ 回门禁页（可再次连接）；「忘记此设备」清除配置与选中态后回门禁。
- 默认 URL：本机 host 用 `ws://127.0.0.1:8080/ws`，其他 host 用 `wss://<当前 host>/ws`。
- **非 localhost 不预填开发凭据**：Device ID / Token 默认为空，需填写自己的凭据；门禁文案会明确提示。

## 常用命令

```bash
cd web/test-client
npm install          # 安装依赖
npm run dev          # 开发服务器：http://127.0.0.1:5173（--strictPort）
npm run typecheck    # vue-tsc --noEmit（严格模式）
npm run build        # typecheck + vite build → dist/
npm run preview      # 静态预览构建产物，默认 http://127.0.0.1:4173
```

**Origin 与 Cloud 白名单**：Cloud 只放行 `configs/cloud.yaml` 的 `allowed_origins`。
默认已放行 `http://127.0.0.1:5173`、`http://localhost:5173`（dev）与对应 `4173`（preview）。
用其它端口 / 域名自建 preview 时，必须先把该 origin 加入 `allowed_origins` 并重启 Cloud，否则 WebSocket 握手会被拒绝（页面表现为「WebSocket 发生错误」）。

## 首次使用

1. 按**根 README.md**启动 Cloud（本地默认 `ws://127.0.0.1:8080/ws`）：`make run-cloud`。
2. 准备 OpenCode 并按根 README 启动 Node（需导出 `OPENCODE_PASSWORD`）：`make run-node`。
3. `npm install && npm run dev`，浏览器打开 http://127.0.0.1:5173。
4. 首次进入是连接门禁；填入 **Cloud URL / Device ID / Device Token**（取自 `configs/cloud.yaml` 的 `auth.devices`）。
   **本文件不复制任何凭据**；开发 Token 只允许用于本地，生产必须换强随机 Token + `wss://`。
5. 连接成功后自动进入总览；去「会话」页选择 Node / Agent / Session 后即可发送 Prompt。
6. Prompt 快捷键为 **Ctrl / Cmd + Enter**；Session 不会自动选中，刷新时从 URL query 恢复。

构建产物（`dist/`）可用任意静态服务器托管（含子目录，资源走相对路径）；
不要用 `file://` 直接打开（ES 模块受 CORS 限制）。

## 会话工作台

- 左栏：Node / Agent 选择、**当前 Node + Agent 作用域内**的 Session 总数与运行中数量、标题 / ID 搜索、状态筛选（全部 / 运行中 / 空闲 / 错误）。
- 左栏只展示 `selectedNodeId` + `selectedAgentId` 的 Session；`allSessions` 仍供事件页下拉、按 ID 查找和总览统计使用。
- 右栏：Session 详情，两个 Tab：
  - **实时事件**（默认）：当前 Session 最近 50 条事件、跟随 / 暂停冻结、「在事件页打开」。
  - **详情**：Session / Agent / Node 元数据与能力标签。
- Prompt 输入条固定在工作台底部，**所有 Tab 都可见**；最近命令响应显示在输入条上方，待审批权限块显示在输入条上方（有 pending 时）。
- 选中态同时写入 URL 与 localStorage（键 `meetagent.test-client.selection.v1`）；
  刷新后按 URL 恢复，Node / Agent 可沿用上次选择。
- `<1024px` 左栏收窄到 300px；`<768px` 列表全屏，选中后详情以 fixed 覆盖层展示并隐藏底部导航，
  顶部返回按钮回到列表。事件区独立滚动，输入条在底部并预留 `safe-area-inset-bottom`。

## 事件页

- **Agent 事件**：按 Node / Agent / Session / 事件类型 / 文本过滤；支持从会话页带 query 进入。
- 已在 `/events` 时再次通过 query 切换 Session 会立即生效；不会清空手动输入的搜索 / 类型。
- `message.delta` 默认合并为一条（累计文本 + 条数徽标）；`tool.*` 按 `call_id` 归组。
- 「暂停跟随」会严格冻结当前列表（事件 id / 行数不变），新事件只累计到「N 条新事件」浮条；恢复后立即补齐。
- 「原始模式」关闭聚合；payload 默认折叠，每行可复制 JSON / event id。
- 错误事件左边线用 `--danger`，权限事件用 `--warn`。客户端最多保留 **300** 条。
- **通信日志**：完整保留原 Activity 能力（心跳开关、清空、类型 / 标题 / 明细文本），新增方向过滤（全部 / 入站 / 出站 / 信息 / 错误）。
- `<768px` 筛选区默认折叠，只显示搜索框与「筛选 (N)」；展开后可操作 Node / Agent / Session / 类型 / 原始模式 / 跟随 / 重置。
- 桌面总览的 Agents 指标显示「健康 / 总数」，仅 `online` / `busy` 计入健康。

## 目录结构与职责

```text
web/test-client/
├── index.html                  # data-theme / data-mode 预置 + 内联 favicon
├── vite.config.ts              # base: './'、@vitejs/plugin-vue、127.0.0.1:5173
├── src/
│   ├── main.ts                 # createApp + pinia + router；mount 前应用主题
│   ├── App.vue                 # 门禁判断 + 外壳（Header / 横幅 / Sidebar / RouterView / Footer / BottomNav）
│   ├── router/index.ts         # 4 路由 + /activity 重定向 + 页面标题
│   ├── api/
│   │   └── cloud-client.ts     # 纯传输层：WebSocket、信封、注册(10s)、心跳(20s)、RTT、
│   │                           #   重连(指数退避+抖动)、补发分页、请求(15s)超时；不 import 业务 store
│   ├── stores/
│   │   ├── connection.ts       # 配置/阶段/错误/重连/RTT；门禁状态（hasSavedConfig / showGate）、
│   │   │                       #   bootstrap / disconnect / forgetDevice；装配 client handlers 与快照编排
│   │   ├── inventory.ts        # Node/Agent/Session 扁平清单、选中态、URL/选择恢复
│   │   ├── events.ts           # 事件去重/排序/300 上限、游标、权限、在途请求、通信日志、最近异常
│   │   ├── commands.ts         # 发送命令、Prompt/中断/Node 工具、禁用原因、权限审批
│   │   ├── workbench.ts        # 会话页 UI：Node/Agent 作用域、搜索、筛选、Tab、选中封装
│   │   ├── eventView.ts        # 事件页 UI：过滤、聚合、暂停冻结、通信日志过滤
│   │   └── theme.ts            # 强调色 + 明暗模式（meetagent.theme.v1）
│   ├── views/                  # Console / Sessions / Events / Settings（只编排与 query 同步，≤220 行）
│   ├── components/
│   │   ├── layout/             # Header / Sidebar / Footer / BottomNav / ReconnectBanner / PermissionBanner
│   │   ├── connection/         # ConnectionGate / Form / Settings / StatusBar
│   │   ├── sessions/           # ListPane / Detail / PromptComposer / LiveEvents / Meta / NodeToolsMenu
│   │   ├── events/             # Filters / List / EventRow / ActivityTab
│   │   ├── activity/           # ActivityLog（通信日志渲染）
│   │   ├── overview/           # RecentIssues
│   │   ├── nodes/              # NodeSummaryCard（总览下钻）
│   │   ├── permissions|pending/
│   │   └── ui/                 # BaseCard / EmptyState / StatItem / StatusPill / Skeleton / ThemeSwitcher
│   ├── lib/
│   │   ├── protocol.ts         # 协议类型 + makeEnvelope/newMessageId（与 Go 同构，勿改字段）
│   │   ├── format.ts           # formatTime / formatRelative / shortId / idSuffix / payload / summarize
│   │   ├── status.ts           # 阶段 / pending / session 文案与色调
│   │   ├── events.ts           # 事件搜索与 message.delta / tool.* 聚合（纯函数）
│   │   └── clipboard.ts        # 复制文本（失败静默）
│   └── styles/
│       ├── index.css           # 唯一入口：tokens → base → components
│       ├── tokens.css          # 设计变量（唯一允许写死色值）+ 6 套强调色 + 明暗表面色
│       ├── base.css            # reset / 排版 / 布局骨架 / 路由淡入
│       └── components.css      # 按钮/卡片/表单/工作台/事件/门禁/响应式（全部走变量）
```

**约定（强制）**

- **业务在 store，组件只展示与上抛事件**；组件不直连 WebSocket、不写协议字段。
- 单 `.vue` ≤ 250 行、view ≤ 220 行、`App.vue` ≤ 80 行、`<script setup lang="ts">`、禁用 `any`。
- 禁止组件内写死色值（`tokens.css` 除外）；禁止循环 import；store 单向组合（UI store → 业务 store）。
- 协议字段冻结：前端不得擅自增删/改名；行为疑问先读根 README 与 `internal/cloud/router/router.go`。
- 错误一律进通信日志或页面提示，不留 `console.log`；不用 `any` 绕类型。

## E2E 稳定锚点（data-testid）

| 区域 | testid |
| --- | --- |
| 导航 | `nav-console` `nav-sessions` `nav-events` `nav-settings`（移动端加 `-mobile` 后缀） |
| 连接门禁 | `connection-gate` `connection-url` `connection-device` `connection-token` `connection-submit` `connection-error` |
| 连接状态 / 设置 | `connection-status` `connection-status-bar` `connection-settings` `connection-disconnect` `connection-forget` `autoconnect-toggle` |
| 全局 | `permission-banner` `permission-badge` `frontend-version` |
| 会话列表 | `session-list-pane` `session-search` `session-filter` `session-filter-all/running/idle/error` `session-item`（附 `data-session-id` / `data-node-id` / `data-agent-id`） |
| 会话详情 | `session-detail` `session-detail-meta` `session-back` `session-tab-events/detail` `session-live-follow` `session-live-resume` `open-events` |
| Prompt / 工具 | `prompt-input` `prompt-send` `session-interrupt` `last-response` `session-permissions` `node-tools` `node-tool-ping/echo/status/sessions` |
| 权限 | `permission-approve` `permission-always` `permission-reject` |
| 总览 | `node-summary`（附 `data-node-id`） `issue-item` |
| 事件页 | `events-tab` `activity-tab` `event-filter` `event-filter-toggle` `event-search` `event-follow` `event-item`（附 `data-event-id`） |
| 通信日志 | `activity-filter` `activity-search` `activity-item` |

关键按钮文案：`连接 Cloud`、`断开连接`、`忘记此设备`、`发送 Prompt`、`中断`、`Node 工具 ▾`、
`允许一次`、`总是允许`、`拒绝`、`暂停跟随` / `恢复跟随`、`重置过滤`、`清空`、`去处理`、`筛选 (N)`。

## 主题系统

- 两轴切换：`<html data-theme="teal|jade|cyan|violet|amber|rose" data-mode="dark|light">`。
- 6 套强调色，明暗两套表面色一一对应；默认 **teal + dark**（`index.html` 预置防闪）。
- 持久化键：**`meetagent.theme.v1`**，值 `{ "accent": "teal", "mode": "dark" }`。
- 入口：Header 与设置页的 ThemeSwitcher；切换即时生效。
- 新增强调色时同步 `tokens.css`（含 `--swatch-*`）、`stores/theme.ts` 的联合类型与选项，再跑构建。

## 协议对接要点（v1）

- **信封**：`version / message_id / type / action? / request_id? / source{type,id} / target?{node_id,agent_id,session_id} / timestamp / payload?`
  类型：`register | heartbeat | command | response | event | sync | error`。
- **阶段机**：`offline → connecting → registering → online`；异常关闭 → `reconnecting`
  （退避 `min(30s, 2ⁿ⁻¹ s) ×(1±0.2 抖动)`，下限 500ms）；注册应答 `ok:false` → `offline` 并展示 `error_code`。
- **重连补发**：重连后 `register` 携带 `last_sequences`；`sync/events` 分页拉取（每页 1000 条，最多 5 页）；
  某节点被 Node 截断（`truncated`）时剔除其事件并**回退完整快照**。
- **快照**：注册应答内联 `snapshot` + 可主动 `sync/snapshot`；应用时 nodes → inventory，事件/游标/权限 → events。
- **心跳**：20s 一次 `heartbeat`（带 `nonce`），响应回填 **RTT**（ms，断线复位为空）。
- **事件**：按 `node_id:sequence` 去重、按时间排序、上限 **300** 条；断线重连后自动补发。
- **命令白名单**：`ping` / `echo` / `get_status` / `list_sessions` / `send_prompt` / `interrupt`，
  另有 `permission_response`。`send_prompt`/`interrupt` 需 Agent+Session；Prompt 不能为空。
- **权限审批**：`once` / `always` / `reject` → `permission_response`。
- **在途请求**：`sent → accepted → executing → completed | failed | rejected | timeout`，15s 未决即 `timeout`。
- **本地设置键**：`meetagent.test-client.v1`（`{url, deviceId, token, autoConnect}`）；
  选中态键：`meetagent.test-client.selection.v1`。

## 常见问题

| 现象 | 原因与处理 |
| --- | --- |
| 一直停留在连接门禁 | 没有保存过配置或自动连接被关闭；填入 URL / Device / Token 后点「连接 Cloud」。 |
| 「注册被拒绝」（含 `error_code`） | Device ID / Token 与 Cloud `auth.devices` 不匹配。离线态改完重连即可。 |
| 「WebSocket 发生错误」/ 连接后立即失败 | 页面 origin 不在 Cloud `allowed_origins`。把该 origin 加入 `configs/cloud.yaml` 并重启 Cloud。 |
| 快照里没有 Node / 节点离线 | Node 未启动或连不上 Cloud；同一 node id 多连接时以最新连接为准（旧连接会被踢）。 |
| 命令 15s 超时 | Node 离线 / Adapter 未连上 OpenCode / OpenCode 无响应。查通信日志的 `response · timeout`。 |
| 命令被 Node 策略拒绝 | `configs/node.yaml` `policy.allowed_actions` 与协议白名单双重校验；Adapter 能力不足同理。 |
| 事件缺失 / 补发 | 重连自动补发；`truncated` 时回退 Snapshot（通信日志有记录）。 |
| 想恢复默认连接 | 设置页「忘记此设备」，或删 `meetagent.test-client.v1` 后刷新。 |
| 页面样式异常 / 白屏 | 必须经静态服务器访问（`npm run dev` / `npm run preview`），不支持 `file://`。 |

## 里程碑对照

- M1 工程骨架 → M2 传输层与状态拆分 → M3 视图/组件拆分与真机闭环 →
  M4 视觉升级（6 套强调色 × 明暗、响应式、焦点走查）→
  **M5 Debug Console 重构**（4 导航、连接门禁、会话双栏工作台、事件过滤/聚合/暂停、总览与设置重排、移动端全屏详情）。

当前前端版本：`0.4.0-phase3`（设置页可查看）。
