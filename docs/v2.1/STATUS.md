# MeetAgent V2.1 状态板

更新时间：2026-11-08

| 模块 | 负责人 | 当前阶段 | 状态 | 下一步 |
|---|---|---|---|---|
| Core / Node | 主会话 | M0 + M1 + M4 Core | 完成；审计/限流/重连边界加固 | M6 可选增强 |
| Protocol v2 | 主会话 | M0 | 完成 | 随实现补兼容测试 |
| Mobile | omp | M5b + UX P0/P1/P2 | 视觉与原生交互改造完成（信息层级/聊天流/触感/键盘/返回键/下拉刷新/视觉打磨） | iOS/Release |
| Terminal / ESPUI | pi | E0–E3a + LCD-4 Bring-up + E4a + E4b-1/2/3 + PHOSPHOR R2 + net QA fixes | PHOSPHOR state-first 重构 + R2 motion 已烧录；六态 demo 连续 2 周期、Offline→Idle 恢复，state label/大缺口/断裂/三层波/NO LINK 已上屏 | P6 真实 ViewModel 映射（并行中）、P5 性能、interrupt/permission、AP portal `/save` |
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
- Terminal E4b-2（2026-11-11）：真实事件 apply 到 ViewModel；session/agent/permission 状态、active 计数、completion/error 计数随事件更新；sequence 去重/旧事件忽略；Node 重启 `replay_truncated=1` 用 snapshot 重建；host tests 974/974；命令/审批留 E4b-3
- Terminal E4b-3（2026-11-11）：command gateway 安全链路已验证（`REFRESH_STATE → session.list` completed，42 sessions，footer ACK）；interrupt/permission.respond 代码完成、pending 表/15s 超时/断线 failed 已接入；host tests 1024/1024；interrupt/permission 尚未安全实机测试
- Terminal PHOSPHOR P1（2026-11-11）：P0 审计 + P1 静态 Core 已上屏；24×16 网格 / 18×27 Bold+dilate / 432×432 canvas / 三条不同轨道 / 2× `>_` / `*` + `?` satellites；PC SDL golden screenshot 与 ESP32 实机同源；host tests 2013/2013；P2 密度/颜色待做
- Terminal PHOSPHOR P2（2026-11-11）：Glyph 密度 ramp、核心密度场、8 色 palette RGB565 精确校验、前后层级合成（NUCLEUS>SATELLITE>FRONT>BACK>FIELD）、2× Nucleus 裁剪；host tests 3541/3541；P2 静态截图 `docs/phosphor/p2_static.png`；P3 动画待做
- Terminal PHOSPHOR P3（2026-11-11）：Idle/Working motion profile、LUT、1s 过渡、activity 低通、demo 6s 循环；PC golden `p3_idle.png`/`p3_working.png`；真机 Idle 7 FPS / Working 7 FPS（Working 目标 10–15 未达，留 P5）；host tests 3613/3613；无 panic/OOM，Wi-Fi/Node 在线
- Terminal PHOSPHOR P3.1（2026-11-11）：Atlas 增加 `0/1`；Outer 采样 64→96，字符集 `+#%@01`，Working 300ms bucket 二进制流（约 3.3Hz，45% 比例）；外围稀疏 0/1 点；Working density 0.85、速度约 +13%；Idle 6–7 FPS / Working 7 FPS；host tests 3619/3619；截图 `p3_idle_v2.png`/`p3_working_v2.png`
- Terminal PHOSPHOR P3.2（2026-11-11）：删除外围零散 0/1 点；Outer 增加连续内侧副带（radius 176、96 采样、`0/1/+/%`、warning 缺口、FIELD），形成约 2-cell 厚外圈；Inner 64→96 采样、`;:;:;+;`、radius 80 不变；Middle 不变；host tests 3620/3620；Idle 6–7 FPS / Working 7 FPS；截图 `p3_idle_v3.png`/`p3_working_v3.png`
- Terminal PHOSPHOR P3.3（2026-11-11）：改用相邻 cell 加厚（Outer 向心 +23 cells、Inner 向外 +6 cells）；warning gap 0.22→0.11 rad 并低亮度填充，Middle gap 0.23→0.04 rad 并填充；Inner 在 Middle/Outer 后绘制；host tests 3624/3624；v3→v4 changed pixels 约 5.5–5.7k；Idle 6 FPS / Working 7 FPS；截图 `p3_idle_v4.png`/`p3_working_v4.png`
- Terminal PHOSPHOR P3.4（2026-11-11）：核心密度场 30–78→48–64、强度 170→108–120、1/17 稀疏；Inner 外侧 band 110–150；Middle 强度 215；外围 P3.3 保持；中心 r<80 非空 cell 35→26（-25.7%）；host tests 3624/3624；Idle 6–7 FPS / Working 7 FPS；截图 `p3_idle_v5.png`/`p3_working_v5.png`
- Terminal PHOSPHOR P3.5（2026-11-11）：Outer 主带 `0101+#%@=/<>` + bucket 平移（Working 300ms / Idle 800ms），`%` 8.3%、0/1 33.3%；Outer band `0101+#%@@` 平移；字符成串沿轨道运输，不再只有 `%`；v5→v6 changed pixels 7.8–8.2k；Idle 7 FPS / Working 7 FPS；截图 `p3_idle_v6.png`/`p3_working_v6.png`
- Terminal PHOSPHOR P3.6（2026-11-11）：Outer 拆水层/漂浮物双层；水层 Secondary `0/1/-/=/\/\\` 60%、intensity 230；漂浮物 Highlight `#@+o/%` 40%、两个 4-cell packet、intensity 255；Middle 改 Primary；Outer band 仅水层；v6→v7 changed pixels 16.2–16.8k；Idle 7 FPS / Working 7 FPS；截图 `p3_idle_v7.png`/`p3_working_v7.png`
- Terminal PHOSPHOR P4（2026-11-11）：新增 state resolver（offline > idle/working，approval/error 可叠加，completed 900ms pulse 按 event_id 去重，error glitch 300ms，4 satellites + overflow 稳定 slot）；demo 22s 六态循环；PC keys 1–6；六态 golden `p4_idle/working/approval/error/offline/completed.png`；host tests 3648/3648；真机 FPS≈7；内部 free 49.4–64.9KB；无 panic/OOM
- Terminal PHOSPHOR P4b（2026-11-11）：重写 state visual 层（旧版薄 overlay 验收失败）；Approval Outer 0.65 rad 缺口 + 2 个 `?`；Error 局部 5 个红色断裂字符 + 220ms glitch，无残留 `?`；Completed 3 层径向波 + 8 粒子；Offline 轨道解体 + `NO LINK` + `LAST SYNC --`；4 sessions demo；顶部 state label；contact sheet `p4_contact.png`；host tests 3683/3683；真机 FPS≈7，Wi-Fi/Node 在线
- Terminal PHOSPHOR R1/R1.1（2026-11-11）：按规格书从头重构为 state-first 核心，取消 base+overlay；visual state 直接生成轨道/缺口/断裂/解体/波；Approval gap 1.35 rad（77°）+ 2 `?`；Error gap 1.55 rad（89°）+ 7 红字 shards；Completed 3 层 16/24/32 波 + 8 粒子；Offline 103 cells 解体 + NO LINK + LAST SYNC；4 sessions + state label；r1_contact.png 六态已验收；pixel diff Approval 8,171 / Error 8,558；host tests 3693/3693；build-only app 1,284,080 B；**未烧录**，R2 motion/flash 待做
- Terminal PHOSPHOR R2（2026-11-11）：motion 加到 state-first core；Idle/Working 0.08/-0.045/0.025 与 0.22/-0.55/0.32 rad/s、600ms transition；Approval 1.75s 呼吸、Error 220ms glitch、Completed 900ms 环收束+三层波+8 粒子、Offline 550ms 降速冻结/恢复；4 sessions 30s demo；**app-flash 成功** 1,284,512 B / 16% free；70s 日志 2 个完整周期、两次 Offline→Idle、无 panic/OOM、Wi-Fi/Node 在线；host tests 3708/3708；FPS 6–7，P5 待做
- Terminal network QA fixes（2026-11-11）：F1 welcome 空/缺失 events、F3 stale pairing code、F5 mock stop、F7 字符串 NUL、F8 heartbeat 时序、F10 node_url 变化清 token 已合并；组合固件 0x135f00 / 17% free；启动日志 token=set、pairing_code=empty、welcome 49KB、snapshot applied、heartbeat RTT 298ms；host tests 3589/3589；F9/F4/F6/F2 仍待后续
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
