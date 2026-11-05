> **V1 legacy**：这些脚本针对 Cloud 为中心的 V1 Web 控制台，V2.1 使用 Node 直连协议（`docs/v2.1/PROTOCOL.md`、`cmd/simulator`）。保留用于 V1 回归，不再是 V2.1 的主验收路径。

# Phase 3 真机 E2E

用无头 Chromium 通过 Chrome DevTools Protocol 驱动 `web/test-client`，验证真实 OpenCode 链路的四条主路径：

| 脚本 | 覆盖 | 通过标准 |
|---|---|---|
| `prompt.js` | Prompt 闭环 | 9/9：命令 `accepted` → `completed`，事件流出现 `message.started/delta/completed`，回复文本为 `PONG-PHASE3` |
| `interrupt.js` | Interrupt | 7/7：长任务执行中打断，响应 `"interrupted": true`，Session 回到 `idle`，之后无该 Session 残留事件 |
| `permission-approve.js` | 权限批准 | 8/8：`permission.requested` 出现在 UI，点击「允许一次」后 `permission.resolved=approved`，工具执行并回传 `E2E_PERMISSION_PROBE` |
| `permission-reject.js` | 权限拒绝 | 9/9：点击「拒绝」后 `permission.resolved` 为 `rejected`、`tool.failed` 出现，回复未泄漏文件内容 |
| `reconnect.js` | 断线恢复 | 9/9：生成中断开 Device，离线窗口内的事件重连后全部补发（缺失 0），在途请求残留 0 |

## 前置条件

- Node.js **≥ 22**（脚本直接使用全局 `WebSocket`，不依赖 `ws` 包）
- 可执行的 Chromium（默认 `/usr/bin/chromium`，可用 `CHROME_BIN` 覆盖）
- Cloud、Node、OpenCode 与 `vite preview` 已启动：
  ```bash
  ./bin/meetagent-cloud -config configs/cloud.yaml
  OPENCODE_PASSWORD="$(opencode service get password)" ./bin/meetagent-node -config configs/node.yaml
  cd web/test-client && npm run build && npx vite preview --host 127.0.0.1 --port 4173 --strictPort
  ```
- Node 的本地策略开启 `permission_response`（`configs/node.yaml` 默认已开启）
- OpenCode 对 `read *.env` 配置为 `ask`（脚本用的 fixture 是 `*.env`），否则权限请求不会出现
- 设备凭据 `web-001` 具备 `node-001` 的 `allowed_nodes`

## 运行

```bash
export OPENCODE_PASSWORD="$(opencode service get password)"
node scripts/e2e/prepare.mjs   # 创建本次专用 Session，写 state.json

node scripts/e2e/prompt.js
node scripts/e2e/interrupt.js
node scripts/e2e/permission-approve.js
node scripts/e2e/permission-reject.js
node scripts/e2e/reconnect.js
```

`prepare.mjs` 每次创建一个新的 OpenCode Session（标题带时间戳）并写入 `/tmp/opencode/meetagent-e2e/state.json`；后续脚本自动读取，无需手动 export。需要复用固定 Session 时设置 `E2E_SESSION_TITLE`，需要覆盖 Session 时设置 `E2E_SESSION_ID`。

每个脚本会启动自己的 Chromium 实例（独立临时 profile）并在结束时关闭；默认 CDP 端口分别为 9230 / 9232 / 9233 / 9235 / 9234，可用 `CDP_PORT` 覆盖单个端口。脚本退出码：`0` 全部通过，`1` 有失败项，`2` 运行错误。

脚本默认使用 Device `web-002` 连接，避免覆盖开发者自己打开的 `web-001` 页面；可用 `E2E_DEVICE_ID` / `E2E_DEVICE_TOKEN` 覆盖。连接门禁会自动填入这组测试凭据。

## 环境变量

| 变量 | 默认值 | 说明 |
|---|---|---|
| `APP_URL` | `http://127.0.0.1:4173` | 测试客户端地址（`vite preview`） |
| `ADMIN_URL` | `http://127.0.0.1:8080` | Cloud HTTP 地址，用于读取 `/api/v1/snapshot` 做事件核对 |
| `ADMIN_TOKEN` | `dev-admin-token-change-me` | 管理端点 Token |
| `E2E_SESSION_ID` | 自动 | `prepare.mjs` 写入 state 文件；权限 / interrupt / reconnect 脚本用它核对事件归属 |
| `E2E_SESSION_TITLE` | 自动 | 本次运行创建的 Session 标题；显式设置时 `prepare.mjs` 会复用同名 Session |
| `E2E_STATE_FILE` | `/tmp/opencode/meetagent-e2e/state.json` | `prepare.mjs` 与各脚本共享的 state 文件 |
| `E2E_FILE` | 每次随机生成 | 权限脚本要读取的 `*.env`；默认每次生成唯一路径，避免上一条审批被复用 |
| `CHROME_BIN` | `/usr/bin/chromium` | Chromium 可执行文件 |
| `CDP_PORT` | 每个脚本不同 | 覆盖 DevTools 端口，避免并发冲突 |

## 注意

- 这些脚本会**真的向所选 Session 发送 Prompt**（含 `sleep 25/30` 的长任务）并真的打断、真的批准/拒绝权限。请使用专门的测试 Session，不要指向正在使用的会话。
- 脚本依赖当前测试客户端的界面文案（「连接 Cloud」「发送命令」「允许一次」「拒绝」等）。界面文案大改后需要同步更新选择器。
- 脚本只做验收，不修改协议、不写业务数据；`prepare.mjs` 创建本次测试 Session，权限脚本各自生成临时 `*.env` fixture。
