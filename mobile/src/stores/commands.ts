import { defineStore } from 'pinia';
import { ref } from 'vue';
import type { PermissionDecision } from '@/domain/protocol';
import { commandStatusLabel, decisionLabel } from '@/domain/labels';
import { impactLight, notifyError } from '@/services/native/haptics';
import type { CommandRequest } from '@/services/client/types';
import { useConnectionStore } from './connection';
import { useNodeStore } from './node';
import { useUiStore } from './ui';

export interface SentPrompt {
  sessionId: string;
  text: string;
  at: string;
}

const MAX_PROMPTS = 40;

export const useCommandsStore = defineStore('commands', () => {
  const connection = useConnectionStore();
  const nodeStore = useNodeStore();
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
        impactLight();
        if (successMessage) ui.push(successMessage, 'success');
        return true;
      }
      notifyError();
      ui.push(result.error ?? `命令${commandStatusLabel(result.status)}`, 'danger');
      return false;
    } catch (err) {
      notifyError();
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
      notifyError();
      ui.push('Prompt 不能为空', 'warning');
      return false;
    }
    const ok = await run(
      `prompt:${sessionId}`,
      {
        type: 'session.prompt',
        ...sessionContext(sessionId),
        payload: { text: trimmed },
      },
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
      { type: 'session.interrupt', ...sessionContext(sessionId), payload: {} },
      '已发送 Interrupt',
    );
  }

  async function respondPermission(
    permissionId: string,
    decision: PermissionDecision,
  ): Promise<boolean> {
    const permission = nodeStore.pendingPermissions.find(
      (item) => item.permission_id === permissionId,
    );
    const context = permission
      ? {
          ...(permission.agent_id ? { agentId: permission.agent_id } : {}),
          ...(permission.session_id ? { sessionId: permission.session_id } : {}),
        }
      : {};
    return run(
      `permission:${permissionId}`,
      {
        type: 'permission.respond',
        ...context,
        payload: { permission_id: permissionId, decision },
      },
      `已提交审批：${decisionLabel(decision)}`,
    );
  }

  function promptsFor(sessionId: string): SentPrompt[] {
    return sentPrompts.value.filter((promptRecord) => promptRecord.sessionId === sessionId);
  }

  function sessionContext(sessionId: string): { agentId?: string; sessionId: string } {
    const session = nodeStore.sessionById[sessionId];
    if (!session) return { sessionId };
    return { agentId: session.agent_id, sessionId };
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
