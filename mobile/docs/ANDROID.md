# MeetAgent Mobile Android 构建指南（M5b + P1）

> 目标：用 Capacitor 把 `mobile/dist` 打包成 Android Debug APK。
> 本机已验证：JDK 21、Android SDK 35、Gradle 8.11.1、Capacitor 7.6.10。
> 已在 HBN-AL00（USB）安装并完成真机验证。

## 1. 版本与工具链

| 组件 | 版本 |
|---|---|
| Capacitor Core / CLI / Android | 7.6.10 |
| @capacitor/network | 7.0.4 |
| @capacitor/haptics | 7.0.5 |
| @capacitor/keyboard | 7.0.6 |
| @capacitor/app | 7.1.2 |
| @capacitor/status-bar | 7.0.6 |
| Java | OpenJDK 21.0.13 |
| Android SDK Platform | android-35 |
| Android Build Tools | 35.0.0 |
| Gradle Wrapper | 8.11.1 |
| Android Gradle Plugin | 8.7.2 |
| compileSdk / targetSdk | 35 |
| minSdk | 23 |
| appId | `com.flyingbasshot.meetagent` |
| appName | MeetAgent Mobile |

## 2. 环境变量

```bash
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk
export ANDROID_SDK_ROOT=/home/perch/Android/Sdk
export ANDROID_HOME=$ANDROID_SDK_ROOT
export PATH=$JAVA_HOME/bin:$ANDROID_SDK_ROOT/platform-tools:$PATH
```

检查：

```bash
java -version
ls "$ANDROID_SDK_ROOT/platforms"       # android-35
ls "$ANDROID_SDK_ROOT/build-tools"     # 35.0.0
```

## 3. 首次初始化

```bash
cd mobile
npm install

# 已安装的依赖
# @capacitor/core@7.6.10
# @capacitor/android@7.6.10
# @capacitor/cli@7.6.10 (devDependency)

npm run build
npx cap add android
npx cap sync android
```

`capacitor.config.ts`：

```ts
import type { CapacitorConfig } from '@capacitor/cli';

const config: CapacitorConfig = {
  appId: 'com.flyingbasshot.meetagent',
  appName: 'MeetAgent Mobile',
  webDir: 'dist',
  // 仅 Android 真机开发联调；Release 必须改回 https 并移除 cleartext。
  server: {
    androidScheme: 'http',
    cleartext: true,
  },
  plugins: {
    Keyboard: {
      resize: 'native',
      resizeOnFullScreen: true,
    },
  },
};

export default config;
```

`androidScheme: 'http'` 让 WebView 以 `http://localhost` 加载页面，避免默认 `https://localhost` 访问 `http://100.x:8787` 时被混合内容策略拦截；否则 UI 会把请求失败误报为 CORS。

`android/local.properties` 指向本机 SDK（不要提交）：

```properties
sdk.dir=/home/perch/Android/Sdk
```

`.gitignore` 已忽略：

```text
android/local.properties
android/.gradle/
android/build/
android/app/build/
android/**/build/
android/**/.gradle/
```

## 4. 构建 Debug APK

```bash
cd mobile
npm run build
npx cap sync android

export JAVA_HOME=/usr/lib/jvm/java-21-openjdk
export ANDROID_SDK_ROOT=/home/perch/Android/Sdk
export ANDROID_HOME=$ANDROID_SDK_ROOT
export PATH=$JAVA_HOME/bin:$ANDROID_SDK_ROOT/platform-tools:$PATH

cd android
./gradlew assembleDebug --no-daemon
```

产物：

```text
mobile/android/app/build/outputs/apk/debug/app-debug.apk
```

检查包名与版本：

```bash
$ANDROID_SDK_ROOT/build-tools/35.0.0/aapt dump badging \
  android/app/build/outputs/apk/debug/app-debug.apk | head
```

本机结果：

```text
package: name='com.flyingbasshot.meetagent' versionCode='1' versionName='1.0'
sdkVersion:'23'
targetSdkVersion:'35'
application-label:'MeetAgent Mobile'
APK: mobile/android/app/build/outputs/apk/debug/app-debug.apk
大小: 4,380,775 bytes（约 4.2 MB）
SHA-256: b8fbd8765add0dc7e970b8ae675ee7c4ed70de2f81b2e34eabb069d97af32d87
```

## 5. Gradle 网络问题

首次执行 wrapper 时，若 `services.gradle.org` 的 TLS 下载失败：

```text
javax.net.ssl.SSLHandshakeException: Remote host terminated the handshake
```

可先用 `curl` 下载 Gradle 8.11.1，并放入 wrapper 缓存目录：

