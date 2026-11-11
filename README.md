# MeetAgent V2.1

面向 Coding Agent 的本地优先控制与管理平台。

**Node-Centric · Multi-Client · Plugin-Based · LAN First · Cloud Optional**

当前状态：**V2.1 核心链路已贯通**。Node 独立服务、V2.1 客户端协议、配对、事件中心、命令/权限、审计与限流已完成；Mobile 已完成 OpenCode 真实 Session、Tailscale 与 Android 真机验收（Prompt、Interrupt、权限审批、断网恢复、Node 重启恢复），并完成 UX P0/P1/P2 改造（信息层级、聊天式详情、统一开发者信息、原生触感/键盘/返回键/下拉刷新/Sheet/视觉打磨）。Terminal 已完成 E0–E3a，并完成 **Waveshare ESP32-S3-Touch-LCD-4 V4** 真机 bring-up、E4a Wi-Fi 配网、E4b-1 Node WebSocket、E4b-2 事件接入、E4b-3 command 链路、PHOSPHOR CORE P1/P2/P3.1 与网络 QA 修复：ESP-IDF 5.5.5、480×480 RGB LCD、GT911、CH32V003、LVGL 8.4；STA 已连接手机热点（172.20.10.2），hello/welcome/snapshot、token NVS、心跳/重连、事件 apply/replay、`session.list` 命令已验证；PHOSPHOR P3.1 使用 24×16/18×27 Bold 字符、432×432 canvas、三条轨道、2× `>_`、两个 Satellite、密度/颜色/层级、Idle/Working motion、Outer `0/1` 二进制流与外围稀疏数据点；真机 Idle 6–7 FPS、Working 7 FPS（目标 10–15 未达，留 P5），固件 1.27 MB；P4 六态、P5 性能、interrupt/permission 实机、AP portal 保存仍待后续。

> 设计文档：
> - [`MeetAgentV2.1.md`](./MeetAgentV2.1.md) — 总体架构、双客户端、南向插件、阶段计划
> - [`TerminalUi.md`](./TerminalUi.md) — ESPUI 与 Terminal Aesthetic 主题系统
> - [`docs/v2.1/EXECUTION.md`](./docs/v2.1/EXECUTION.md) — 技术总监执行计划与模块边界
> - [`docs/v2.1/PROTOCOL.md`](./docs/v2.1/PROTOCOL.md) — Node ↔ Mobile/Terminal 协议
> - [`docs/v1/README.md`](./docs/v1/README.md) — 已归档的 V1 Cloud 方案

---

## 架构

```text
Mobile (Vue + Capacitor)        ESP32 Terminal (ESP-IDF + LVGL)
              \                /
               Client Protocol v2
                     |
                MeetAgent Node
           (standalone HTTP/WSS)
                     |
        ┌────────────┼────────────┐
        |            |            |
   Domain Core  Client Registry  Event Hub
        |
   Adapter Registry
        |
  OpenCode / Mock / Agent B
```

- **Node** 是核心：Agent 接入、Session、事件、命令、权限、配对、客户端注册
- **Mobile** 是完整远程入口：LAN/Tailscale 直连，M4 多客户端与 Node 重启恢复已验证，Capacitor Android debug APK 已完成真机配对与 Prompt 回传
- **Terminal** 是局域网常驻终端：E0–E3a 已完成，LCD-4 V4 官方 BSP + 480×480 构建通过；待实物显示/触摸验收
- **Cloud** 不再是必需项；旧 Cloud 代码保留为可选 relay
- **Adapter** 通过统一 Manifest/Capability 接入，OpenCode 只是第一个 Adapter

---

## 模块

| 模块 | 目录 | 状态 |
|---|---|---|
| Core / Node | `cmd/node`, `internal/`, `pkg/`, `configs/` | M0/M1/M4 完成；审计、限流、重连边界加固 |
| Node 模拟器 | `cmd/simulator` | 可用 |
| Mobile | `mobile/` | M5b + UX P0/P1/P2 完成；Android 真机 Prompt/Interrupt/审批/断网恢复/原生交互已验证 |
| ESP32 Terminal / ESPUI | `firmware/terminal/` | E0–E3a + LCD-4 bring-up + E4a Wi-Fi + E4b-1/2/3 Node + PHOSPHOR P1/P2/P3.1 + net QA fixes | P4 六态、P5 性能、interrupt/permission 实机、AP portal `/save` |
| V1 Cloud / Web Console | `cmd/cloud`, `web/test-client` | 归档参考 |

