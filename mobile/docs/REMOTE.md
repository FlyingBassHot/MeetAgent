# MeetAgent Mobile 远程连接指南（M5a）

> 范围：Tailscale / 反向代理 WSS / 证书信任 / 远程地址填写。
> 当前本机没有 Android SDK、adb 或 iOS Xcode 环境，Capacitor 原生打包未在本轮验证。

## 1. 拓扑

```text
Mobile（浏览器或 Capacitor WebView）
        │  HTTPS / WSS
        ▼
Tailscale（加密网络，100.x / MagicDNS）
        │
        ▼
Node 主机
  ├── meetagent-node :8787（本地 HTTP/WS）
  └── TLS 反向代理 :443 / :8789（WSS → 本地 8787）
```

- Mobile 只需要填写 Node 的 **HTTP(S) 基地址**，例如 `https://node.tailnet.ts.net`。
- Mobile 自动派生 `wss://<host>/ws`；不要手动填写 `/ws`。
- 远程访问时应同时使用 HTTPS 托管 Mobile 页面，避免 Token 与界面资源走明文 HTTP。
- Node 仍负责 Agent、Session、事件与审批；Cloud 不参与执行权限。

## 2. Tailscale 准备

Node 主机与手机安装 Tailscale 并登录同一 tailnet：

```bash
tailscale up
tailscale status
tailscale ip -4
```

建议使用 MagicDNS 主机名（如 `node.tailnet.ts.net`），避免依赖 100.x 地址。
ACL 至少允许手机访问 Node 主机的 HTTPS/WSS 端口；Node 不需要暴露到公网。

## 3. Node 侧 TLS / 反向代理

### 方案 A：Tailscale 证书 + Caddy（推荐）

先在 Tailscale 管理台启用 HTTPS 证书，并生成证书：

```bash
tailscale cert node.tailnet.ts.net
# 生成 node.tailnet.ts.net.crt / node.tailnet.ts.net.key
```

Caddyfile：

```caddyfile
https://node.tailnet.ts.net {
    tls /etc/ssl/node.tailnet.ts.net.crt /etc/ssl/node.tailnet.ts.net.key
    reverse_proxy 127.0.0.1:8787
}
```

Caddy 自动处理 WebSocket 升级。`*.ts.net` 证书由 Tailscale 签发，不是公共 ACME 证书，必须显式配置 `tls` 指令或手动安装信任。

### 方案 B：Nginx

```nginx
server {
    listen 443 ssl;
    server_name node.tailnet.ts.net;

    ssl_certificate     /etc/ssl/node-fullchain.pem;
    ssl_certificate_key /etc/ssl/node-key.pem;

    location / {
        proxy_pass http://127.0.0.1:8787;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_read_timeout 3600s;
    }
}
```

必须保留 `Upgrade` / `Connection` 头，否则 `/ws` 无法升级为 WebSocket。

### 方案 C：Node 直接 TLS（仅测试）

本轮使用自签证书 Node：

```text
https://127.0.0.1:8789
证书：/tmp/opencode/tls-test/cert.pem（CN=127.0.0.1，自签，仅本机测试）
```

生产环境不要使用 `--ignore-certificate-errors`。

### CORS

Node 的 `allowed_origins` 需要包含 Mobile 页面实际来源，例如：

- 本机开发：`http://localhost:5174`、`http://localhost:4173`
- 远程 Web：`https://mobile.example.com`
- Capacitor：按 WebView 实际 origin 配置

## 4. Mobile 填写示例

| 场景 | Node 地址 |
|---|---|
| 同机测试（TLS） | `https://127.0.0.1:8789` |
| 家庭 LAN | `https://192.168.1.10:8789` |
| Tailscale + Caddy | `https://node.tailnet.ts.net` |
| 自建反向代理 | `https://meetagent.example.com` |

步骤：

1. 在 Node 日志中获取 6 位配对码（TTL 10 分钟）。
2. Mobile 选择「真实 Node」，填写地址与配对码，连接。
3. 配对成功后 Token 保存在当前浏览器；Capacitor 版本需迁移到 Keychain / Keystore。
4. Settings → 连接诊断可查看 `navigator.onLine`、`ws/wss` scheme、连接阶段与最后错误。

## 5. 证书信任

- 公网 CA 或 Tailscale 证书：通常无需额外操作。
- 自签证书：
  - 桌面 Chromium 测试可临时使用 `--ignore-certificate-errors`（只限本机测试）。
  - Android：将 CA 安装到系统信任存储；Android 7+ 默认不信任用户 CA，Capacitor 需配置 network security config 或在系统层信任。
  - iOS：安装 CA 描述文件后，在「设置 → 通用 → 关于本机 → 证书信任设置」中启用完全信任。
- 浏览器通常不会把证书链细节暴露给页面；Mobile 只能显示通用提示：
  「WSS 需要有效证书；自签证书请先在系统/浏览器中信任」。

## 6. Capacitor 环境（未在本机安装）

Android：

- Android Studio + Android SDK + platform-tools（`adb`）+ JDK 17
- `npm i @capacitor/core @capacitor/cli`
- `npx cap init MeetAgent com.example.meetagent --web-dir=dist`
- `npx cap add android && npx cap sync`
- 生产使用 WSS；开发如需 HTTP 明文，只能通过 debug network security config 放开，不要进入发布包
- Token 存入 Android Keystore（通过 Capacitor Preferences / 自定义插件）
- 系统通知需要 Android 13+ 通知权限与 FCM（后续）

iOS：

- macOS + Xcode + CocoaPods
- `npx cap add ios && npx cap sync`
- ATS 默认禁止非 TLS；WSS 域名需可信任证书
- Token 存入 Keychain
- APNs 需要推送证书与 provisioning profile（后续）

本机当前没有 `adb` / Android SDK，因此本轮只验证 Web 浏览器与 WSS；未执行 `npx cap add`、Gradle 构建或真机安装。

## 7. 诊断命令

```bash
# 检查 TLS 与 Node 信息（自签证书用 -k）
curl -k https://127.0.0.1:8789/api/v2/info

# 查看证书主体与有效期
openssl x509 -in /tmp/opencode/tls-test/cert.pem -noout -subject -dates

# 检查监听
ss -tnlp | grep 8789
```

Mobile 连接诊断应显示：

- `navigator.onLine = true（在线）`
- `WebSocket scheme = wss`
- 连接阶段 `online · 握手完成`
- 最后错误为空；若证书不受信任，会显示通用证书提示

## 8. 网络切换行为

- `window offline`：立即停止 WebSocket 与指数退避定时器，UI 展示「网络离线」，不继续重试。
- `window online`：取消退避计时并立即发起一次重连，避免与旧退避叠加。
- 断线重连仍携带最新 `last_sequence`；以 Snapshot 为准应用补发事件。

## 9. 安全注意事项

- 远程连接必须使用 WSS；不要在公网使用明文 `ws://`。
- 每台设备单独配对、独立 Token，可单独撤销；不要共享长期 Token。
- 自签证书只用于测试；正式环境使用受信任 CA 或 Tailscale 证书。
- Tailscale 只提供网络可达性，不替代 Node 的配对与权限校验。
