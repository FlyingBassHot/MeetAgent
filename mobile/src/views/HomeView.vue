<script setup lang="ts">
import { computed } from 'vue';
import type { PermissionDecision } from '@/domain/protocol';
import { isImportantEvent } from '@/domain/activity';
import { useCommandsStore } from '@/stores/commands';
import { useConnectionStore } from '@/stores/connection';
import { useNodeStore } from '@/stores/node';
import PageHeader from '@/components/PageHeader.vue';
import SessionCard from '@/components/SessionCard.vue';
import PermissionCard from '@/components/PermissionCard.vue';
import EventRow from '@/components/EventRow.vue';
import PageSkeleton from '@/components/PageSkeleton.vue';

const connection = useConnectionStore();
const nodeStore = useNodeStore();
const commands = useCommandsStore();

const onlineAgents = computed(
  () => nodeStore.agents.filter((agent) => agent.status !== 'offline').length,
);
const activeSessions = computed(() =>
  [...nodeStore.runningSessions, ...nodeStore.waitingSessions].slice(0, 4),
);
const pendingViews = computed(() =>
  nodeStore.pendingPermissions.map((permission) => ({
    permission,
    agentName: permission.agent_id ? nodeStore.agentById[permission.agent_id]?.name : undefined,
    sessionTitle: permission.session_id
      ? nodeStore.sessionById[permission.session_id]?.title
      : undefined,
  })),
);
const recentEvents = computed(() =>
  nodeStore.recentEvents.filter(isImportantEvent).slice(0, 5),
);
const loading = computed(() => !nodeStore.hasSnapshot && connection.isBusy);

function respond(permissionId: string, decision: PermissionDecision): void {
  void commands.respondPermission(permissionId, decision);
}
</script>

<template>
  <div class="home">
    <PageHeader title="概览" />

    <PageSkeleton v-if="loading" :rows="5" />
    <template v-else>
      <div v-if="connection.mode === 'mock'" class="banner">
      演示模式：以下 Agent、消息与审批均为本地 Mock 数据。
    </div>
    <div v-if="nodeStore.sequenceGap" class="banner banner--danger">
      事件序号不连续，请在设置中重新连接以刷新。
    </div>

    <p class="home__status">
      <strong>{{ onlineAgents }}</strong> 个 Agent ·
      <strong>{{ nodeStore.runningSessions.length }}</strong> 个运行中 ·
      <strong :class="{ 'home__alert': nodeStore.pendingPermissions.length > 0 }">
        {{ nodeStore.pendingPermissions.length }}
      </strong>
      个待审批
    </p>

    <section v-if="pendingViews.length > 0" class="section">
      <div class="section__head">
        <h2>待处理</h2>
        <RouterLink to="/activity">全部动态</RouterLink>
      </div>
      <div class="stack">
        <PermissionCard
          v-for="item in pendingViews"
          :key="item.permission.permission_id"
          :permission="item.permission"
          :agent-name="item.agentName"
          :session-title="item.sessionTitle"
          :busy="commands.isBusy(`permission:${item.permission.permission_id}`)"
          @respond="respond"
        />
      </div>
    </section>

    <section class="section">
      <div class="section__head">
        <h2>活跃会话</h2>
        <RouterLink to="/sessions">查看全部</RouterLink>
      </div>
      <div v-if="activeSessions.length === 0" class="home__empty">当前没有运行中的会话</div>
      <div v-else class="list">
        <SessionCard
          v-for="session in activeSessions"
          :key="session.session_id"
          :session="session"
          :agent-name="nodeStore.agentById[session.agent_id]?.name"
          :pending-count="nodeStore.pendingFor(session.session_id).length"
        />
      </div>
    </section>

    <section class="section">
      <div class="section__head">
        <h2>最近动态</h2>
        <RouterLink to="/activity">查看全部</RouterLink>
      </div>
      <div v-if="recentEvents.length === 0" class="home__empty">暂无动态</div>
      <div v-else class="list">
        <EventRow
          v-for="event in recentEvents"
          :key="event.event_id"
          :event="event"
          :session-title="event.session_id ? nodeStore.sessionById[event.session_id]?.title : ''"
          :agent-name="event.agent_id ? nodeStore.agentById[event.agent_id]?.name : ''"
        />
      </div>
    </section>
    </template>
  </div>
</template>

<style scoped src="./home.css"></style>
