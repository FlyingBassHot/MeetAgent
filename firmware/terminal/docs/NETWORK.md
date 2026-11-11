# Wi-Fi 配网与 Node 可达性（E4a）

日期：2026-11-11  
范围：`firmware/terminal/components/networking/`  
状态：STA + Node `/api/v2/info` 真机通过；AP portal 的 `/save` 实机保存需第二台设备确认

## 1. 范围

E4a 只做：

- Wi-Fi SSID/密码、Node URL、可选 pairing code 的 NVS 保存
- 无配置时开放 SoftAP + HTTP 配网页
- 有配置时 STA 连接，失败 3 次回退 SoftAP
- STA 断线重连、周期性刷新 DHCP IP
- STA 成功后 HTTP GET `<node_url>/api/v2/info`

不做：WebSocket、pairing、snapshot、Event、Command、Token 持久化（E4b）。

## 2. NVS

namespace 与 key 必须与固件完全一致：

```text
namespace : meet_wifi
wifi_ssid : string
wifi_pass : string
node_url  : string
pairing_code : string（可空）
```

- 密码与 pairing code 只存 NVS；日志只打印 `set` / `empty`，绝不打印值。
- `/status` 不返回密码、pairing code 或 token。
- NVS 默认未加密；能物理读取 Flash 的人可以读取凭据。生产前应结合 Flash encryption/NVS encryption。

## 3. 启动流程

```text
app_main
  └─ nvs_flash_init()
       └─ meet_wifi_prov_start()
            ├─ NVS 有 ssid + node_url
            │    └─ STA 连接（最多 3 次 × 15s）
            │         ├─ 成功：获取 IP → 启动 :80 /status → GET /api/v2/info
            │         │        └─ 持续监控断线（30s 轮询 IP），断线重连
            │         └─ 3 次失败：回退 SoftAP + portal
            └─ 无配置
                 └─ SoftAP "MeetAgent-<MAC后4位>" + portal
```

- SoftAP 为开放热点（无密码），SSID 例：`MeetAgent-746C`。
- AP 模式 IP：`192.168.4.1`，HTTP 端口 80。
- STA 模式也会启动同一个 HTTP server：`/status` 可用于验证；`/save` 在 STA 模式返回 403，仅允许 SoftAP 配网。
- STA 重连失败 3 次后才回退 SoftAP；成功连接后不会因为一次短断线就永久放弃。

## 4. Portal

```text
GET  /        配网 HTML 表单
POST /save    保存 NVS → 返回“已保存，设备重启” → esp_restart
GET  /status  JSON：wifi_state/ssid/ip/node_url/has_config/node_reachable/...
```

表单字段：

```text
ssid          必填，≤32
password      可空（开放网络），≤64
node_url      必填，http:// 开头，≤127
pairing_code  可选，≤32
```

- 实现按 NUL 结尾复制到 `wifi_config_t`，SSID 有效上限 31 字节，密码有效上限 63 字节；常规手机热点不受影响。
- `node_url` 必须是 Node 的 LAN 地址，例如 `http://<host-lan-ip>:8787`。
- 修改 `node_url` 会清除旧 Node 的 token/client_id/last_sequence；需要重新配对。
- ESP32 只连 Node，不直接访问 OpenCode。
- `/save` 只在 AP / AP_FALLBACK 状态接受；STA 模式返回 403。
- 保存成功后延迟 800 ms 重启。

### 开放热点风险

- AP 无密码，附近设备可以连接配网页；仅在首次配网或 3 次 STA 失败回退时开启。
- 配网页不提供 TLS；Node URL 本阶段也仅支持 `http://`。
- 保存后设备重启进入 STA；不会长期停留在开放 AP（除非 STA 连续失败）。

## 5. Node 可达性

STA 获取 IP 后：

```http
GET http://<node_url>/api/v2/info
```

- 最多 3 次，每次间隔 2 秒，超时 5 秒。
- 日志只打印 `name/version/protocol/schema`，不打印 Token。
- 成功时 `/status` 的 `node_reachable=true`。
- Node 不可达不会断开 Wi-Fi；日志会记录 HTTP `err/status/read`，便于排查 ARP/路由/防火墙。

## 6. 清除 NVS 重新配网

只清 NVS 分区（offset `0x9000`，size `0x6000`）：

```bash
esptool.py --port /dev/ttyACM0 erase_region 0x9000 0x6000
```

