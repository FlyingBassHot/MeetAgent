# MeetAgent Test Client（Web）

Phase 2 浏览器测试客户端：通过 MeetAgent Cloud 连接远程机器上的 `meetagent-node`（OpenCode Adapter），
提供 **连接管理、状态快照与事件补发、白名单远程命令、Agent 权限审批、Activity 活动日志**，
用于在浏览器侧验收整条 Phase 2 链路（Browser → Cloud → Node → OpenCode）。

> 状态：**M1–M5 已验收**（工程骨架 → 传输层与状态拆分 → 视图/组件拆分与功能回归 → 视觉升级 → 文档与收尾）。
> 协议字段与 Go 侧行为冻结，前端只做展示与拆分。

## 环境要求

- Node.js **≥ 20.19**（Vite 8 要求）
- npm **≥ 10**（随 Node 自带）
- Go 侧（Cloud / Node）见**根 README.md**；本文只覆盖 `web/test-client/`

## 常用命令

```bash
cd web/test-client
npm install          # 安装依赖
npm run dev          # 开发服务器：http://127.0.0.1:5173 （--strictPort，占用即失败）
npm run typecheck    # vue-tsc --noEmit（严格模式）
npm run build        # vue-tsc --noEmit && vite build → dist/
npm run preview      # 静态预览构建产物，默认 http://127.0.0.1:4173
```

**Origin 与 Cloud 白名单**：Cloud 只放行 `configs/cloud.yaml` 的 `allowed_origins`。
默认已放行 `http://127.0.0.1:5173`、`http://localhost:5173`（dev）与对应 `4173`（preview）。
用其它端口 / 域名自建 preview 时，必须先把该 origin 加入 `allowed_origins` 并重启 Cloud，否则 WebSocket 握手会被拒绝（页面表现为「WebSocket 发生错误」）。

## 首次使用

1. 按**根 README.md**启动 Cloud（本地默认 `ws://127.0.0.1:8080/ws`）：`make run-cloud`。
2. 准备 OpenCode 并按根 README 启动 Node（需导出 `OPENCODE_PASSWORD`）：`make run-node`。
3. `npm install && npm run dev`，浏览器打开 http://127.0.0.1:5173。
4. 连接所需的 **Cloud URL / Device ID / Device Token** 取自 `configs/cloud.yaml` 的 `auth.devices`
   （例如 `web-001`、`web-002`，各自可达节点不同，见根 README「make web-dev」一节）。
   **本文件不复制任何凭据**；开发 Token 只允许用于本地，生产必须换强随机 Token + wss://。
   配置仅保存到浏览器 localStorage（键 `meetagent.test-client.v1`），不会上传到任何服务器。
5. 点击「连接 Cloud」：注册成功后顶栏显示在线、Nodes 数刷新，Node 清单来自注册应答里的 Snapshot。
6. 发命令前先在「会话」页选中 Node / Agent / Session；发送快捷键为 **Ctrl / Cmd + Enter**。

构建产物（`dist/`）可用任意静态服务器托管（含子目录，资源走相对路径）；
不要用 `file://` 直接打开（ES 模块受 CORS 限制）。

## 目录结构与职责

