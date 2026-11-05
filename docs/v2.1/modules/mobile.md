# Mobile 模块任务书（负责人：omp）

> 依据：`MeetAgentV2.1.md`、`docs/v2.1/EXECUTION.md`、`docs/v2.1/PROTOCOL.md`
> 目录：只允许修改 `mobile/`
> 不要修改 Node/Go/协议/configs/web/test-client/根文档；不要 commit/push。

## 目标

从零建立 MeetAgent Mobile Web 原型（后续 Capacitor 打包）。Mobile 是完整的远程交互入口，不是调试控制台：

- 连接/配对 Node
- Home：Node 状态、Agent 状态、活跃 Session、待审批/异常
- Sessions：Session 列表、详情、事件、Prompt、Interrupt、审批
- Activity：全局事件流、筛选、审批入口
- Settings：Node 配置、连接、通知、安全/权限（基础）

## 技术栈

- Vue 3 + TypeScript + Vite
- Pinia + Vue Router（hash 模式）
- 原生 CSS + CSS 变量，不引入 UI 组件库
- 不引入 Tailwind/CSS-in-JS
- Capacitor 先只做 `platform/` 抽象层和 README，不要求现在打包 Android/iOS

## 协议

- 阅读 `docs/v2.1/PROTOCOL.md` 与 `pkg/clientproto/protocol.go`
- 实现 `services/client` 抽象：`WsClient`（真实 Node）+ `MockClient`（本地 mock）
- WS：hello → welcome/snapshot → event / command / command.result / heartbeat
- HTTP：`GET /api/v2/info`、`POST /api/v2/pair`
- 重连：hello 带 last_sequence；收到 welcome 后用 snapshot 覆盖，再应用补发事件
- 认证：Token 存 localStorage（Web 原型）；后续 Capacitor 换 Keychain/Keystore

Node 尚未完成时先用 `MockClient` 做 UI；完成后我会提供地址/配对码，切 `WsClient` 做真机联调。

## M2 交付（先做这一步）

1. `mobile/` 工程可运行：`npm install && npm run dev`
2. 连接/配对页：Node URL + 配对码/Token，连接状态、错误、重试
3. Home / Sessions / Activity / Settings 四个页面
4. Sessions 支持列表 → 详情、Prompt 输入、Interrupt、审批（Mock 数据）
5. 响应式：390×844 与 1440×900 都可用；手机优先
6. 状态管理：连接、Snapshot、事件、命令、审批、错误
7. `npm run typecheck`、`npm run build` 通过
8. 截图放 `/tmp/opencode/meetagent-mobile/`
9. 更新 `mobile/README.md`

## 约束

- `any` 禁止；`console.log` 禁止；TODO/FIXME 禁止
- 单 `.vue` ≤ 250 行、view ≤ 220 行、App.vue ≤ 100 行
- 组件不直接碰 WebSocket；网络在 services，状态在 stores
- 颜色走 CSS 变量；明暗模式先支持暗色即可，后续再加
- 不伪造 Agent 进度；Mock 数据要明确标注 Mock
- 代码/文案用中文为主，协议字段保持英文

## 验收与汇报

完成 M2 后停下汇报：

1. 文件树 + 行数
2. typecheck/build 输出
3. 截图路径
4. 已实现/未实现清单
5. 与 Node 联调还缺什么
6. 阻塞项

不要自行进入 M3 真实联调，等主会话提供 Node 服务。