---

## 快速开始（Mock）

不依赖 OpenCode、Cloud 或硬件：

```bash
# 1. 启动带 Mock Agent 的 Node
make run-node-mock

# 日志会打印配对码，例如：
#   msg="pairing code ready" code=394631

# 2. 连接协议模拟器（另开终端）
go run ./cmd/simulator -pair-code 394631 -prompt "hello mock"

# 输出的 token 可复用：
go run ./cmd/simulator -token <token> -last-sequence 10
```

默认端口：

- Node HTTP/WSS：`0.0.0.0:8787`
- WebSocket：`ws://127.0.0.1:8787/ws`
- 健康检查：`http://127.0.0.1:8787/healthz`
- 发现信息：`http://127.0.0.1:8787/api/v2/info`

---

## 快速开始（真实 OpenCode）

```bash
export OPENCODE_PASSWORD="$(opencode service get password)"
make run-node            # configs/node.yaml
```

`configs/node.yaml` 默认：

- 本地服务 `0.0.0.0:8787`
- Cloud relay `cloud.enabled: false`
- OpenCode Adapter `opencode-main`
- mDNS：`_meetagent._tcp`
- 配对存储：`./data/paired-clients.json`（已 gitignore）

---

## Web 界面

V2.1 的 Web 客户端就是 `mobile/` 里的 Vue 应用，和 Android App 共用同一套界面与协议：

```bash
cd mobile
npm install
npm run dev
# 浏览器打开 http://127.0.0.1:5174
```

远程访问时可打开 `http://100.69.235.2:5174`（Node 已允许 Tailscale CORS）。连接页填写 Node 地址和配对码即可。

> `web/test-client/` 是 V1 Cloud 调试控制台，仍然保留在仓库中，但只走旧的 Cloud 协议，**不连接 V2.1 Node**，仅作归档参考。

---

## 配置

### `configs/node.yaml`

```yaml
node:
  id: "node-001"
  name: "developer-machine"
  agents:
    - id: "opencode-main"
      type: "opencode"
      connection:
        base_url: "http://127.0.0.1:49374"
        password_env: "OPENCODE_PASSWORD"

server:
  enabled: true
  addr: "0.0.0.0:8787"
  pairing_ttl: 10m
  permission_ttl: 10m
  pairing_store: "./data/paired-clients.json"
  allow_lan_origins: true
  mdns: true
  rate_limit_per_second: 100
  rate_limit_burst: 200

cloud:
  enabled: false
```

### `configs/node-mock.yaml`

不依赖外部服务的 Mock 配置：

```yaml
agents:
  - id: "mock-1"
    type: "mock"
    sessions:
      - id: "session-1"
        title: "Mock Session"
```

---

## 本地 WSS / 远程访问

生成本地自签证书（仅开发）：

```bash
make dev-tls
# data/tls/cert.pem, data/tls/key.pem
```

在 `configs/node.yaml` 的 `server` 中启用：

```yaml
server:
  tls_cert: "./data/tls/cert.pem"
  tls_key: "./data/tls/key.pem"
```

- 浏览器/手机必须信任该证书；自签证书只用于本机测试
- 生产环境用 Caddy/Nginx 终止 TLS，Node 可继续监听 `127.0.0.1`
- Mobile 填写 `https://<主机>:8787`，客户端会自动使用 `wss://`
- 公网部署请关闭 `allow_lan_origins`，只放行明确域名

### systemd user service（替代 Docker）

开发时可以用 `make run-node`；不用 Docker 时可安装 systemd user service：

```bash
make build
make install-service
```

服务会自动重启，OpenCode 密码保存在 `~/.config/meetagent/node.env`（权限 600）：

```bash
systemctl --user status meetagent-node
systemctl --user restart meetagent-node
journalctl --user -u meetagent-node -f
```

`install-service` 同时启用 linger，使服务在注销图形会话后仍保持运行。

### Docker 部署（当前方式）

