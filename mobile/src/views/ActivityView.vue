<script setup lang="ts">
import { computed, ref } from 'vue';
import type { PermissionDecision } from '@/domain/protocol';
import { groupEventsByDay, isImportantEvent } from '@/domain/activity';
import { useCommandsStore } from '@/stores/commands';
import { useNodeStore } from '@/stores/node';
import { usePreferencesStore } from '@/stores/preferences';
import PageHeader from '@/components/PageHeader.vue';
import PermissionCard from '@/components/PermissionCard.vue';
import EventRow from '@/components/EventRow.vue';
import EmptyState from '@/components/EmptyState.vue';
import BottomSheet from '@/components/BottomSheet.vue';

type ActivityFilter = 'important' | 'approvals' | 'sessions' | 'agents' | 'tools' | 'all';

const nodeStore = useNodeStore();
const commands = useCommandsStore();
const preferences = usePreferencesStore();
const filter = ref<ActivityFilter>('important');
const filterOpen = ref(false);

const pendingViews = computed(() =>
  nodeStore.pendingPermissions.map((permission) => ({
    permission,
    agentName: permission.agent_id ? nodeStore.agentById[permission.agent_id]?.name : undefined,
    sessionTitle: permission.session_id
      ? nodeStore.sessionById[permission.session_id]?.title
      : undefined,
  })),
);

const filters = computed<Array<{ key: ActivityFilter; label: string }>>(() => {
  const base: Array<{ key: ActivityFilter; label: string }> = [
    { key: 'important', label: '重要事件' },
    { key: 'approvals', label: '审批' },
    { key: 'sessions', label: '会话' },
    { key: 'agents', label: 'Agent' },
    { key: 'tools', label: '工具失败' },
  ];
  if (preferences.settings.showRawEvents) base.push({ key: 'all', label: '全部事件' });
  return base;
});

const visibleEvents = computed(() => {
  const source = preferences.settings.showRawEvents
    ? nodeStore.recentEvents
    : nodeStore.recentEvents.filter(isImportantEvent);
  return source.filter((event) => {
    const important = isImportantEvent(event);
    switch (filter.value) {
      case 'all':
        return true;
      case 'approvals':
        return event.type.startsWith('permission.');
      case 'sessions':
        return event.type.startsWith('session.');
      case 'agents':
        return event.type.startsWith('agent.');
      case 'tools':
        return event.type === 'tool.failed';
      default:
        return important;
    }
  });
});

const groups = computed(() => groupEventsByDay(visibleEvents.value));
const activeFilterLabel = computed(
  () => filters.value.find((item) => item.key === filter.value)?.label ?? '重要事件',
);

function selectFilter(next: ActivityFilter): void {
  filter.value = next;
  filterOpen.value = false;
}

function respond(permissionId: string, decision: PermissionDecision): void {
  void commands.respondPermission(permissionId, decision);
}
</script>

<template>
  <div class="activity">
    <PageHeader title="动态" subtitle="会话、审批与 Agent 的关键变化">
      <template #actions>
        <button
          type="button"
          class="activity__filter"
          :class="{ 'activity__filter--active': filter !== 'important' }"
          @click="filterOpen = true"
        >
          筛选
        </button>
      </template>
    </PageHeader>

    <div v-if="nodeStore.sequenceGap" class="banner banner--danger">
      事件序号不连续，请在设置中重新连接以刷新。
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

    <div class="activity__current">
      <span>{{ activeFilterLabel }}</span>
      <span class="muted">{{ visibleEvents.length }} 条</span>
    </div>

    <div v-if="groups.length === 0" class="card card--pad">
      <EmptyState title="暂无动态" description="新的会话、审批与 Agent 事件会出现在这里。" />
    </div>
    <template v-else>
      <section v-for="group in groups" :key="group.key" class="activity__group">
        <h2 class="activity__day">{{ group.label }}</h2>
        <div class="list">
          <EventRow
            v-for="event in group.events"
            :key="event.event_id"
            :event="event"
            :session-title="event.session_id ? nodeStore.sessionById[event.session_id]?.title : ''"
            :agent-name="event.agent_id ? nodeStore.agentById[event.agent_id]?.name : ''"
            :show-technical="preferences.settings.showRawEvents"
          />
        </div>
      </section>
    </template>

    <BottomSheet :open="filterOpen" title="筛选动态" @close="filterOpen = false">
      <div class="activity__options">
        <button
          v-for="item in filters"
          :key="item.key"
          type="button"
          class="activity__option"
          :class="{ 'activity__option--active': filter === item.key }"
          @click="selectFilter(item.key)"
        >
          {{ item.label }}
        </button>
      </div>
    </BottomSheet>
  </div>
</template>

<style scoped src="./activity.css"></style>