```text
web/test-client/
├── index.html                  # 预置 data-theme / data-mode（防首屏闪烁）
├── vite.config.ts              # base: './'、@vitejs/plugin-vue、127.0.0.1:5173
├── src/
│   ├── main.ts                 # createApp + pinia + router；mount 前先应用主题并注册 CloudClient 回调
│   ├── App.vue                 # 仅外壳：AppHeader / ReconnectBanner / Sidebar / RouterView / AppFooter / BottomNav
│   ├── router/index.ts         # hash 路由、重定向、页面标题（document.title）
│   ├── api/
│   │   └── cloud-client.ts     # 纯传输层：WebSocket、信封收发、注册(10s 超时)、心跳(20s)、RTT、
│   │                           #   重连(指数退避+抖动)、事件补发分页、请求(15s)超时定时器。
│   │                           #   不 import 任何业务 store；经回调(handlers)把消息交给 store
│   ├── stores/                 # Pinia：业务全在这里
│   │   ├── connection.ts       # url/deviceId/token、阶段、lastError、重连、RTT；装配 client handlers、快照编排
│   │   ├── inventory.ts        # Node/Agent/Session 清单与选中态（applyNodes 失效回退）
│   │   ├── events.ts           # 事件(去重/排序/上限 300)、lastSequences、权限、在途请求、Activity 日志
│   │   ├── commands.ts         # 当前 action、echo/prompt 文本、发送与输入校验、权限审批动作
│   │   └── theme.ts            # 强调色 + 明暗模式；localStorage（meetagent.theme.v1）
│   ├── views/                  # 路由页：只组装组件、注入 store，不写业务（≤220 行）
│   ├── components/
│   │   ├── layout/             # AppHeader / AppSidebar / AppFooter / BottomNav(<768 固定底部导航) / ReconnectBanner
│   │   ├── connection|nodes|agents|commands|events|permissions|pending|activity/   # 业务区块·展示与上抛
│   │   └── ui/                 # BaseCard / EmptyState / StatItem / StatusPill / Skeleton / ThemeSwitcher
│   ├── lib/
│   │   ├── protocol.ts         # 协议类型 + makeEnvelope/newMessageId/isTerminalStatus（与 Go 同构，勿改字段）
│   │   ├── format.ts           # formatTime / shortId / formatPayload / summarize / truncate
│   │   └── status.ts           # 阶段文案 / pending 文案 / 事件色调 / phaseTone
│   └── styles/
│       ├── index.css           # 唯一入口：@import tokens → base → components
│       ├── tokens.css          # 设计变量（唯一允许写死色值的地方）+ 6 套强调色 + 明暗两套表面色
│       ├── base.css            # reset / 排版 / 布局骨架 / 路由淡入
│       └── components.css      # 按钮/卡片/表单/胶囊/列表/空态/骨架/底部导航（全部走变量）
```

**约定（强制）**

- **业务在 store，组件只展示与上抛事件**；组件不直连 WebSocket、不写协议字段。
- 单 `.vue` ≤ 250 行、view ≤ 220 行、`App.vue` ≤ 80 行、`<script setup lang="ts">`、禁用 `any`。
- 禁止组件内写死色值（`tokens.css` 除外）；禁止循环 import；store 可单向组合（commands → connection/inventory/events）。
- 协议字段冻结：前端不得擅自增删/改名；行为疑问先读根 README 与 `internal/cloud/router/router.go`。
- 错误一律进 Activity 日志或页面提示，不留 `console.log`；不用 `any` 绕类型。

## 主题系统

- 两轴切换：`<html data-theme="teal|jade|cyan|violet|amber|rose" data-mode="dark|light">`。
- 6 套强调色（暗色对比度实测：按钮 ≥ 6.4、作文字 ≥ 6.4；亮色 ≥ 4.9），明暗两套表面色一一对应。
- 持久化键：**`meetagent.theme.v1`**，值 `{ "accent": "teal", "mode": "dark" }`；
  **默认 teal + dark**（`index.html` 预置防闪）；无存储时跟随 `prefers-color-scheme`。
- 入口：Header（与「设置」页）里的 ThemeSwitcher；切换即时生效并写入 localStorage。
- **新增一套强调色**（以 `mint` 为例），组件无需改动：
  1. `styles/tokens.css`：追加 `[data-theme='mint'] { --accent; --accent-strong; --accent-soft; --accent-contrast; }`
     与 `[data-mode='light'][data-theme='mint'] { … }`（亮色需加深）；
  2. 同文件 `:root` 追加 `--swatch-mint: #xxxxxx;`（色板固定色值，仅此处允许）；
  3. `stores/theme.ts`：`AccentTheme` 联合、`ACCENT_VALUES`、`ACCENT_OPTIONS`（中文 label）同步加 `mint`；
  4. 校验对比度（正文 ≥ 4.5、次要文字 ≥ 3，按钮文字 ≥ 4.5）：用相对亮度公式或任一 WCAG 计算器核对；
  5. `npm run typecheck && npm run build` 后，Header/设置页色板即出现新色。

## 协议对接要点（v1）

- **信封**：`version / message_id / type / action? / request_id? / source{type,id} / target?{node_id,agent_id,session_id} / timestamp / payload?`
  类型：`register | heartbeat | command | response | event | sync | error`。
