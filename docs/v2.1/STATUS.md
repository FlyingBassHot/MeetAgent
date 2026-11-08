# MeetAgent V2.1 状态板

更新时间：2026-11-06

| 模块 | 负责人 | 当前阶段 | 状态 | 下一步 |
|---|---|---|---|---|
| Core / Node | 主会话 | M0 + M1 + M4 Core | 完成；审计/限流/重连边界加固 | M6 可选增强 |
| Protocol v2 | 主会话 | M0 | 完成 | 随实现补兼容测试 |
| Mobile | omp | M4 + M5a + M5b | 完成（Tailscale、WSS/TLS、Android 真机配对/Prompt/事件/node 重启恢复） | iOS/Release |
| Terminal / ESPUI | pi | E0–E2 + E3a | ESP-IDF v5.4.4 已安装；首次固件编译验证中 | ESP32 实物显示/触摸验收 |
| Cloud relay | 主会话 | 可选 | 保留 V1 实现 | M6 再验证混合模式 |

## 已完成

- V2.1 文档统一；`v1.0-phase3` tag
- `pkg/clientproto`：hello/welcome、snapshot、event、command/result、pair
- Node standalone：HTTP + WS `:8787`、`/healthz`、`/api/v2/info`、`/api/v2/pair`、`/api/v2/snapshot`、`/api/v2/clients`
- 配对：6 位码、TTL、Token SHA-256、持久化、撤销
- Client Registry：client_id/type/capabilities/permissions/last_sequence、同 ID 替换
- Event Hub：Node sequence、bounded cache、snapshot、replay、raw fan-out（Cloud 可选）
- Command Gateway：幂等、超时、client capability + permission + adapter capability
- Adapter Manifest + Mock Adapter + `cmd/simulator`
- mDNS `_meetagent._tcp`（avahi-browse 实测）
- 测试：`go test ./... -race -count=1` 通过
- Mobile：`npm run typecheck` / `npm run build` 通过；390/1440 截图
- Terminal：host tests 805/805；syntax 33/33；ESP-IDF 未安装

## 真实联调（主会话）

- Mock Node：`configs/node-mock.yaml` + simulator：配对、prompt、message/tool 事件通过
- OpenCode Node：`configs/node.yaml` + simulator：36 个真实 Session，prompt `V21-SIM` 返回成功
- Admin：`GET /api/v2/clients` 正确列出模拟器；撤销后 Token 失效（集成测试）
- mDNS：`avahi-browse -rt _meetagent._tcp` 能看到 node-001:8787
- Cloud 可选：node-002 同时跑本地服务 + Cloud relay；Cloud Device 成功下发 `send_prompt` 并收到完整 message.* 事件
- WSS/TLS：自签证书 + `wss://127.0.0.1:8789/ws` + simulator，prompt/event 闭环通过（仅本地自签验证，生产证书仍需配置）
- M5a Mobile：浏览器 online/offline 暂停重试与立即恢复；TLS 自签配对/Snapshot/Prompt；诊断面板；`mobile/docs/REMOTE.md`
- Tailscale：`100.69.235.2` 已登录；Mobile 通过 `http://100.69.235.2:5174` 连接 `http://100.69.235.2:8787`，配对/Snapshot 成功；CORS 已允许 100.64/10
- 客户端在线状态：`/api/v2/clients` 返回 `online` 与最近活动时间，心跳/命令会刷新 last_seen
- 权限过期：无 `expires_at` 的 permission.requested 自动补充 10 分钟默认过期；过期/重复决策被 Node 拒绝
- 审计：配对/连接/重连（含 last_sequence、replay_truncated）/认证失败/断开/撤销/权限更新/命令终态写入结构化 audit 日志，不记录 Token
- 限流：单客户端入站令牌桶（100/s、burst 200），超限返回 `rate_limited`；`cmd/node` 已从配置接线
- 重连边界：先订阅后生成 Snapshot，消除握手期事件丢失；缓存淘汰/TTL/Node 重启时 `welcome.replay_truncated=true`，不再返回部分补发事件
- Node 重启恢复：同一配对存储下，旧 `last_sequence` 重连得到新序号 Snapshot，后续事件从新序号连续递增（集成测试）
- 运维：systemd user service `meetagent-node`（`Restart=always`、linger、密码从 `~/.config/meetagent/node.env` 读取）；`GET /metrics` 输出连接数、事件序号/缓存、命令结果等 Prometheus 指标
- Tailscale：100.69.235.2 登录；Mobile 通过 Tailscale IP 配对并读取真实 Snapshot（CORS 已允许 100.64/10）
- Android：JDK 21 + Android SDK 35；Capacitor 7.6.10；Gradle 8.11.1 Debug APK 4,230,914 bytes，SHA-256 `b8fbdc243d45251c0e98e542282e9e1e23b12aa0e2f289bfa1c9d93195cceec4`（package com.flyingbasshot.meetagent）
- Android 真机（HBN-AL00 / 2NP0224717021762）：USB ADB 已授权；Tailscale `100.69.235.2:8787` 配对成功，Home 恢复 41 个真实 Session；真实测试 Session `ses_e5a1c7989ffefcU1XdrTiq6WXX` 发出 Prompt，收到 `message.completed` 与 running→idle 状态；Node 重启后客户端自动重连并应用新 Snapshot
- Android 真机修复：旧 WebView 缺少 `Promise.withResolvers`，已在 `mobile/src/main.ts` 增加 polyfill，并重新构建/安装 APK
- `make dev-tls`：本地自签证书生成脚本（data/ 已忽略）
- M4 Core：双客户端同时收到同一 Prompt 事件；同一 permission 只允许一个最终决策；按 last_sequence 补发事件；PATCH 可独立配置客户端权限（集成测试）
- M4 Mobile：单标签页 Web Locks 守卫 + 接管；两个独立 client 跨端事件/Interrupt/审批一致；Node 受控重启后 Snapshot 恢复、序号重置不报缺口

## 阻塞 / 暂停

- **ESP32 硬件未到**：ESP-IDF v5.4.4 与 esp32s3 工具链已安装，Terminal 可编译；显示/触摸/烧录验收需等实物
- Mobile：使用 5174，避免与 V1 的 5173 冲突
- 语音/STT：按用户要求暂缓
- 多客户端/同 Token 多标签页：M4 处理
