import type {
  CommandResult,
  ConnectionPhase,
  JsonObject,
  ProtocolEvent,
  Snapshot,
  Welcome,
} from '@/domain/protocol';
import type { ClientMode } from '@/services/storage/preferences';

export interface ClientConfig {
  mode: ClientMode;
  baseUrl: string;
  token: string | null;
  pairingCode: string;
  deviceName: string;
  capabilities: string[];
  lastSequence: number;
}

export type TransportEvent =
  | { kind: 'phase'; phase: ConnectionPhase; detail?: string }
  | { kind: 'welcome'; welcome: Welcome }
  | { kind: 'snapshot'; snapshot: Snapshot }
  | { kind: 'event'; event: ProtocolEvent }
  | { kind: 'command-result'; result: CommandResult }
  | { kind: 'heartbeat'; rttMs: number; serverTime?: string }
  | { kind: 'error'; code: string; message: string; fatal: boolean };

export type CommandType =
  | 'ping'
  | 'node.status'
  | 'session.list'
  | 'session.prompt'
  | 'session.interrupt'
  | 'permission.respond';

export interface CommandRequest {
  type: CommandType;
  agentId?: string;
  sessionId?: string;
  payload: JsonObject;
}

export type TransportListener = (event: TransportEvent) => void;

export interface ClientTransport {
  connect(config: ClientConfig): void;
  disconnect(): void;
  setLastSequence(sequence: number): void;
  send(command: CommandRequest): Promise<CommandResult>;
  subscribe(listener: TransportListener): () => void;
}