```bash
curl -L -o ~/.cache/gradle-8.11.1-all.zip \
  https://services.gradle.org/distributions/gradle-8.11.1-all.zip

DIST=~/.gradle/wrapper/dists/gradle-8.11.1-all/2qik7nd48slq1ooc2496ixf4i
mkdir -p "$DIST"
unzip -q ~/.cache/gradle-8.11.1-all.zip -d "$DIST"
touch "$DIST/gradle-8.11.1-all.zip.ok"
```

随后重新执行 `./gradlew assembleDebug`。不要通过关闭证书校验来绕过下载问题。

## 6. 连接后端与明文流量

- 正式远程：使用 `https://<host>`，客户端自动派生 `wss://<host>/ws`。**Release 构建必须使用 HTTPS/WSS。**
- 当前 Debug 同时开启：
  - `capacitor.config.ts` → `server.androidScheme: "http"`、`server.cleartext: true`（APK 内 `assets/capacitor.config.json` 可验证）
  - `android/app/src/debug/AndroidManifest.xml` → `android:usesCleartextTraffic="true"` 与 debug network security config
  - Tailscale 网段 `100.64.0.0/10`，例如 `http://100.69.235.2:8787` / `ws://100.69.235.2:8787/ws`
  - Release 必须改回 `androidScheme: "https"`、删除 `cleartext`，并使用 HTTPS/WSS；不要携带 debug 配置发布。
- Android 的 network security config 不能直接表达 CIDR；debug 采用全局允许明文，正式包必须改回 HTTPS/WSS，不能把该配置带入 release。
- Token 当前仍存在 WebView localStorage；Capacitor 正式版应迁移到 Android Keystore（通过自定义插件或安全存储插件）。

## 7. 真机验证（2026-11-08）

设备与网络：

- `adb` 设备：`2NP0224717021762`（HBN-AL00，USB authorized）
- 手机 Tailscale：`hbn-al00` / `100.87.152.120`
- Node：`http://100.69.235.2:8787`（真实 OpenCode Agent）
- App：`com.flyingbasshot.meetagent`，Debug APK

结果：

- 配对成功（Token 自动重连），Home 展示真实 Node `developer-machine` 与 41 个 Session
- Sessions 列表与测试 Session `ses_e5a1c7989ffefcU1XdrTiq6WXX` 详情正常打开
- 在测试 Session 发送 Prompt，收到真实 Agent 回复，Session 状态 `running → idle`
- Interrupt：发送较慢任务后中断，状态回到 `idle`，UI 无卡死，时间线记录 `running → idle`
- 权限审批：读取 `.env` 触发真实 `permission.requested`，点击「允许一次」后返回 `OK-1794118574`；时间线仅一条「审批完成 · 决策 once」，pending 卡片消失
- 断网/恢复：关闭 Wi-Fi/数据并停止 Tailscale 后显示「网络离线：已暂停 Node 自动重连」；恢复网络与 Tailscale 后自动重连，Snapshot 恢复（序号 710）
- logcat：修复后未出现 Capacitor JS Console 异常、CORS 或 mixed-content 错误
- 未向主会话 `ses_e5e6e776fffeeeCdeah5WtPEhJ` 发送任何 Prompt
- 真机截图：`/tmp/opencode/meetagent-mobile-android/`
  - `06-home-real.png`
  - `07-sessions-real.png`
  - `08-target-session-real.png`
  - `24-running-before-interrupt.png`
  - `25-after-interrupt.png`
  - `30-permission-pending.png`
  - `32-permission-approved.png`
  - `41-offline-verified.png`
  - `42-recovered-verified.png`

真机修复：

1. 旧版 Android WebView（Chrome < 119）没有 `Promise.withResolvers`，点击连接时报：
   `E/Capacitor/Console ... TypeError: Promise.withResolvers is not a function`
   已在 `src/main.ts` 增加运行时 polyfill。
2. WebView 的 `navigator.onLine` 在 Tailscale VPN 残留时不可靠，断网后仍为 `true`。
   已接入 `@capacitor/network@7.0.4`，使用原生 `networkStatusChange` + 4 秒轮询刷新；离线时暂停重连，恢复后立即重连。

未在真机验证：

- Wi-Fi / 蜂窝 / Tailscale 网络之间的切换时延
- Release 签名、系统通知、Keystore、后台保活

P0 UX 真机检查（2026-11-08）：

