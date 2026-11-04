<script setup lang="ts">
withDefaults(
  defineProps<{
    /** 骨架行数 */
    rows?: number
    /** 每行高度（px） */
    rowHeight?: number
  }>(),
  { rows: 3, rowHeight: 56 },
)
</script>

<template>
  <div class="skeleton-group" role="status" aria-label="加载中">
    <div
      v-for="n in rows"
      :key="n"
      class="skeleton"
      :style="{ height: `${rowHeight}px` }"
    ></div>
  </div>
</template>