然后复位设备。不要 `erase-flash`（会擦除 app/bootloader/分区表；原厂 Flash 备份在
`/home/perch/esp/backups/`，不要覆盖）。

STA 连续失败 3 次也会回退 SoftAP，允许重新配网。

## 7. 真机验证方式

由于主机必须始终保持手机热点（模型连接依赖主机网络），**没有让主机切换 Wi-Fi**。

验证方式：NVS 预置。

- 先备份现有 NVS：`read_flash 0x9000 0x6000 /tmp/opencode/nvs-backup.bin`
  （24,576 bytes；SHA-256 `bff03118...7fa549f1`）
- 用 IDF `nvs_partition_gen.py` 生成 0x6000 镜像，namespace/key 与固件一致
- 只写 `0x9000`，不 erase-flash，不动分区表/0x10000 之后 app
- 主机保持在手机热点，Node URL 使用主机当前 LAN IP `http://172.20.10.5:8787`

结果：

```text
ESP32 STA IP: 172.20.10.2
Node:         developer-machine / 2.1.0 / protocol=1 / schema=1
ESP32 /status: wifi_state=connected, node_reachable=true
```

`/status` 实测：

```json
{"wifi_state":"connected","ip":"172.20.10.2","node_url":"http://172.20.10.5:8787","has_config":true,"node_reachable":true,"node_name":"developer-machine","node_version":"2.1.0","protocol_version":1,"schema_version":1}
```

- 无 `password`/`pairing_code` 字段。
- STA 模式 `POST /save` 返回 **403**（只允许 SoftAP）。
- 重启（monitor DTR hard reset，非物理断电）后自动从 NVS 连接，再次 `node_reachable=true`。
- 约束后主机保持 `172.20.10.5`；约束前曾短暂连接 AP 读取 `/` 与 `/status`（未提交 `/save`），随后立即恢复手机热点。未使用校园网 SSID（需网页认证）。
- 主机 ufw 已由用户放行 `172.20.10.0/28 → TCP 8787`。

### AP portal 实机验证状态

- 在收到“主机不得切换 Wi-Fi”约束前，曾短暂连接 AP 读取 `/` 与 `/status`；**未提交 `/save`**，随后立即恢复手机热点。
- 最终 STA/Node 验证全部使用 NVS 预置，未再切换主机网络。
- AP portal 的 `POST /save` 实机保存尚未验证；由用户用第二台设备按下面步骤确认。

### 第二台设备验证 Portal

1. 让设备处于 AP（无配置或 STA 连续失败回退），SSID 为 `MeetAgent-XXXX`（开放）。
2. 第二台手机/平板连接该 AP。
3. 打开 `http://192.168.4.1/`。
4. 填写 Wi-Fi SSID/密码、Node URL、可选 pairing code。
5. 提交后页面显示“Saved”，设备重启。
6. 串口应出现 `STA got IP`、`Node reachable`；`GET http://<sta-ip>/status` 返回 connected。
7. 验证后第二台设备断开 AP，不要影响主机网络。

## 8. 未验证 / 风险

- [x] STA 连接手机热点、DHCP IP、Node `/api/v2/info`
- [x] STA `/status`、`/save` 403
- [x] 重启后 NVS 自动重连、再次 Node 可达
- [x] 主机网络未切换、未使用校园网 SSID
- [ ] AP portal `POST /save` 实机保存（需第二台设备）
- [ ] 强制断线重连（代码有 30s 监控 + 重连循环；未主动断开热点验证）
- [ ] DHCP 短租约续租（lwIP 自动续租；未等待租约到期）
- [ ] iPhone 热点客户端隔离/防火墙异常场景（本次 Node 可达）
- [ ] NVS 加密 / Flash encryption
- [ ] E4b WebSocket/pairing/snapshot/event

## 9. 依赖

```text
nvs_flash, esp_wifi, esp_netif, esp_event,
esp_http_server, esp_http_client, json
```

E4a 未引入 `esp_websocket_client`；E4b-1 已引入（1.8.0）。


---

# E4b-1：真实 Node WebSocket 接入（2026-11-11）

## 1. 依赖与连接

```text
espressif/esp_websocket_client 1.8.0
WS URI: ws://<node-lan-ip>:8787/ws
client_type: terminal
client_version: 0.1.0
name: MeetAgent Terminal
capabilities: ui.compact, display.touch, input.button, input.encoder,
              notification.local
```

