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
- `send_action` 仍未实现，只打印一次警告。

## 2. NVS 鉴权

namespace `meet_wifi` 新增：

```text
client_token    string（welcome 返回 token 后保存）
client_id       string
last_sequence   u64
pairing_code    继续复用 E4a key
```

- token 存在时 `hello` 只发 token；否则发 pairing_code。
- token 无效：清 token；若有 pairing_code，重试一次 pairing；否则退避。
- 日志只打 `token=set/empty`、`pairing_code=set/empty`，不打印值。

## 3. 47KB welcome 内存策略

- WebSocket buffer：1024 bytes；分片累积到 **128KB PSRAM** 原始缓冲。
- 不把原始 welcome 直接交给 cJSON。
- 在原始 JSON 上做字符串/转义/嵌套感知的扫描：
  - 找到 `snapshot.events` 数组；
  - 统计事件数；
  - 将数组内容原地替换为 `[]`（memmove，只缩小）。
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

- [x] token 无效 → `auth_failed` → 清 token（bogus token 实测）
- [ ] token 无效 + 有效 pairing_code 自动重试
- [x] `replay_truncated=1` 时以 snapshot 重建（Node 重启实测；后续 replay 事件未验证）
- [ ] 连续 3 次 heartbeat 无回包触发重连（未主动断 WS）
- [ ] 事件 apply/replay（E4b-2）
- [ ] 命令/审批发送（E4b-2）
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
- 事件：E4b-1 只记录 `type/seq/agent/session`，不修改 ViewModel；E4b-2 再做 apply/replay。

## 6. 真机验证

- 主机保持 iPhone 热点 `172.20.10.5`，**未切换 Wi-Fi**。
- ESP32 STA `172.20.10.2`。
- NVS 预置前备份：`/tmp/opencode/nvs-pre-e4b.bin`（24,576 bytes，
  SHA-256 `e5159daf...eb0a241`）。
- 首次 pairing 日志：`/tmp/opencode/e4b1-firstpair2.log`
- token 重启 + Node 重启日志：`/tmp/opencode/e4b1-token-reboot.log`
- `/status` 仍正常，`node_reachable=true`。

### E4b-2 待办

- event apply / replay / sequence 补偿
- command gateway：session.prompt / interrupt / permission.respond
- token 轮换/撤销、无效 token + pairing retry 验证
- 多客户端一致性、WS 心跳失联强制测试
