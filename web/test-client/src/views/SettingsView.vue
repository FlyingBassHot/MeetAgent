<script setup lang="ts">
import BaseCard from '../components/ui/BaseCard.vue'
import ConnectionCard from '../components/connection/ConnectionCard.vue'
import ThemeSwitcher from '../components/ui/ThemeSwitcher.vue'
import { PROTOCOL_VERSION } from '../lib/protocol'
</script>

<template>
  <div class="page-stack">
    <header class="view-header">
      <h2>设置</h2>
      <p>Cloud 连接配置、外观主题与协议说明。</p>
    </header>

    <ConnectionCard />

    <BaseCard title="外观" description="强调色与明暗模式即时生效，选择会持久化到本机">
      <div class="settings-row">
        <div>
          <strong>主题</strong>
          <p class="muted settings-caption">6 套强调色 · 暗色 / 亮色双模式</p>
        </div>
        <ThemeSwitcher />
      </div>
    </BaseCard>

    <BaseCard title="协议说明" description="MeetAgent Cloud · JSON Envelope over WebSocket">
      <dl class="about-list">
        <div>
          <dt>协议版本</dt>
          <dd><code>v{{ PROTOCOL_VERSION }}</code></dd>
        </div>
        <div>
          <dt>注册</dt>
          <dd>携带 last_sequences，断线重连后自动补发事件</dd>
        </div>
        <div>
          <dt>事件</dt>
          <dd>node_id:sequence 去重 · 按时间排序 · 上限 300 条</dd>
        </div>
        <div>
          <dt>心跳 / 超时</dt>
          <dd>20s 心跳 · 15s 命令超时 · 10s 注册超时</dd>
        </div>
        <div>
          <dt>事件补发</dt>
          <dd>分页拉取（每页 1000 条，最多 5 页），截断时回退完整快照</dd>
        </div>
        <div>
          <dt>技术栈</dt>
          <dd>Vue 3 + TypeScript + Vite</dd>
        </div>
      </dl>
    </BaseCard>
  </div>
</template>
