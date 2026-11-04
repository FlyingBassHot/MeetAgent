<script setup lang="ts">
import { computed } from 'vue'
import { useCommandsStore } from '../../stores/commands'
import { useEventsStore } from '../../stores/events'
import { useConnectionStore } from '../../stores/connection'
import { formatTime, shortId } from '../../lib/format'
import EmptyState from '../ui/EmptyState.vue'
import Skeleton from '../ui/Skeleton.vue'

const props = withDefaults(
  defineProps<{
    /** 只显示某个 Session 的权限（会话详情用）；缺省显示全部。 */
    sessionId?: string
    compact?: boolean
  }>(),
  { sessionId: '', compact: false },
)

const events = useEventsStore()
const commands = useCommandsStore()
const connection = useConnectionStore()

const items = computed(() =>
  props.sessionId
    ? events.pendingPermissions.filter((item) => item.session_id === props.sessionId)
    : events.pendingPermissions,
)

function decide(permissionId: string, decision: 'once' | 'always' | 'reject'): void {
  commands.respondPermission(permissionId, decision)
}
</script>

<template>
  <Skeleton v-if="connection.isConnecting && items.length === 0" :rows="2" :row-height="120" />
  <EmptyState
    v-else-if="items.length === 0"
    icon="✓"
    title="没有待审批请求"
    hint="permission.requested 事件会派生到这里"
    compact
  />
  <ul v-else class="permission-list">
    <li v-for="item in items" :key="item.permission_id">
      <div class="permission-top">
        <strong>{{ item.description || 'Permission request' }}</strong>
        <span class="permission-node">{{ item.node_id }}</span>
      </div>
      <div class="permission-meta">
        <code>{{ shortId(item.permission_id) }}</code>
        <span v-if="item.agent_id">{{ item.agent_id }}</span>
        <span v-if="item.session_id">{{ shortId(item.session_id) }}</span>
        <span v-if="item.expires_at">expires {{ formatTime(item.expires_at) }}</span>
      </div>
      <div class="permission-actions">
        <button
          type="button"
          class="button tiny primary"
          data-testid="permission-approve"
          @click="decide(item.permission_id, 'once')"
        >
          允许一次
        </button>
        <button
          type="button"
          class="button tiny ghost"
          data-testid="permission-always"
          @click="decide(item.permission_id, 'always')"
        >
          总是允许
        </button>
        <button
          type="button"
          class="button tiny danger"
          data-testid="permission-reject"
          @click="decide(item.permission_id, 'reject')"
        >
          拒绝
        </button>
      </div>
    </li>
  </ul>
</template>