- **阶段机**：`offline → connecting → registering → online`；异常关闭 → `reconnecting`
  （退避 `min(30s, 2ⁿ⁻¹ s) ×(1±0.2 抖动)`，下限 500ms）；注册应答 `ok:false` → `offline` 并展示 `error_code`。
- **重连补发**：重连后 `register` 携带 `last_sequences`；`sync/events` 分页拉取（每页 1000 条，最多 5 页）；
  某节点被 Node 截断（`truncated`）时剔除其事件并**回退完整快照**。
- **快照**：注册应答内联 `snapshot` + 可主动 `sync/snapshot`；应用时 nodes → inventory，事件/游标/权限 → events。
  Cloud 在节点上/下线时会主动推送快照（按 `node_id:sequence` 去重，幂等应用）。
- **心跳**：20s 一次 `heartbeat`（带 `nonce`），响应回填 **RTT**（ms，断线复位为空）。
- **事件**：按 `node_id:sequence` 去重、按时间排序、上限 **300** 条；断线重连后自动补发。
- **命令 6 个**（白名单）：`ping` / `echo` / `get_status` / `list_sessions` / `send_prompt` / `interrupt`，
  另有 `permission_response`（权限审批）。`send_prompt`/`interrupt` 需先选 Agent+Session；Prompt 不能为空。
- **权限审批**：`once`（允许一次）/ `always`（总是允许）/ `reject`（拒绝）→ `permission_response`。
- **在途请求**：`sent → accepted → executing → completed | failed | rejected | timeout`，15s 未决即 `timeout`。
- **本地设置键**：`meetagent.test-client.v1`（`{url, deviceId, token}`，与旧版同键，向后兼容）。

## 常见问题

| 现象 | 原因与处理 |
| --- | --- |
| 「注册被拒绝」alert（含 `error_code`） | Device ID / Token 与 Cloud `auth.devices` 不匹配。离线态改完重连即可（自动写入 localStorage）；详见根 README 的设备凭据一节。 |
| 「WebSocket 发生错误」/ 连接后立即失败 | 页面 origin 不在 Cloud `allowed_origins`。把该 origin 加入 `configs/cloud.yaml` 并重启 Cloud（dev 默认已放行 5173/4173）。 |
| 快照里没有 Node / 节点离线 | Node 未启动或连不上 Cloud；`make run-node`（需 `OPENCODE_PASSWORD`）。同一 node id 多连接时 Cloud 会踢旧连接（`close 4001 replaced by a newer connection`），以最新连接为准。 |
| 命令 15s 超时（Activity：`response · timeout`） | Node 离线 / Adapter 未连上 OpenCode / OpenCode 无响应。查 Node 日志与 Activity 中 `node.online`、`agent.status_changed`。 |
| 命令被 Node 策略拒绝（`rejected`） | `configs/node.yaml` `policy.allowed_actions` 白名单与协议白名单双重校验；`permission_response` 默认不在白名单，需显式加入；Adapter 能力不足同理。 |
| 事件缺失 / 补发 | 重连自动补发；`truncated` 时回退 Snapshot（Activity 有「事件缓存已截断，回退到 Snapshot」条目）。 |
| 断线后点了没反应 | 断线期间命令/快照按钮不可用（属预期）；顶栏「重连中 #N」点「立即重试」可立即重连。 |
| 配置损坏 / 想恢复默认 | DevTools → Application → 删 `meetagent.test-client.v1` 与 `meetagent.theme.v1` 后刷新（恢复 dev 默认地址与 teal+dark）。 |
| 页面样式异常 / 白屏 | 必须经静态服务器访问（`npm run dev` / `npm run preview`），不支持 `file://`；dev 端口被占时会直接报错。 |

## 里程碑对照

- M1 工程骨架（路由 / Pinia / 样式框架）→ M2 传输层与状态拆分（`api/cloud-client.ts` + 4 个 store）→
  M3 视图与组件拆分 + 功能零回归（真机冒烟 12/12：M3 功能冒烟 8/8 —— 连接、快照回流、设置页、断开、输入解锁；
  心跳专项 4/4 —— RTT、心跳日志开关、Activity 清空）→ M4 视觉升级（6 套强调色 × 明暗、三档响应式、状态设计、焦点走查 0 失焦）。