- `net_client_stub.c` 已替换为真实 `net_client.c`。
- `net_client` 等待 `meet_wifi_prov_get_status()` 为 connected 且 node_url 非空。
- `http://host:port` → `ws://host:port/ws`；E4b-1 不支持 https。
- E4b-1 阶段 `send_action` 未实现；E4b-3 已实现 interrupt / permission.respond / session.list。

## 2. NVS 鉴权

namespace `meet_wifi` 新增：

```text
client_token    string（welcome 返回 token 后保存）
client_id       string
last_sequence   u64
pairing_code    继续复用 E4a key；仅在首次配对时使用
```

- token 存在时 `hello` 只发 token；否则发 pairing_code。
- welcome 保存 token 后清除 NVS `pairing_code`（服务端配对码成功即轮换，旧码不可复用）。
- token/pairing 认证失败：清 token 和 pairing_code，进入“需要重新配网”状态，**不自动重试**旧码。
- 修改 `node_url` 时清除 `client_token/client_id/last_sequence`，避免把旧 Node 的 token 发给新 Node。
- 日志只打 `token=set/empty`、`pairing_code=set/empty`，不打印值。

## 3. 47KB welcome 内存策略

- WebSocket buffer：1024 bytes；分片累积到 **128KB PSRAM** 原始缓冲。
- 不把原始 welcome 直接交给 cJSON。
- 在原始 JSON 上做字符串/转义/嵌套感知的扫描（`welcome_elision.c`，host 可测）：
  - 找到 `snapshot.events` 数组；
  - 统计事件数；
  - 将数组内容原地替换为 `[]`（memmove，只缩小）。
- `events` 缺失且 welcome >20KB 才拒绝；`events: []` 放行（不能把“无回放事件”误判为损坏）。
- 之后 cJSON 只解析约 3KB 的 node/agents/sessions/pending_permissions。
- 大缓冲与 snapshot 结构均 `heap_caps_*` PSRAM；cJSON 小对象用内部 RAM。
- 42 sessions 安全截断到 8（ViewModel 容量）；日志记录 `sessions_stored/total`。

验证结果：

```text
first pairing: welcome 50,341 bytes, parse 10 ms, events_skipped=100
client_id: cli_b8aaf428f6ad3073
permissions: session.read, session.control, permission.respond
token reboot : welcome 50,287 bytes, parse 10 ms, events_skipped=100
node restart : welcome 32,615 bytes, parse  9 ms, events_skipped=45
sessions      : 8/42 (stored/total), pending=0, permissions=3
memory        : internal_free=65,895 B, psram_free=6,516,920 B
```

## 4. 握手/配对待验证项

已验证：

- [x] pairing_code → welcome → token=set（首次配对）
- [x] token 重启 → welcome → snapshot，无需 pairing code
- [x] client_id、permissions、node name/version/protocol
- [x] snapshot 应用后 `meet_mock_state_stop()`，Mock 不再覆盖 ViewModel
- [x] Node 重启后 WS 自动重连（token 仍有效）
- [x] heartbeat 15s，RTT 59ms（首次）/53ms（Node 重启后）
- [x] 无 panic/assert/OOM

未验证：

- [x] token 无效 → `auth_failed` → 清 token/pairing_code（代码实现，待真机）
- [x] 不再自动重试已轮换的 pairing_code；需重新配网（代码实现，待真机）
- [x] `replay_truncated=1` 时以 snapshot 重建（Node 重启实测）
- [x] replay 重建后的后续事件 apply（seq 46–52 实测）
- [ ] 连续 3 次 heartbeat 无回包触发重连（未主动断 WS）
- [x] 事件 apply / sequence 去重（E4b-2 实测）
- [x] 命令发送链路 / session.list 安全刷新（E4b-3）
- [ ] interrupt / permission.respond 真机链路
- [ ] 多客户端、权限撤销、token 轮换

## 4.1 2026-11-11 review fixes

- Event envelope 字段从 `payload` 读取；日志现在输出真实事件类型/sequence。
- Error envelope 字段从 `payload` 读取；`auth_failed` / `invalid_pairing_code` /
  `pairing_expired` 会触发 token 清除与（如有）pairing 重试。
- `wifi_connecting` 只回调一次，不随 wait 循环刷屏。
- stop 生命周期由 net_task 独占清理，调用者只请求停止并等待最多 2s。

## 5. 重连/心跳

- WS close/error → 1/2/5/10/30s 指数退避。
- 重连 hello 带 NVS `last_sequence`；成功 welcome 后重置退避。
- Wi-Fi 未连接时等待 wifi_prov 恢复。
- 每 15s 发送：
  ```json
  {"schema_version":1,"type":"heartbeat","payload":{"nonce":"hb-..."}}
  ```
