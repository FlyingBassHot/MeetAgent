import type { CapacitorConfig } from '@capacitor/cli';

const config: CapacitorConfig = {
  appId: 'com.flyingbasshot.meetagent',
  appName: 'MeetAgent Mobile',
  webDir: 'dist',
  // 仅 Android 真机开发联调：WebView 以 http://localhost 加载，
  // 允许访问 LAN / Tailscale（100.64.0.0/10）的 HTTP 与 WS，避免混合内容。
  // Release 必须改为 https（或删除 server.androidScheme/cleartext），并使用 HTTPS/WSS。
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
