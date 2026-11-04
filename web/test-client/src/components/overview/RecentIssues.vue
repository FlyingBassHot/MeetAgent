<script setup lang="ts">
import { useEventsStore } from '../../stores/events'
import { formatRelative } from '../../lib/format'
import EmptyState from '../ui/EmptyState.vue'

const events = useEventsStore()
</script>

<template>
  <EmptyState
    v-if="events.recentIssues.length === 0"
    icon="✓"
    title="最近没有异常"
    hint="Agent error、命令失败 / 超时与通信错误会汇总在这里"
    compact
  />
  <ul v-else class="issue-list">
    <li v-for="issue in events.recentIssues" :key="issue.key" class="issue-item" data-testid="issue-item">
      <span class="issue-kind" :class="issue.kind">{{ issue.kind }}</span>
      <div class="issue-body">
        <strong>{{ issue.title }}</strong>
        <small v-if="issue.detail" :title="issue.detail">{{ issue.detail }}</small>
      </div>
      <time>{{ formatRelative(new Date(issue.at).toISOString()) }}</time>
    </li>
  </ul>
</template>