- 连续 3 个心跳周期无回包 → 关闭 WS 重连。
- 事件：E4b-1 只记录 `type/seq/agent/session`；E4b-2 起由 State Adapter apply 到 ViewModel。

## 6. 真机验证

- 主机保持 iPhone 热点 `172.20.10.5`，**未切换 Wi-Fi**。
- ESP32 STA `172.20.10.2`。
- NVS 预置前备份：`/tmp/opencode/nvs-pre-e4b.bin`（24,576 bytes，
  SHA-256 `e5159daf...eb0a241`）。
- 首次 pairing 日志：`/tmp/opencode/e4b1-firstpair2.log`
- token 重启 + Node 重启日志：`/tmp/opencode/e4b1-token-reboot.log`
- `/status` 仍正常，`node_reachable=true`。

### E4b-2 已完成

- event apply / replay / sequence 去重
- snapshot session active-first 截断
- command gateway 已在 E4b-3 实现（interrupt / permission.respond / session.list）；session.prompt 不做
- token 轮换/撤销、无效 token + pairing retry 验证
- 多客户端一致性、WS 心跳失联强制测试

---

# E4b-2：事件 apply / replay（2026-11-11）

## 1. State Adapter

`meet_state_adapter_apply_event()` 现在是真实实现，仍不依赖 cJSON：

```text
event.schema_version 必须为 1
sequence <= adapter.last_sequence  → ignored_events++，ESP_OK
否则 last_sequence = sequence
  agent.connected/disconnected/status_changed/error → upsert agent + 状态映射
  session.created/updated/status_changed          → upsert session
  session.deleted                                 → remove session
  permission.requested                            → Attention(APPROVAL)
  permission.resolved                             → resolve Attention
  tool.failed                                     → error_count（上限 99）
  message.completed                               → completion_count（上限 99）
  unknown                                         → 只推进 sequence + debug
```

- 业务字段由 `event->payload_json` 的最小 JSON 字符串提取器读取；支持基本
  `\" \\ / b f n r t` 转义，`\u` 跳过 4 位并输出 `?`；不分配内存。
- Session 变化后重算所属 Agent 的 `active_session_count`（只算 RUNNING /
  WAITING_APPROVAL）。
- 未知/流式事件（`message.started/delta`）也推进 sequence，但不改 ViewModel。
- ViewModel 新增 transient `error_count`/`completion_count`（上限 99），由
  `recount_attention` 与 approval 分开统计；Snapshot 会清零。

## 2. Net Client 集成

- `process_event()` 从外层 `payload` 取事件元数据，把内层 `payload` 对象
  `cJSON_PrintUnformatted` 后交给 State Adapter，随后立即释放。
- 只对 session/agent/permission、`message.completed`、`tool.failed` 触发
  ViewModel post；`message.delta` 不 post。所有 post 经过 250ms（最多 4Hz）
  脏标记节流，snapshot 仍立即投递。
- 事件序号：
  - `sequence <= last_sequence` 忽略（adapter 计数）；
  - 非连续但更大的序号打印 `sequence gap previous/current`，不强制重连
    （E4b-4 再决定补偿/重连策略）；
  - `s_last_sequence` 始终与 adapter 同步；WS 断开/退出时写 NVS。
- `replay_truncated=1`：以 welcome snapshot 全量重建 ViewModel，更新
  `last_sequence`，后续新序号事件继续 apply；旧序号忽略。
- Snapshot Session 截断：每个 Agent 第一遍存 RUNNING/WAITING_APPROVAL，第二遍
  存其余，直到全局 8 条；`sessions_total` 仍统计全部（当前 42），日志输出
  `sessions_stored/total`。

## 3. 真机验证

最终固件：

```text
app: build/meetagent_terminal.bin
size: 1,250,736 bytes (0x1315b0)
sha256: 084a8210fc26884b9771f6bea42cc763481f6f682e0e7b6042774918029b9270
app partition free: 0x45a50 (19%)
DIRAM: 199,455 / 341,760 (58.36%)
IRAM: 16,384 / 16,384（size 工具的固定窗口；链接断言通过）
```

启动 + 实时事件（`/tmp/e4b2-final.log`）：