Docker 与 systemd 二选一，不要同时启动（都会占用 `8787`）：

```bash
cp .env.example .env
# 编辑 .env：OPENCODE_PASSWORD=$(opencode service get password)

make docker-build
# 如果已安装 systemd 服务，先停掉：
sudo systemctl stop meetagent-node
make docker-up
docker compose ps
make docker-logs
```

首次加入 `docker` 组后需要重新登录；未重新登录前可临时使用：

```bash
make docker-up DOCKER="sudo docker"
```

- `compose.yaml` 使用 `network_mode: host`：这样容器能访问宿主机 `127.0.0.1:49374` 的 OpenCode，mDNS 也能正常广播
- `configs/node.yaml` 只读挂载，`./data` 持久化配对 Token
- 容器内以 UID 1000 运行，`./data` 需要当前用户可写
- 停止：`make docker-down`；数据保留在 `./data`

Node 暴露 Prometheus 文本指标 `GET /metrics`（本机访问免认证；远程访问需要 `admin_token`）：

```text
meetagent_ws_clients
meetagent_paired_clients
meetagent_agents{status="online"}
meetagent_events_published_total
meetagent_events_retained
meetagent_event_sequence
meetagent_command_results_total{status="completed"}
```

### 生产 TLS（反向代理）

公网部署时让 Caddy/Nginx 终止 TLS，Node 只监听 loopback：

```yaml
server:
  addr: "127.0.0.1:8787"
  allow_lan_origins: false
  allowed_origins:
    - "https://meetagent.example.com"
  admin_token: "replace-with-a-long-random-token"
```

仓库提供 Caddy 示例 [`deploy/caddy/Caddyfile.example`](./deploy/caddy/Caddyfile.example)；Caddy 会自动签发/续期证书，并把 HTTPS/WSS 反向代理到 `127.0.0.1:8787`。Mobile 填写 `https://meetagent.example.com` 即可自动使用 `wss://`。

## 协议与安全

- 客户端先通过 6 位配对码换取独立 Token；Token 只保存 SHA-256 哈希
- WebSocket 首包 `hello`，Node 返回 `welcome + snapshot`
- 事件带单调递增 `sequence`，重连时按 `last_sequence` 补发
- 命令带 `command_id`，Node 做幂等、权限、能力和超时校验
- 三种能力分离：Client Capability / Agent Capability / User Permission
- 终端首版只连 LAN；公网部署必须 `wss://`，并替换开发凭据
- 不要把真实 Token 写入仓库

详见 [`docs/v2.1/PROTOCOL.md`](./docs/v2.1/PROTOCOL.md)。

---

## 开发命令

```bash
make build            # 编译 meetagent-cloud / meetagent-node
make simulator        # 编译协议模拟器
make test             # go test ./... -race -count=1
make fmt
make vet
make run-node         # 真实 OpenCode 配置
make run-node-mock    # Mock Agent 配置
make run-cloud        # 旧 V1 Cloud（可选）
```

---

## V2.1 阶段

| 阶段 | 内容 | 状态 |
|---|---|---|
| M0 | 协议、Adapter Manifest、Mock、Simulator | 完成 |
| M1 | Node Standalone、配对、Client Registry、Snapshot/Event/Command | 完成 |
| M2 | Mobile M2 + Terminal E0–E2 | 完成（Mock / host tests） |
| M3 | 真实 OpenCode 集成 | Mobile 完成；Terminal 待真机 |
| M4 | 双端一致性与稳定性 | Core + Mobile 完成；Node 重启/缓存截断/握手补发边界已加固；Terminal 暂停 |
| M5 | Mobile 远程（Tailscale/Capacitor） | 完成（Tailscale 实网 + WSS/TLS + Android debug APK 真机验证；iOS/Release 待环境） |
| M6 | 语音/STT、Cloud Relay、第二 Adapter、OTA | 暂缓 |

> 语音/STT 按当前要求暂缓。ESP32 硬件未到，但 ESP-IDF v5.4.4 与 esp32s3 工具链已安装，Terminal 正在做首次真实编译；Node 与 Mobile 继续推进，Mobile 已完成 M4、M5a/M5b（Tailscale、WSS/TLS、网络切换、诊断、Android debug APK）。
