<script setup lang="ts">
import { computed } from 'vue'
import BaseCard from '../components/ui/BaseCard.vue'
import EventStream from '../components/events/EventStream.vue'
import { useEventsStore } from '../stores/events'

const events = useEventsStore()

const visibleCount = computed(() => Math.min(events.recentEvents.length, 100))
</script>

<template>
  <div class="page-stack">
    <header class="view-header">
      <h2>事件</h2>
      <p>每 Node 独立 sequence 的事件流，支持断线补发与 payload 展开。</p>
    </header>

    <BaseCard
      title="Event stream"
      :description="`最近 ${visibleCount} / ${events.events.length} 条 · 每 Node 独立 sequence`"
    >
      <template #actions>
        <span class="chip">{{ events.events.length }}</span>
      </template>
      <EventStream />
    </BaseCard>
  </div>
</template>
