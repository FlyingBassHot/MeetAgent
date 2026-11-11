# MeetAgent V2.1 状态板

更新时间：2026-11-08

| 模块 | 负责人 | 当前阶段 | 状态 | 下一步 |
|---|---|---|---|---|
| Core / Node | 主会话 | M0 + M1 + M4 Core | 完成；审计/限流/重连边界加固 | M6 可选增强 |
| Protocol v2 | 主会话 | M0 | 完成 | 随实现补兼容测试 |
| Mobile | omp | M5b + UX P0/P1/P2 | 视觉与原生交互改造完成（信息层级/聊天流/触感/键盘/返回键/下拉刷新/视觉打磨） | iOS/Release |
| Terminal / ESPUI | pi | E0–E3a + LCD-4 Bring-up + E4a + E4b-1 | 真机启动成功；Wi-Fi STA 172.20.10.2；Node WS hello/welcome/snapshot、token NVS、心跳与重连已验证 | E4b-2（事件/命令）、AP portal `/save` 实测 |
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
- Terminal：host tests 807/807；syntax 33/33；ESP-IDF v5.5.5 + LVGL 8.4.0 + 官方 BSP 3.0.0 构建/烧录成功，app 506,240 bytes，480×480 布局已适配
- Terminal 真机 bring-up（2026-11-11）：16MB QIO Flash、8MB Octal PSRAM 80MHz、240MHz、ST7701 480×480、GT911 0x14、ESPUI 初始化成功；无 panic/assert/reset loop；原厂 16MB Flash 已备份（SHA-256 `c996b103...e2c7`）
- Terminal 屏幕方向：BSP 默认方向反 180°，已通过 `LV_DISP_ROT_180` 修正并烧录；用户人工确认画面已正立
- Terminal GT911 触摸：临时 Touch Test overlay 实机捕获 `TOUCH x=46 y=125`，用户确认 TL/TR/BL/BR/C 五点均跟随手指，180° 映射正确；测试 overlay 已移除
- Terminal E4a（2026-11-11）：Wi-Fi NVS 配网、STA 自动连接手机热点、断线重连、Node `/api/v2/info` 可达已验证；ESP32 `172.20.10.2`，Node `developer-machine/2.1.0`；AP portal `/save` 待第二设备实测
- Terminal E4b-1（2026-11-11）：Node WebSocket hello/welcome/snapshot 已通；50KB welcome 在 PSRAM 中剥离 events 后解析（10ms）；token 写入 NVS，重启仅用 token 重连；heartbeat RTT 约 50–60ms；Node 重启后自动重连并以 snapshot 重建；events apply/命令留 E4b-2
- Terminal 硬件目标确定为 **Waveshare ESP32-S3-Touch-LCD-4 V4**（480×480 RGB LCD + GT911 + CH32V003 helper）；真实 BSP 已接入，显示/触摸方向/PSRAM 待实物验证

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
- 运维：Docker 部署（`compose.yaml`，host network、`restart: unless-stopped`、配置/数据挂载、健康检查）已实测；systemd user service 保留为替代方案；`GET /metrics` 输出连接数、事件序号/缓存、命令结果等 Prometheus 指标
- Tailscale：100.69.235.2 登录；Mobile 通过 Tailscale IP 配对并读取真实 Snapshot（CORS 已允许 100.64/10）
- Android：JDK 21 + Android SDK 35；Capacitor 7.6.10 + `@capacitor/network` 7.0.4；Debug APK 4,260,002 bytes，SHA-256 `bbc8f06e16451080ef8469d98e4f0b153a08a0e95b1ddeaf185111e183b4bd27`（package com.flyingbasshot.meetagent）
- Android 真机（HBN-AL00 / 2NP0224717021762）：USB ADB 已授权；Tailscale `100.69.235.2:8787` 配对成功，Home 恢复 41 个真实 Session；测试 Session 完成 Prompt、Interrupt、权限「允许一次」、断网离线提示与恢复重连；Node 重启后自动应用新 Snapshot
- Android 真机修复：旧 WebView 缺少 `Promise.withResolvers`，已在 `mobile/src/main.ts` 增加 polyfill，并重新构建/安装 APK
- Mobile UX P0：Home 压缩为概览/活跃会话/最近动态；Sessions 去除 ID/路径；详情改为聊天流 + 底部输入；Activity 改「动态」并按天分组；技术信息统一进 Settings → 开发者信息
- Mobile UX P1：Capacitor Haptics/Keyboard/App/StatusBar；下拉刷新（snapshot envelope）；AppSheet/ActionSheet；返回键分层；键盘避让；页面过渡与滚动恢复
- Mobile UX P2：字体/间距/圆角/分隔线统一；状态视觉与空状态；Sessions 分组；动态分组；设置/开发者/连接页打磨；Android 启动背景与状态栏；APK 4,192,802 bytes，SHA-256 `adfcc3efd3928c7a46e80e01e3a766235c1668256a72a632bd081fbca1ae3702`
- `make dev-tls`：本地自签证书生成脚本（data/ 已忽略）
- M4 Core：双客户端同时收到同一 Prompt 事件；同一 permission 只允许一个最终决策；按 last_sequence 补发事件；PATCH 可独立配置客户端权限（集成测试）
- M4 Mobile：单标签页 Web Locks 守卫 + 接管；两个独立 client 跨端事件/Interrupt/审批一致；Node 受控重启后 Snapshot 恢复、序号重置不报缺口

## 阻塞 / 暂停

- **ESP32 真机 bring-up 已完成**：ESP-IDF 5.5.5 烧录/启动成功；屏幕实际颜色/方向、GT911 触摸方向/命中、CH32V003 背光/电池 ADC、多点触控与长时间运行仍需人工/后续验证；PHOSPHOR CORE 新规格暂不下发
- Mobile：使用 5174，避免与 V1 的 5173 冲突
- 语音/STT：按用户要求暂缓
- 多客户端/同 Token 多标签页：M4 处理
