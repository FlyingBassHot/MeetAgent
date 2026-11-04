<script setup lang="ts">
import { useEventViewStore } from '../../stores/eventView'

const view = useEventViewStore()
</script>

<template>
  <div class="event-filters" data-testid="event-filter">
    <div
      class="filter-row event-filter-selects"
      :class="{ open: view.filtersOpen }"
    >
      <label class="compact-field">
        <span>Node</span>
        <select v-model="view.nodeId" aria-label="按 Node 过滤">
          <option value="">全部 Node</option>
          <option v-for="option in view.nodeOptions" :key="option.value" :value="option.value">
            {{ option.label }}
          </option>
        </select>
      </label>

      <label class="compact-field">
        <span>Agent</span>
        <select v-model="view.agentId" aria-label="按 Agent 过滤">
          <option value="">全部 Agent</option>
          <option v-for="option in view.agentOptions" :key="option.value" :value="option.value">
            {{ option.label }}
          </option>
        </select>
      </label>

      <label class="compact-field grow-field">
        <span>Session</span>
        <select v-model="view.sessionId" aria-label="按 Session 过滤">
          <option value="">全部 Session</option>
          <option v-for="option in view.sessionOptions" :key="option.value" :value="option.value">
            {{ option.label }}
          </option>
        </select>
      </label>

      <label class="compact-field">
        <span>事件类型</span>
        <select v-model="view.eventType" aria-label="按事件类型过滤">
          <option value="">全部类型</option>
          <option v-for="type in view.eventTypeOptions" :key="type" :value="type">{{ type }}</option>
        </select>
      </label>
    </div>

    <div class="filter-row event-filter-search-row">
      <input
        v-model="view.search"
        class="filter-search"
        data-testid="event-search"
        type="search"
        placeholder="搜索类型 / Session / payload"
        aria-label="搜索事件"
        spellcheck="false"
      />

      <button
        type="button"
        class="button tiny ghost filter-toggle"
        data-testid="event-filter-toggle"
        :aria-expanded="view.filtersOpen"
        @click="view.toggleFilters()"
      >
        筛选<template v-if="view.activeFilterCount"> ({{ view.activeFilterCount }})</template>
      </button>

      <span
        class="event-filter-extra"
        :class="{ open: view.filtersOpen }"
      >
        <label class="toggle">
          <input v-model="view.rawMode" type="checkbox" />
          <span>原始模式</span>
        </label>

        <button
          type="button"
          class="button tiny"
          :class="view.follow ? 'primary' : 'ghost'"
          data-testid="event-follow"
          :aria-pressed="view.follow"
          @click="view.setFollow(!view.follow)"
        >
          {{ view.follow ? '暂停跟随' : '恢复跟随' }}
        </button>

        <button type="button" class="button tiny ghost" @click="view.resetFilters()">
          重置过滤
        </button>
        <span class="muted filter-count">{{ view.filteredEvents.length }} 条</span>
      </span>
    </div>
  </div>
</template>
