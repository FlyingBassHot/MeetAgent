<script setup lang="ts">
import BaseCard from '../components/ui/BaseCard.vue'
import ActivityLog from '../components/activity/ActivityLog.vue'
import { useEventsStore } from '../stores/events'

const events = useEventsStore()
</script>

<template>
  <div class="page-stack">
    <header class="view-header">
      <h2>活动</h2>
      <p>通信与状态记录：注册、心跳、命令、响应与快照，最多保留 300 条。</p>
    </header>

    <BaseCard title="Activity" :description="`最近 ${events.logs.length} 条通信与状态记录`">
      <template #actions>
        <div class="log-tools">
          <label class="toggle">
            <input v-model="events.showHeartbeats" type="checkbox" />
            <span>心跳日志</span>
          </label>
          <button
            type="button"
            class="button tiny ghost"
            :disabled="events.logs.length === 0"
            @click="events.clearLogs"
          >
            清空
          </button>
        </div>
      </template>
      <ActivityLog />
    </BaseCard>
  </div>
</template>
