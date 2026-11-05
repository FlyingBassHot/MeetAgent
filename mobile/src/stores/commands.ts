import { defineStore } from 'pinia';
import { ref } from 'vue';
import type { PermissionDecision } from '@/domain/protocol';
import { commandStatusLabel, decisionLabel } from '@/domain/labels';
import type { CommandRequest } from '@/services/client/types';
import { useConnectionStore } from './connection';
import { useUiStore } from './ui';

export interface SentPrompt {
  sessionId: string;
  text: string;
  at: string;
}

const MAX_PROMPTS = 40;

export const useCommandsStore = defineStore('commands', () => {
  const connection = useConnectionStore();
  const ui = useUiStore();

  const inFlight = ref<Record<string, boolean>>({});
  const sentPrompts = ref<SentPrompt[]>([]);

  function isBusy(key: string): boolean {
    return Boolean(inFlight.value[key]);
  }

  function sessionBusy(sessionId: string): boolean {
    return isBusy(`prompt:${sessionId}`) || isBusy(`interrupt:${sessionId}`);
  }

  async function run(
    key: string,
    request: CommandRequest,
    successMessage?: string,
  ): Promise<boolean> {
    if (inFlight.value[key]) return false;
    inFlight.value[key] = true;
    try {
      const result = await connection.sendCommand(request);
      if (result.status === 'completed') {
        if (successMessage) ui.push(successMessage, 'success');
        return true;
      }
      ui.push(result.error ?? `命令${commandStatusLabel(result.status)}`, 'danger');
      return false;
    } catch (err) {
      const message = err instanceof Error ? err.message : '命令发送失败';
      ui.push(message, 'danger');
      return false;
    } finally {
      delete inFlight.value[key];
    }
  }

  async function prompt(sessionId: string, text: string): Promise<boolean> {
    const trimmed = text.trim();
    if (trimmed.length === 0) {
      ui.push('Prompt 不能为空', 'warning');
      return false;
    }
    const ok = await run(
      `prompt:${sessionId}`,
      { type: 'session.prompt', sessionId, payload: { text: trimmed } },
      'Prompt 已发送',
    );
    if (ok) {
      sentPrompts.value.unshift({ sessionId, text: trimmed, at: new Date().toISOString() });
      if (sentPrompts.value.length > MAX_PROMPTS) {
        sentPrompts.value.splice(MAX_PROMPTS);
      }
    }
    return ok;
  }

  function interrupt(sessionId: string): Promise<boolean> {
    return run(
      `interrupt:${sessionId}`,
      { type: 'session.interrupt', sessionId, payload: {} },
      '已发送 Interrupt',
    );
  }

  async function respondPermission(
    permissionId: string,
    decision: PermissionDecision,
  ): Promise<boolean> {
    return run(
      `permission:${permissionId}`,
      {
        type: 'permission.respond',
        payload: { permission_id: permissionId, decision },
      },
      `已提交审批：${decisionLabel(decision)}`,
    );
  }

  function promptsFor(sessionId: string): SentPrompt[] {
    return sentPrompts.value.filter((promptRecord) => promptRecord.sessionId === sessionId);
  }

  return {
    inFlight,
    sentPrompts,
    isBusy,
    sessionBusy,
    prompt,
    interrupt,
    respondPermission,
    promptsFor,
  };
});
