<script setup lang="ts">
import { computed } from 'vue';
import type { PermissionDecision } from '@/domain/protocol';
import { formatRelative, formatRtt, shortId } from '@/domain/format';
import { agentStatusLabel, agentStatusTone } from '@/domain/labels';
import { latestSummaryBySession } from '@/domain/session';
import { useCommandsStore } from '@/stores/commands';
import { useConnectionStore } from '@/stores/connection';
import { useNodeStore } from '@/stores/node';
import PageHeader from '@/components/PageHeader.vue';
import StatTile from '@/components/StatTile.vue';
import SessionCard from '@/components/SessionCard.vue';
import PermissionCard from '@/components/PermissionCard.vue';
import EventRow from '@/components/EventRow.vue';
import StatusPill from '@/components/StatusPill.vue';

const connection = useConnectionStore();
const nodeStore = useNodeStore();
const commands = useCommandsStore();

const isMock = computed(() => connection.mode === 'mock');
const summaries = computed(() => latestSummaryBySession(nodeStore.events));
const nodeName = computed(() => nodeStore.node?.name ?? '未连接 Node');
const onlineAgents = computed(
  () => nodeStore.agents.filter((agent) => agent.status !== 'offline').length,
);
const activeSessions = computed(() =>
  nodeStore.sessions
    .filter((session) => session.status === 'running' || session.status === 'waiting')
    .slice(0, 4),
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
const recentEvents = computed(() => nodeStore.recentEvents.slice(0, 5));
const lastSync = computed(() => formatRelative(connection.serverTime ?? undefined));

function respond(permissionId: string, decision: PermissionDecision): void {
  void commands.respondPermission(permissionId, decision);
}
</script>

<template>
  <div class="stack--lg">
    <PageHeader
      title="Home"
      :subtitle="`${nodeName} · ${onlineAgents} 个 Agent 在线`"
    >
      <template #actions>
        <span v-if="isMock" class="pill pill--warning">MOCK</span>
      </template>
    </PageHeader>

    <div v-if="isMock" class="banner">
      <span>当前为本地 Mock 数据，所有 Agent 状态、事件与审批均由脚本生成。</span>
    </div>
    <div v-if="nodeStore.sequenceGap" class="banner banner--danger">
      {{ '检测到事件序号不连续，建议在 Settings 断开后重新连接以刷新 Snapshot。' }}
    </div>

    <div class="stat-grid">
      <StatTile label="Agents" :value="onlineAgents" :hint="`共 ${nodeStore.agents.length} 个`" />
      <StatTile
        label="运行中"
        :value="nodeStore.runningSessions.length"
        :hint="`等待 ${nodeStore.waitingSessions.length} 个`"
      />
      <StatTile
        label="待审批"
        :value="nodeStore.pendingPermissions.length"
        hint="需要人工决策"
      />
      <StatTile label="事件缓存" :value="nodeStore.events.length" :hint="`序号 ${nodeStore.lastSequence}`" />
    </div>

    <section class="section">
      <div class="section__head">
        <h2>待审批与异常</h2>
        <RouterLink to="/activity">全部 Activity</RouterLink>
      </div>
      <div v-if="pendingViews.length === 0" class="card card--pad empty-inline small muted">
        当前没有待审批请求。
      </div>
      <div v-else class="stack">
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
        <h2>活跃 Session</h2>
        <RouterLink to="/sessions">查看全部</RouterLink>
      </div>
      <div v-if="activeSessions.length === 0" class="card card--pad empty-inline small muted">
        没有运行中或等待中的 Session。
      </div>
      <div v-else class="session-grid">
        <SessionCard
          v-for="session in activeSessions"
          :key="session.session_id"
          :session="session"
          :agent-name="nodeStore.agentById[session.agent_id]?.name"
          :pending-count="nodeStore.pendingFor(session.session_id).length"
          :latest-event="summaries[session.session_id]"
        />
      </div>
    </section>

    <section class="section">
      <div class="section__head">
        <h2>Agent 状态</h2>
        <span class="small muted">{{ nodeStore.adapters.length }} 个 Adapter</span>
      </div>
      <div class="agent-grid">
        <article v-for="agent in nodeStore.agents" :key="agent.agent_id" class="card card--pad agent">
          <div class="row row--between">
            <div class="agent__name">
              <strong>{{ agent.name }}</strong>
              <span class="small muted mono">{{ agent.type }}</span>
            </div>
            <StatusPill
              :label="agentStatusLabel(agent.status)"
              :tone="agentStatusTone(agent.status)"
            />
          </div>
          <div class="agent__meta small muted">
            <span>{{ agent.sessions.length }} 个 Session</span>
            <span v-if="agent.capabilities?.send_prompt">· 可 Prompt</span>
            <span v-if="agent.capabilities?.interrupt">· 可 Interrupt</span>
          </div>
        </article>
      </div>
    </section>

    <section class="section">
      <div class="section__head">
        <h2>最近事件</h2>
        <RouterLink to="/activity">事件流</RouterLink>
      </div>
      <div v-if="recentEvents.length === 0" class="card card--pad empty-inline small muted">
        还没有收到事件。
      </div>
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

    <section class="card card--pad node-foot">
      <div>
        <div class="stat__label">Node</div>
        <div class="row row--wrap small">
          <span class="mono">{{ shortId(nodeStore.node?.node_id, 10) }}</span>
          <span class="muted">{{ nodeStore.node?.platform || '—' }}</span>
          <span class="muted">v{{ nodeStore.node?.version || '—' }}</span>
        </div>
      </div>
      <div class="node-foot__right small muted">
        <span>RTT {{ formatRtt(connection.rttMs) }}</span>
        <span>同步 {{ lastSync }}</span>
      </div>
    </section>
  </div>
</template>

<style scoped src="./home.css"></style>
