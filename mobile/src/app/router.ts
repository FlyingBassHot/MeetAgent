import { createRouter, createWebHashHistory } from 'vue-router';
import { useConnectionStore } from '@/stores/connection';
import AppShell from '@/components/AppShell.vue';
import HomeView from '@/views/HomeView.vue';
import SessionsView from '@/views/SessionsView.vue';
import SessionDetailView from '@/views/SessionDetailView.vue';
import ActivityView from '@/views/ActivityView.vue';
import SettingsView from '@/views/SettingsView.vue';
import DeveloperView from '@/views/DeveloperView.vue';
import ConnectView from '@/views/ConnectView.vue';

const router = createRouter({
  history: createWebHashHistory(),
  scrollBehavior: () => ({ top: 0 }),
  routes: [
    {
      path: '/connect',
      name: 'connect',
      component: ConnectView,
      meta: { requiresAuth: false, title: '连接' },
    },
    {
      path: '/',
      component: AppShell,
      children: [
        { path: '', redirect: '/home' },
        { path: 'home', name: 'home', component: HomeView, meta: { title: '概览' } },
        {
          path: 'sessions',
          name: 'sessions',
          component: SessionsView,
          meta: { title: '会话' },
        },
        {
          path: 'sessions/:sessionId',
          name: 'session-detail',
          component: SessionDetailView,
          meta: { title: '会话详情' },
        },
        {
          path: 'activity',
          name: 'activity',
          component: ActivityView,
          meta: { title: '动态' },
        },
        {
          path: 'settings',
          name: 'settings',
          component: SettingsView,
          meta: { title: '设置' },
        },
        {
          path: 'settings/developer',
          name: 'settings-developer',
          component: DeveloperView,
          meta: { title: '开发者信息' },
        },
      ],
    },
    { path: '/:pathMatch(.*)*', redirect: '/home' },
  ],
});

router.beforeEach((to) => {
  const connection = useConnectionStore();
  const requiresAuth = to.meta.requiresAuth !== false;
  if (requiresAuth && !connection.isOnline) {
    return { name: 'connect', query: { next: to.fullPath } };
  }
  if (to.name === 'connect' && connection.isOnline) {
    const next = typeof to.query.next === 'string' ? to.query.next : '/home';
    return next.startsWith('/') ? next : '/home';
  }
  return true;
});

router.afterEach((to) => {
  const title = typeof to.meta.title === 'string' ? to.meta.title : 'MeetAgent';
  document.title = `${title} · MeetAgent Mobile`;
});

export default router;
