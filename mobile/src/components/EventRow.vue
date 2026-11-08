<script setup lang="ts">
import { computed, ref } from 'vue';
import type { ProtocolEvent } from '@/domain/protocol';
import { formatClock, formatRelative } from '@/domain/format';
import { eventSummary, stringifyValue } from '@/domain/payload';
import { EVENT_LABELS, eventTone } from '@/domain/labels';

const props = withDefaults(
  defineProps<{
    event: ProtocolEvent;
    compact?: boolean;
    sessionTitle?: string;
    agentName?: string;
    showTechnical?: boolean;
  }>(),
  { compact: false, sessionTitle: '', agentName: '', showTechnical: false },
);

const open = ref(false);
const label = computed(() => EVENT_LABELS[props.event.type] ?? props.event.type);
const summary = computed(() =>
  eventSummary(props.event, {
    sessionTitle: props.sessionTitle,
    agentName: props.agentName,
  }),
);
const details = computed(() =>
  Object.entries(props.event.payload ?? {}).map(([key, value]) => ({
    key,
    value: stringifyValue(value),
  })),
);

function toggle(): void {
  if (!props.showTechnical) return;
  open.value = !open.value;
}
</script>

<template>
  <article
    class="event"
    :class="{ 'event--compact': compact, 'event--open': open, 'event--plain': !showTechnical }"
  >
    <div
      class="event__head"
      :role="showTechnical ? 'button' : undefined"
      :tabindex="showTechnical ? 0 : undefined"
      @click="toggle"
      @keydown.enter="toggle"
    >
      <span v-if="showTechnical" class="event__seq mono">{{ event.sequence }}</span>
      <span class="event__kind">
        <span class="event__dot" :class="`event__dot--${eventTone(event.type)}`" />
        <span class="event__label">{{ label }}</span>
      </span>
      <span class="event__summary grow">{{ summary }}</span>
      <span class="event__time nowrap" :title="formatRelative(event.occurred_at)">
        {{ formatClock(event.occurred_at) }}
      </span>
      <span v-if="showTechnical" class="event__chevron" :class="{ 'event__chevron--open': open }">
        ›
      </span>
    </div>
    <div v-if="showTechnical && open" class="event__details">
      <dl class="kv">
        <template v-if="sessionTitle">
          <dt>Session</dt>
          <dd>{{ sessionTitle }}</dd>
        </template>
        <template v-if="agentName">
          <dt>Agent</dt>
          <dd>{{ agentName }}</dd>
        </template>
        <dt>Event ID</dt>
        <dd class="mono">{{ event.event_id }}</dd>
        <dt>时间</dt>
        <dd>{{ formatRelative(event.occurred_at) }}</dd>
        <dt v-if="event.source?.adapter_id">Adapter</dt>
        <dd v-if="event.source?.adapter_id" class="mono">{{ event.source.adapter_id }}</dd>
        <template v-for="detail in details" :key="detail.key">
          <dt>{{ detail.key }}</dt>
          <dd class="mono">{{ detail.value }}</dd>
        </template>
      </dl>
    </div>
  </article>
</template>

<style scoped>
.event {
  overflow: hidden;
  background: transparent;
}

.event--open {
  background: var(--surface-2);
}

.event__head {
  display: flex;
  align-items: center;
  gap: 10px;
  width: 100%;
  min-height: 52px;
  padding: 10px 14px;
  border: 0;
  background: transparent;
  color: inherit;
  text-align: left;
}

.event__head[role='button'] {
  cursor: pointer;
}

.event__head:hover {
  background: var(--surface-2);
}

.event__head:active {
  background: var(--surface-2);
}

.event__seq {
  min-width: 30px;
  color: var(--faint);
  font-size: 10px;
}

.event__kind {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  flex-shrink: 0;
  width: 96px;
  color: var(--muted);
  font-size: 11px;
  line-height: 1.3;
}

.event__dot {
  flex-shrink: 0;
  width: 6px;
  height: 6px;
  border-radius: 50%;
  background: var(--faint);
}

.event__dot--success { background: var(--success); }
.event__dot--warning { background: var(--warning); }
.event__dot--danger { background: var(--danger); }
.event__dot--info { background: var(--info); }

.event__label {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.event__summary {
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  color: var(--fg);
  font-size: 13px;
}

.event--compact .event__summary {
  white-space: normal;
  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
  line-height: 1.45;
}

.event__time {
  flex-shrink: 0;
  min-width: 42px;
  color: var(--faint);
  font-size: 11px;
  font-variant-numeric: tabular-nums;
  text-align: right;
}

.event__chevron {
  color: var(--faint);
  font-size: 15px;
  transform: rotate(0deg);
  transition: transform 0.15s ease;
}

.event__chevron--open {
  transform: rotate(90deg);
}

.event__details {
  padding: 2px 14px 14px;
  border-top: 1px solid var(--border);
}

.event--compact {
  border: 1px solid var(--border);
  border-radius: 12px;
  background: var(--surface);
}

.event--compact .event__seq,
.event--compact .event__time {
  display: none;
}

.event--compact .event__head {
  min-height: 0;
  padding: 9px 11px;
}

.event--compact .event__kind {
  width: auto;
  max-width: 110px;
}

@media (max-width: 420px) {
  .event__kind {
    width: 94px;
  }

  .event__head {
    gap: 8px;
    padding: 10px 12px;
  }

  .event__summary {
    white-space: normal;
    display: -webkit-box;
    -webkit-line-clamp: 2;
    -webkit-box-orient: vertical;
    line-height: 1.4;
  }
}
</style>
