import type { ClientMode } from '@/services/storage/preferences';
import type { ClientTransport } from './types';
import { MockClient } from './MockClient';
import { WsClient } from './WsClient';

export type { ClientConfig, ClientTransport, CommandRequest, CommandType, TransportEvent } from './types';
export { MockClient } from './MockClient';
export { WsClient } from './WsClient';

export function createClient(mode: ClientMode): ClientTransport {
  return mode === 'mock' ? new MockClient() : new WsClient();
}
