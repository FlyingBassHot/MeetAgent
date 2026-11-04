<script setup lang="ts">
import { computed } from 'vue'
import type { SessionEntry } from '../../stores/inventory'
import { formatTime, idSuffix, shortId } from '../../lib/format'
import { useInventoryStore } from '../../stores/inventory'

const props = defineProps<{
  session: SessionEntry
}>()

const inventory = useInventoryStore()

const capabilities = computed(() =>
  Object.entries(props.session.capabilities)
    .filter(([, enabled]) => enabled)
    .map(([name]) => name),
)

const node = computed(() => inventory.nodes.find((item) => item.node_id === props.session.node_id) ?? null)
</script>

<template>
  <div class="session-meta" data-testid="session-detail-meta">
    <dl class="detail-grid">
      <div>
        <dt>Session ID</dt>
        <dd><code :title="session.session_id">{{ session.session_id }}</code></dd>
      </div>
      <div>
        <dt>状态</dt>
        <dd>{{ session.status }}</dd>
      </div>
      <div>
        <dt>Agent</dt>
        <dd>{{ session.agentName }} <code>{{ session.agentType }}</code></dd>
      </div>
      <div>
        <dt>Node</dt>
        <dd>{{ node?.name || session.node_id }} <code>{{ session.node_id }}</code></dd>
      </div>
      <div>
        <dt>工作目录</dt>
        <dd><code>{{ session.working_directory || '—' }}</code></dd>
      </div>
      <div>
        <dt>更新时间</dt>
        <dd>{{ formatTime(session.updated_at) }}</dd>
      </div>
      <div>
        <dt>短 ID</dt>
        <dd><code>{{ shortId(session.session_id) }}</code></dd>
      </div>
      <div>
        <dt>Agent 状态</dt>
        <dd>{{ session.agentStatus }}</dd>
      </div>
      <div>
        <dt>Node 版本</dt>
        <dd>{{ node?.version ? `v${node.version}` : '—' }}</dd>
      </div>
      <div>
        <dt>Node 平台</dt>
        <dd>{{ node?.platform || '—' }}</dd>
      </div>
      <div>
        <dt>宿主</dt>
        <dd>{{ node?.hostname || '—' }}</dd>
      </div>
      <div>
        <dt>最后心跳</dt>
        <dd>{{ formatTime(node?.last_seen) }}</dd>
      </div>
    </dl>

    <div class="detail-capabilities">
      <span class="eyebrow">Agent 能力</span>
      <div v-if="capabilities.length" class="tag-row">
        <span v-for="capability in capabilities" :key="capability" class="tag">{{ capability }}</span>
      </div>
      <p v-else class="muted">未上报可用能力</p>
    </div>

    <p class="detail-footnote">
      Session 短 ID：{{ idSuffix(session.session_id) }} · 事件按 node_id:sequence 去重
    </p>
  </div>
</template>
