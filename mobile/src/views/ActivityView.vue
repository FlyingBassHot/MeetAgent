<script setup lang="ts">
import { computed, ref } from 'vue';
import type { PermissionDecision } from '@/domain/protocol';
import { eventGroup, type EventGroup } from '@/domain/labels';
import { useCommandsStore } from '@/stores/commands';
import { useConnectionStore } from '@/stores/connection';
import { useNodeStore } from '@/stores/node';
import PageHeader from '@/components/PageHeader.vue';
import PermissionCard from '@/components/PermissionCard.vue';
import EventRow from '@/components/EventRow.vue';
import EmptyState from '@/components/EmptyState.vue';

type ActivityFilter = 'all' | EventGroup;

const nodeStore = useNodeStore();
const commands = useCommandsStore();
const connection = useConnectionStore();
const filter = ref<ActivityFilter>('all');

const pendingViews = computed(() =>
  nodeStore.pendingPermissions.map((permission) => ({
    permission,
    agentName: permission.agent_id ? nodeStore.agentById[permission.agent_id]?.name : undefined,
    sessionTitle: permission.session_id
      ? nodeStore.sessionById[permission.session_id]?.title
      : undefined,
  })),
);

const counts = computed<Record<ActivityFilter, number>>(() => {
  const total: Record<ActivityFilter, number> = {
    all: nodeStore.events.length,
    message: 0,
    tool: 0,
    permission: 0,
    session: 0,
    agent: 0,
    other: 0,
  };
  for (const event of nodeStore.events) {
    total[eventGroup(event.type)] += 1;
  }
  return total;
});

const visibleEvents = computed(() =>
  nodeStore.recentEvents.filter(
    (event) => filter.value === 'all' || eventGroup(event.type) === filter.value,
  ),
);

const filters: Array<{ key: ActivityFilter; label: string }> = [
  { key: 'all', label: '全部' },
  { key: 'message', label: '消息' },
  { key: 'tool', label: '工具' },
  { key: 'permission', label: '审批' },
  { key: 'session', label: 'Session' },
  { key: 'agent', label: 'Agent' },
];

function respond(permissionId: string, decision: PermissionDecision): void {
  void commands.respondPermission(permissionId, decision);
}
</script>

<template>
  <div class="stack">
    <PageHeader
      title="Activity"
      :subtitle="`全局事件流 · 序号 ${nodeStore.lastSequence} · ${nodeStore.events.length} 条缓存`"
    >
      <template #actions>
        <span v-if="connection.mode === 'mock'" class="pill pill--warning">MOCK</span>
      </template>
    </PageHeader>

    <div v-if="nodeStore.sequenceGap" class="banner banner--danger">
      事件序号出现缺口，本地状态可能不完整。请断开后重新连接，Node 会下发 Snapshot 并补发事件。
    </div>

    <section v-if="pendingViews.length > 0" class="section">
      <div class="section__head">
        <h2>待审批</h2>
        <span class="small muted">{{ pendingViews.length }} 项</span>
      </div>
      <PermissionCard
        v-for="item in pendingViews"
        :key="item.permission.permission_id"
        :permission="item.permission"
        :agent-name="item.agentName"
        :session-title="item.sessionTitle"
        :busy="commands.isBusy(`permission:${item.permission.permission_id}`)"
        @respond="respond"
      />
    </section>

    <div class="chip-row chip-row--wrap">
      <button
        v-for="item in filters"
        :key="item.key"
        type="button"
        class="chip"
        :class="{ 'chip--active': filter === item.key }"
        @click="filter = item.key"
      >
        {{ item.label }} <span class="activity__count">{{ counts[item.key] }}</span>
      </button>
    </div>

    <div v-if="visibleEvents.length === 0" class="card card--pad">
      <EmptyState title="没有匹配的事件" description="切换筛选条件，或等待 Node 推送新事件。" />
    </div>
    <div v-else class="list">
      <EventRow
        v-for="event in visibleEvents"
        :key="event.event_id"
        :event="event"
        :session-title="event.session_id ? nodeStore.sessionById[event.session_id]?.title : ''"
        :agent-name="event.agent_id ? nodeStore.agentById[event.agent_id]?.name : ''"
      />
    </div>
  </div>
</template>

<style scoped src="./activity.css"></style>