```text
auth last_sequence=52
welcome 35,203 B, parse 11 ms, events_skipped=52
snapshot agents=1 sessions=8/42 seq=52
internal_free=65,763 B psram_free=6,516,232 B
event #53 agent.status_changed  took_us=138  active=0
event #54 session.status_changed took_us=213 active=1
event #57 message.completed      took_us=102 active=1 alerts=1
event #58 session.status_changed took_us=144 active=0 alerts=1
event #59 agent.status_changed   took_us=301 active=0 alerts=1
heartbeat RTT=77/131 ms
```

- `message.started/delta` 不产生 ViewModel post；`message.completed` 更新
  completion alert；agent/session 状态和 active 计数按事件变化。
- 无 sequence gap、panic、assert、OOM。
- Node 重启（`/tmp/e4b2-node-restart.log`）：WS 断开 → 1s 重连 →
  welcome 32,615 B / 9ms / events_skipped=45 / `replay_truncated=1`，
  snapshot seq 从 243 重置为 45，并成功 apply；随后 seq 46–52 事件继续
  apply（`/tmp/e4b2-events4.log`）。
- 日志只输出 type/sequence/计数/耗时，不输出 prompt/delta 文本。
- 并发修正前构建（`2fbd2f25...`）启动验证：NVS `last_sequence=59`，
  welcome 37,812 B / 11 ms / 59 events，snapshot seq=59，heartbeat 53 ms。

## 4. 未验证 / 留待 E4b-4

- 真机 permission.requested / permission.resolved（host tests 已覆盖）。
- 高频事件下 4Hz 节流的压力/丢帧表现。
- 主动制造 sequence gap 的补偿策略（当前只告警）。
- 连续 3 次 heartbeat 无回包强制重连。
- 多 Agent 时的全局 active-first 截断（当前仅单 Agent 实测）。
- UI 视觉更新需人工确认；E4b-3 已实现安全命令发送，未实现 session.prompt。


## 5. 并发修正（2026-11-11 review）

问题：`s_model` / `s_model_dirty` / `s_last_post_ms` 同时被 WS event 回调
（`process_welcome` / `process_event`）和 `heartbeat_loop` 的
`post_model_if_due()` 访问，可能在 apply 中途复制出撕裂的 ViewModel。

修正：

- 新增 `s_model_lock`，在 `meet_net_client_start()` 创建、`net_task_cleanup()`
  删除。
- `post_model_locked()` 假定已持锁；`post_model_if_due()` 取锁后调用。
  `process_event()` 在 apply + dirty + post + 计数读取期间持锁；
  `process_welcome()` 在 apply_snapshot + post + dirty 期间持锁；
  `heartbeat_loop()` 通过 `post_model_if_due()` 取锁。
- Snapshot post 改为单次非阻塞投递；失败置 dirty，由 heartbeat 的 250ms
  节流路径重试，避免持锁 `vTaskDelay`。
- `net_task_cleanup()` 先释放 `s_lifecycle_lock`，再把 `s_task = NULL`，
  避免 `meet_net_client_stop()` 在 cleanup 仍持 lifecycle mutex 时删除它。

修正后固件：

```text
size: 1,250,736 bytes (0x1315b0)
sha256: 084a8210fc26884b9771f6bea42cc763481f6f682e0e7b6042774918029b9270
app partition free: 0x45a50 (19%)
DIRAM: 199,455 / 341,760 (58.36%)
```

验证日志 `/tmp/opencode/e4b2fix-verify.log`：

```text
auth last_sequence=114
welcome 54,301 B / 11 ms / 100 events
snapshot agents=1 sessions=8/42 seq=114
internal_free=65,687 B psram_free=6,516,104 B
event #115 agent.status_changed  took_us=144
event #116 session.status_changed took_us=189 active=1
event #119 message.completed      took_us=99  alerts=1
event #120 session.status_changed took_us=341 active=0
event #121 agent.status_changed   took_us=337 active=0
heartbeat RTT=62/13 ms
```

无 panic/assert/OOM；host tests 974 checks、syntax 32 files 均通过。
stop 竞态修正目前为代码审查验证，尚无运行入口做真机 stop 压测。


---

# E4b-3：命令/审批发送（2026-11-11）

## 1. Action → Command

当前 `espui_action_t` 只有以下可发送动作，不扩展 prompt/text：

