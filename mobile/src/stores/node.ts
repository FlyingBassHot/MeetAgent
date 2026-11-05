import { defineStore } from 'pinia';
import { computed, ref } from 'vue';
import type {
  AdapterInfo,
  AgentView,
  JsonObject,
  NodeInfo,
  PermissionView,
  ProtocolEvent,
  SessionView,
  Snapshot,
  Welcome,
} from '@/domain/protocol';
import { SCHEMA_VERSION } from '@/domain/protocol';
import { parseEventPayload, textField } from '@/domain/payload';

const MAX_EVENTS = 500;

export const useNodeStore = defineStore('node', () => {
  const node = ref<NodeInfo | null>(null);
  const adapters = ref<AdapterInfo[]>([]);
  const agents = ref<AgentView[]>([]);
  const events = ref<ProtocolEvent[]>([]);
  const pendingPermissions = ref<PermissionView[]>([]);
  const lastSequence = ref(0);
  const serverTime = ref<string | null>(null);
  const sequenceGap = ref(false);
  const connectedAt = ref<string | null>(null);

  const agentById = computed<Record<string, AgentView>>(() => {
    const map: Record<string, AgentView> = {};
    for (const agent of agents.value) map[agent.agent_id] = agent;
    return map;
  });

  const sessionById = computed<Record<string, SessionView>>(() => {
    const map: Record<string, SessionView> = {};
    for (const agent of agents.value) {
      for (const session of agent.sessions) map[session.session_id] = session;
    }
    return map;
  });

  const sessions = computed<SessionView[]>(() =>
    Object.values(sessionById.value).sort(
      (left, right) => Date.parse(right.updated_at) - Date.parse(left.updated_at),
    ),
  );

  const runningSessions = computed(() =>
    sessions.value.filter((session) => session.status === 'running'),
  );

  const waitingSessions = computed(() =>
    sessions.value.filter((session) => session.status === 'waiting'),
  );

  const pendingBySession = computed<Record<string, PermissionView[]>>(() => {
    const map: Record<string, PermissionView[]> = {};
    for (const permission of pendingPermissions.value) {
      if (!permission.session_id) continue;
      const list = map[permission.session_id] ?? [];
      list.push(permission);
      map[permission.session_id] = list;
    }
    return map;
  });

  const recentEvents = computed(() => [...events.value].reverse());

  function applySnapshot(snapshot: Snapshot): void {
    node.value = snapshot.node ?? null;
    adapters.value = snapshot.adapters ?? [];
    agents.value = snapshot.agents ?? [];
    events.value = [...(snapshot.events ?? [])]
      .sort((left, right) => left.sequence - right.sequence)
      .slice(-MAX_EVENTS);
    pendingPermissions.value = snapshot.pending_permissions ?? [];
    lastSequence.value = snapshot.last_sequence ?? 0;
    serverTime.value = snapshot.server_time ?? null;
    sequenceGap.value = false;
    connectedAt.value = new Date().toISOString();
  }

  function applyWelcome(welcome: Welcome): void {
    applySnapshot(welcome.snapshot);
    node.value = welcome.node;
    connectedAt.value = new Date().toISOString();
  }

  function applyEvent(event: ProtocolEvent): void {
    if (event.schema_version !== SCHEMA_VERSION) return;
    if (event.sequence <= lastSequence.value) return;
    if (lastSequence.value > 0 && event.sequence > lastSequence.value + 1) {
      sequenceGap.value = true;
    }
    lastSequence.value = event.sequence;
    applyEventToState(event);
    events.value.push(event);
    if (events.value.length > MAX_EVENTS) {
      events.value.splice(0, events.value.length - MAX_EVENTS);
    }
  }

  function applyEventToState(event: ProtocolEvent): void {
    const payload = parseEventPayload(event);
    const sessionId = event.session_id ?? textField(payload, 'session_id');
    switch (event.type) {
      case 'agent.status_changed':
        updateAgentStatus(event.agent_id, textField(payload, 'status'));
        break;
      case 'session.created':
      case 'session.updated':
        upsertSession(event, payload);
        break;
      case 'session.status_changed':
        updateSessionStatus(sessionId, textField(payload, 'status'), event.occurred_at);
        break;
      case 'session.deleted':
        removeSession(sessionId);
        break;
      case 'permission.requested':
        upsertPermission(event, payload);
        break;
      case 'permission.resolved':
        removePermission(textField(payload, 'permission_id'));
        break;
      case 'message.started':
      case 'message.delta':
      case 'message.completed':
      case 'tool.started':
      case 'tool.completed':
      case 'tool.failed':
        touchSession(sessionId, event.occurred_at);
        break;
      default:
        break;
    }
  }

  function updateAgentStatus(agentId: string | undefined, status: string | undefined): void {
    if (!agentId || !status) return;
    const agent = agentById.value[agentId];
    if (agent) agent.status = status;
  }

  function updateSessionStatus(
    sessionId: string | undefined,
    status: string | undefined,
    occurredAt: string,
  ): void {
    if (!sessionId || !status) return;
    const session = sessionById.value[sessionId];
    if (!session) return;
    session.status = status;
    session.updated_at = occurredAt;
  }

  function touchSession(sessionId: string | undefined, occurredAt: string): void {
    if (!sessionId) return;
    const session = sessionById.value[sessionId];
    if (!session) return;
    if (Date.parse(occurredAt) > Date.parse(session.updated_at)) {
      session.updated_at = occurredAt;
    }
  }

  function upsertSession(event: ProtocolEvent, payload: JsonObject): void {
    const sessionId = textField(payload, 'session_id') ?? event.session_id;
    if (!sessionId) return;
    const existing = sessionById.value[sessionId];
    const status = textField(payload, 'status');
    const title = textField(payload, 'title');
    const updatedAt = textField(payload, 'updated_at') ?? event.occurred_at;
    if (existing) {
      if (title) existing.title = title;
      if (status) existing.status = status;
      existing.updated_at = updatedAt;
      return;
    }
    if (event.type === 'session.updated') return;
    const agentId = event.agent_id ?? textField(payload, 'agent_id');
    if (!agentId) return;
    const agent = agentById.value[agentId];
    if (!agent) return;
    const workingDirectory = textField(payload, 'working_directory');
    const session: SessionView = {
      session_id: sessionId,
      agent_id: agentId,
      node_id: event.node_id,
      status: status ?? 'idle',
      updated_at: updatedAt,
    };
    if (title) session.title = title;
    if (workingDirectory) session.working_directory = workingDirectory;
    agent.sessions.push(session);
  }

  function removeSession(sessionId: string | undefined): void {
    if (!sessionId) return;
    for (const agent of agents.value) {
      const index = agent.sessions.findIndex((session) => session.session_id === sessionId);
      if (index >= 0) {
        agent.sessions.splice(index, 1);
        break;
      }
    }
  }

  function upsertPermission(event: ProtocolEvent, payload: JsonObject): void {
    const permissionId = textField(payload, 'permission_id');
    if (!permissionId) return;
    const status = textField(payload, 'status');
    if (status && status !== 'pending') {
      removePermission(permissionId);
      return;
    }
    if (pendingPermissions.value.some((item) => item.permission_id === permissionId)) return;
    const sessionId = textField(payload, 'session_id') ?? event.session_id;
    const agentId = textField(payload, 'agent_id') ?? event.agent_id;
    const permission: PermissionView = {
      permission_id: permissionId,
      node_id: event.node_id,
      status: 'pending',
      created_at: textField(payload, 'created_at') ?? event.occurred_at,
    };
    const description = textField(payload, 'description');
    const expiresAt = textField(payload, 'expires_at');
    if (description) permission.description = description;
    if (expiresAt) permission.expires_at = expiresAt;
    if (sessionId) permission.session_id = sessionId;
    if (agentId) permission.agent_id = agentId;
    pendingPermissions.value.push(permission);
  }

  function removePermission(permissionId: string | undefined): void {
    if (!permissionId) return;
    pendingPermissions.value = pendingPermissions.value.filter(
      (permission) => permission.permission_id !== permissionId,
    );
  }

  function reset(): void {
    node.value = null;
    adapters.value = [];
    agents.value = [];
    events.value = [];
    pendingPermissions.value = [];
    lastSequence.value = 0;
    serverTime.value = null;
    sequenceGap.value = false;
    connectedAt.value = null;
  }

  function sessionEvents(sessionId: string): ProtocolEvent[] {
    return events.value.filter((event) => event.session_id === sessionId);
  }

  function pendingFor(sessionId: string): PermissionView[] {
    return pendingBySession.value[sessionId] ?? [];
  }

  function agentFor(sessionId: string): AgentView | undefined {
    const session = sessionById.value[sessionId];
    if (!session) return undefined;
    return agentById.value[session.agent_id];
  }

  return {
    node,
    adapters,
    agents,
    events,
    pendingPermissions,
    lastSequence,
    serverTime,
    sequenceGap,
    connectedAt,
    agentById,
    sessionById,
    sessions,
    runningSessions,
    waitingSessions,
    pendingBySession,
    recentEvents,
    applySnapshot,
    applyWelcome,
    applyEvent,
    reset,
    sessionEvents,
    pendingFor,
    agentFor,
  };
});
