<script setup lang="ts">
import { useEventsStore } from '../../stores/events'
import { useEventViewStore, ACTIVITY_DIRECTIONS } from '../../stores/eventView'
import { useConnectionStore } from '../../stores/connection'
import ActivityLog from '../activity/ActivityLog.vue'
import Skeleton from '../ui/Skeleton.vue'

const events = useEventsStore()
const view = useEventViewStore()
const connection = useConnectionStore()
</script>

<template>
  <div class="activity-tab">
    <div class="filter-row activity-toolbar">
      <label class="toggle">
        <input v-model="events.showHeartbeats" type="checkbox" />
        <span>心跳日志</span>
      </label>

      <div class="filter-chips" role="group" aria-label="通信方向过滤" data-testid="activity-filter">
        <button
          v-for="option in ACTIVITY_DIRECTIONS"
          :key="option.value"
          type="button"
          class="filter-chip"
          :class="{ active: view.activityDirection === option.value }"
          :aria-pressed="view.activityDirection === option.value"
          @click="view.activityDirection = option.value"
        >
          {{ option.label }}
        </button>
      </div>

      <input
        v-model="view.activitySearch"
        class="filter-search"
        data-testid="activity-search"
        type="search"
        placeholder="搜索类型 / 标题 / 明细"
        aria-label="搜索通信日志"
        spellcheck="false"
      />

      <button
        type="button"
        class="button tiny ghost"
        :disabled="events.logs.length === 0"
        @click="events.clearLogs"
      >
        清空
      </button>
      <span class="muted filter-count">{{ view.filteredLogs.length }} / {{ events.logs.length }} 条</span>
    </div>

    <Skeleton v-if="connection.isConnecting && events.logs.length === 0" :rows="5" :row-height="60" />
    <ActivityLog v-else :entries="view.filteredLogs" />
  </div>
</template>