| Action | Command | 必填字段 | Inner payload |
|---|---|---|---|
| `ESPUI_ACTION_INTERRUPT_SESSION` | `session.interrupt` | `session_id` + `agent_id` | `{}` |
| `ESPUI_ACTION_RESPOND_PERMISSION` | `permission.respond` | `permission_id` + `session_id` + `agent_id` | `{permission_id, decision}` |
| `ESPUI_ACTION_REFRESH_STATE` | `session.list` | 无（agent 省略） | `{}` |

- `session_id` 来自 action（interrupt）或 Attention（permission）。
- `agent_id` 从 ViewModel 中该 Session 取。
- `s_model_lock` 只用于复制 id；释放后才 build/send，不在持锁路径发送。
- 缺失/未知 session/agent/permission、WS 非 online、表满都返回明确
  `esp_err_t` 并打印不含业务内容的日志。
- 不实现 `session.prompt`，也不新增 UI 输入字段。

命令外层：

```json
{
  "schema_version": 1,
  "type": "command",
  "message_id": "msg-terminal-<uptime>-<counter>",
  "payload": {
    "schema_version": 1,
    "command_id": "cmd-terminal-<uptime>-<counter>",
    "agent_id": "opencode-main",
    "session_id": "ses_...",
    "type": "session.interrupt",
    "payload": {}
  }
}
```

`command_id` / `message_id` 由 uptime + 单调 counter 生成，长度 < 64；发送前先登记
待处理表，避免极快 result 先于登记。

## 2. Pending table / result

- `components/networking/command_table.c`：纯逻辑、无分配、固定 4 条。
- `s_cmd_lock` 保护 `s_pending_commands`；start 创建、cleanup 删除。
- 每条：`command_id` + `espui_action_t` + `sent_ms`。
- `command.result` 按 `payload.command_id` 匹配并移除；重复/未知只告警。
- 状态映射：
  - `completed` → `ESPUI_ACTION_STATUS_OK`
  - `rejected` → `REJECTED`
  - `timeout` → `TIMEOUT`
  - `failed` / other → `FAILED`
- `message` 使用 result `error`；为空时使用 result status 文本。
- `session.list` 的 `data.sessions` 数组长度写入日志（本次 42）。
- heartbeat 250ms tick 检查 15s 超时；超时移除并回调 TIMEOUT。
- WS 断开/stop 时 drain 未完成命令，回调 FAILED `connection lost`；不跨重连重试。
- app_main 的 `on_action_result` 调用 `espui_post_action_result()`，theme footer
  显示 `ACK // ...` / `ERR // ...`。

## 3. 真机验证

最终固件：

```text
size: 1,255,696 bytes (0x132910)
sha256: 84374daaa18a1a4f661d72a8e96789a3449ddcab7458eb48cc72c3a7b7891aac
app partition free: 0x446f0 (18%)
DIRAM: 200,247 / 341,760 (58.59%)
```

安全命令使用临时诊断任务只在 WS online 后发送一次 `REFRESH_STATE`
（诊断构建 hash `6448647b...`；测试后已移除，最终固件默认不触发）：

```text
command sent cmd=cmd-terminal-00000e0d-00000001 type=session.list agent=- session=-
command result cmd=cmd-terminal-00000e0d-00000001 type=node.refresh status=completed sessions=42 duration_ms=0
app_main: action result: type=node.refresh status=1
heartbeat RTT=102 ms
```

最终固件回归（无诊断）：

```text
last_sequence=218
welcome 49,958 B / 10 ms / 100 events
snapshot agents=1 sessions=8/42 seq=218
internal_free=64,823 B psram_free=6,514,984 B
event #219 agent.status_changed  took_us=146
event #220 session.status_changed took_us=239 active=1
event #223 message.completed      took_us=86  alerts=1
event #224 session.status_changed took_us=268 active=0
event #225 agent.status_changed   took_us=281 active=0
heartbeat RTT=61/12 ms
```

Host tests：`1024 checks, 0 failures`；`syntax_check.sh` 33 files OK。
无 panic/assert/OOM。

## 4. 未验证 / 风险

- Interrupt 真机链路未构造运行中的 test session；未强行中断其他用户 session。
- permission.respond 真机未触发（当前无 pending permission）；代码审查通过。
- 15s 超时、断线 FAILED 的真实网络故障未构造；command table host tests 已覆盖
  超时/回绕/drain 逻辑。
- Footer 视觉未截图；已确认 action result 回调 status=OK 并进入 ESPUI 队列。
- 未实现 session.prompt / 文本输入；`send_action` 仅支持上述三个 action。
- WS 发送路径仍沿用现有 `send_text()` 保护；未新增独立 WS send 锁。