- 真实 Node（Token 自动重连）检查概览 / 会话 / 详情 / 动态 / 设置 / 开发者信息
- 测试 Session 发送 Prompt 后，用户 Prompt 与 Agent 回复合并为聊天流
- 权限卡不再显示内部 permission_id；技术字段只在开发者信息/原始事件开关中出现
- 事件摘要已中文化：会话标题 + 中文状态、审批决策（允许一次/拒绝）、工具状态（运行中/已完成）
- 空会话 composer 贴近底部；有消息时保持 sticky，不与 TabBar 重叠
- 截图：`/tmp/opencode/meetagent-mobile-android/`
  - `80-p0-real-home.png`
  - `81-p0-real-sessions.png`
  - `82-p0-real-session-detail.png`
  - `83-p0-real-activity.png`
  - `84-p0-real-settings.png`
  - `85-p0-real-developer.png`
  - `86-p0-real-chat.png`
  - `89-p0-session-menu.png`
  - `95-final-empty-composer.png`
  - `96-final-activity-cn.png`

## 8. P1 原生交互真机验证（2026-11-08）

插件接入：

- `@capacitor/haptics`：Light（Prompt 成功、审批、Interrupt、下拉刷新成功）、Error（失败）
- `@capacitor/keyboard`：`KeyboardResize.Native`，键盘出现时隐藏 TabBar，滚动自动收起
- `@capacitor/app`：硬件返回键分层（Sheet → 详情/开发者 router.back → 根 Tab 退出）
- `@capacitor/status-bar`：深色背景 `#0b0d10` + 浅色图标；启动/导航栏同步

验证结果：

- `npm run typecheck`、`npm run build`、`npx cap sync android`、`./gradlew assembleDebug` 全部通过
- APK：`4,380,775 bytes` / SHA-256 `b8fbd8765add0dc7e970b8ae675ee7c4ed70de2f81b2e34eabb069d97af32d87`
- 下拉刷新：真实 Node 发送 `snapshot` envelope，Home/Sessions/Activity 可用；阈值 70px；刷新指示器显示
- 离线：关闭 Wi-Fi/数据 + 停止 Tailscale，显示「网络离线」；下拉不触发刷新
- 恢复：启用 Wi-Fi/数据并启动 Tailscale，自动重连并恢复 Snapshot
- ActionSheet：详情「⋯」、Interrupt 二次确认、筛选 BottomSheet；遮罩/动画/安全区正常
- 返回键：关闭 Sheet、详情返回会话列表、根 Tab 退出 App
- 键盘：输入条位于键盘上方，TabBar 隐藏；滚动页面后键盘收起
- Haptics：`adb shell dumpsys vibrator_manager` 记录 `com.flyingbasshot.meetagent` TOUCH 50ms 振动
- Tab 滚动位置：动态页切换前后截图一致；会话列表按 scrollY 恢复（实时重排导致内容偏移）
- 仅使用测试 Session `ses_e5a1c7989ffefcU1XdrTiq6WXX`；未向主会话发送 Prompt
- 真实 Node 联调期间 Tailscale 路由短暂中断；临时使用 `adb reverse` 完成 Snapshot 验证，随后恢复 `http://100.69.235.2:8787` 并移除 reverse

截图：`/tmp/opencode/meetagent-mobile-android/`

- `101-p1-home.png`
- `111-p1-action-sheet.png`
- `113-p1-keyboard.png`
- `116-p1-interrupt-confirm.png`
- `121-p1-live-pull.png`
- `122-p1-live-pull-done.png`
- `130-p1-filter-sheet.png`
- `131-p1-offline.png`
- `132-p1-offline-pull.png`
- `133-p1-offline-recovered.png`
- `134-p1-activity-scroll-before.png` / `135-p1-activity-scroll-after.png`

## 9. 签名与发布

- Debug APK 使用 Android debug keystore，仅用于内部测试，不能发布。
- Release 需要独立 keystore：

```bash
keytool -genkeypair -v \
  -keystore meetagent-release.jks \
  -alias meetagent \
  -keyalg RSA -keysize 2048 -validity 10000
```

- 在 `android/app/build.gradle` 配置 `signingConfigs.release`，密码通过环境变量或 `~/.gradle/gradle.properties` 注入，**不要提交 keystore 或密码**。
- 发布前检查：
  - `applicationId` 与签名证书匹配；
  - `versionCode` / `versionName` 递增；
  - 使用 WSS 与受信任证书；
  - 关闭 debug 的 cleartext 配置；
  - 在真机或 Firebase Test Lab 上跑安装与连接冒烟。

## 10. 本机未验证

- 已完成 Debug APK 安装、连接、Prompt、Interrupt、权限审批与 P1 原生交互真机验证。
- 未构建 Release APK，未配置发布签名。
- 未验证 Android Keystore、系统通知、FCM、后台保活。
- 未验证 Wi-Fi / 蜂窝 / Tailscale 之间的实际切换时延。
- 未在 iOS 上打包或运行。
