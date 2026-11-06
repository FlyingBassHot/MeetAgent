import { createApp } from 'vue';
import { createPinia } from 'pinia';
import App from './App.vue';
import router from './app/router';
import './styles/base.css';

// Android WebView（Chrome < 119）没有 Promise.withResolvers。
// WsClient / MockClient / tabGuard 在连接路径中使用它，必须在挂载前补齐。
if (typeof Promise.withResolvers !== 'function') {
  Promise.withResolvers = function <T>(): PromiseWithResolvers<T> {
    let resolve: (value: T | PromiseLike<T>) => void = () => undefined;
    let reject: (reason?: unknown) => void = () => undefined;
    const promise = new Promise<T>((res, rej) => {
      resolve = res;
      reject = rej;
    });
    return { promise, resolve, reject };
  };
}

const pinia = createPinia();
createApp(App).use(pinia).use(router).mount('#app');
