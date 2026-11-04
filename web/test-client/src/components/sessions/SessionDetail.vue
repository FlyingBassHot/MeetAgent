<script setup lang="ts">
import { computed } from 'vue'
import { SESSION_TABS, useWorkbenchStore } from '../../stores/workbench'
import { useEventsStore } from '../../stores/events'
import type { SessionEntry } from '../../stores/inventory'
import { formatRelative, idSuffix } from '../../lib/format'
import { sessionStatusLabel, sessionStatusTone } from '../../lib/status'
import PromptComposer from './PromptComposer.vue'
import SessionLiveEvents from './SessionLiveEvents.vue'
import SessionMeta from './SessionMeta.vue'
import NodeToolsMenu from './NodeToolsMenu.vue'
import PermissionList from '../permissions/PermissionList.vue'

const props = defineProps<{
  session: SessionEntry
}>()

const emit = defineEmits<{
  close: []
  openEvents: []
}>()

const workbench = useWorkbenchStore()
const events = useEventsStore()

const permissions = computed(() => events.sessionPermissions(props.session.session_id))
</script>

<template>
  <section class="session-detail" data-testid="session-detail" aria-label="Session 详情">
    <header class="detail-header">
      <button
        type="button"
        class="icon-button detail-back"
        data-testid="session-back"
        aria-label="返回 Session 列表"
        @click="emit('close')"
      >
        ←
      </button>

      <div class="detail-title">
        <span class="session-status-dot" :class="sessionStatusTone(session.status)" aria-hidden="true"></span>
        <div>
          <h2 :title="session.title || session.session_id">
            {{ session.title?.trim() || idSuffix(session.session_id) }}
          </h2>
          <p>
            <span class="detail-status">{{ sessionStatusLabel(session.status) }}</span>
            · {{ formatRelative(session.updated_at) }}
            · <code :title="session.session_id">{{ idSuffix(session.session_id) }}</code>
            · {{ session.agentName }}
          </p>
        </div>
      </div>

      <div class="detail-header-actions">
        <NodeToolsMenu />
      </div>
    </header>

    <div class="detail-tabs" role="tablist" aria-label="Session 详情视图">
      <button
        v-for="tab in SESSION_TABS"
        :key="tab.value"
        type="button"
        role="tab"
        class="detail-tab"
        :class="{ active: workbench.tab === tab.value }"
        :aria-selected="workbench.tab === tab.value"
        :data-testid="`session-tab-${tab.value}`"
        @click="workbench.setTab(tab.value)"
      >
        {{ tab.label }}
      </button>
    </div>

    <div class="detail-body">
      <SessionLiveEvents
        v-if="workbench.tab === 'events'"
        :session-id="session.session_id"
        @open-events="emit('openEvents')"
      />
      <SessionMeta v-else :session="session" />
    </div>

    <footer class="detail-footer">
      <div v-if="permissions.length" class="detail-permissions" data-testid="session-permissions">
        <div class="detail-section-heading">
          <strong>当前 Session 的权限请求</strong>
          <span class="chip">{{ permissions.length }}</span>
        </div>
        <PermissionList :session-id="session.session_id" />
      </div>

      <PromptComposer :session-id="session.session_id" />
    </footer>
  </section>
</template>
