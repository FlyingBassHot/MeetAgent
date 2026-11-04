import { createApp } from 'vue'
import { createPinia } from 'pinia'
import App from './App.vue'
import router from './router'
import { useThemeStore } from './stores/theme'
import { useConnectionStore } from './stores/connection'
import './styles/index.css'

const app = createApp(App)
const pinia = createPinia()

app.use(pinia)

// 在挂载前应用主题，避免首屏闪烁
useThemeStore(pinia).init()
// 尽早注册 CloudClient 回调（连接、注册、心跳、快照、命令）
useConnectionStore(pinia)

app.use(router)
app.mount('#app')
