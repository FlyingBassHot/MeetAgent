# MeetAgent Mobile Android 构建指南（M5b）

> 目标：用 Capacitor 把 `mobile/dist` 打包成 Android Debug APK。
> 本机已验证：JDK 21、Android SDK 35、Gradle 8.11.1、Capacitor 7.6.10。
> 没有连接设备或模拟器，未执行安装与真机运行。

## 1. 版本与工具链

| 组件 | 版本 |
|---|---|
| Capacitor Core / CLI / Android | 7.6.10 |
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
};

export default config;
```

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
APK: mobile/android/app/build/outputs/apk/debug/app-debug.apk（约 4.0 MB）
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
- Debug 构建已允许明文 HTTP/WS，用于本机、LAN 与 Tailscale：
  - `android/app/src/debug/AndroidManifest.xml`：`android:usesCleartextTraffic="true"`，并引用 debug 专用 network security config
  - `android/app/src/debug/res/xml/network_security_config.xml`：`cleartextTrafficPermitted="true"`
  - Tailscale 网段为 `100.64.0.0/10`，例如 `http://100.69.235.2:8787` / `ws://100.69.235.2:8787/ws` 可在 debug 包访问
  - 这些文件只进入 debug variant；`release` 使用平台默认（禁止 cleartext）
- Android 的 network security config 不能直接表达 CIDR；debug 采用全局允许明文，正式包必须改回 HTTPS/WSS，不能把该配置带入 release。
- Token 当前仍存在 WebView localStorage；Capacitor 正式版应迁移到 Android Keystore（通过自定义插件或安全存储插件）。

## 7. 签名与发布

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

## 8. 本机未验证

- 无 adb 设备/模拟器：未执行 `adb install`、启动 Activity 或真机联调。
- 未构建 Release APK，未配置发布签名。
- 未验证 Android Keystore、系统通知、FCM、后台保活。
- 未验证 Tailscale Android 客户端实际网络切换。
