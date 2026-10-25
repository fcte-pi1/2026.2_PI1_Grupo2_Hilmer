
import {
  afterEach,
  beforeEach,
  describe,
  expect,
  it,
} from '@jest/globals';
import { createServer, type Server } from 'http';
import type { AddressInfo } from 'net';
import type { HttpAdapterHost } from '@nestjs/core';
import type {
  EventPayloadDto,
  TelemetryPayloadDto,
} from '../dto/telemetry.dto';
import type { IngestedMessageEnvelope } from './telemetry-validation.service';
import { TelemetryBroadcastService } from './telemetry-broadcast.service';
import WebSocket from 'ws';

describe('TelemetryBroadcastService', () => {
  let server: Server;
  let service: TelemetryBroadcastService;
  let baseUrl: string;
  let clients: WebSocket[];

  beforeEach(async () => {
    clients = [];
    server = createServer();

    const adapterHost = {
      httpAdapter: {
        getHttpServer: () => server,
      },
    } as unknown as HttpAdapterHost;

    service = new TelemetryBroadcastService(adapterHost);

    // Registra o listener de upgrade antes de iniciar o servidor.
    service.onApplicationBootstrap();

    await new Promise<void>((resolve) => {
      server.listen(0, '127.0.0.1', resolve);
    });

    const address = server.address() as AddressInfo;
    baseUrl = `ws://127.0.0.1:${address.port}`;
  });

  afterEach(async () => {
    // Encerra os clientes WebSocket do teste.
    for (const client of clients) {
      if (client.readyState !== WebSocket.CLOSED) {
        client.terminate();
      }
    }

    // Encerra o servidor WebSocket do serviço.
    service.onModuleDestroy();

    // Encerra o servidor HTTP.
    if (server.listening) {
      await new Promise<void>((resolve) => {
        server.close(() => resolve());
      });
    }
  });

  function connectClient(path = '/live'): Promise<WebSocket> {
    return new Promise((resolve, reject) => {
      const client = new WebSocket(`${baseUrl}${path}`);

      const onError = (error: Error) => {
        reject(error);
      };

      client.once('error', onError);

      client.once('open', () => {
        client.removeListener('error', onError);
        clients.push(client);
        resolve(client);
      });
    });
  }

  function waitForMessage(client: WebSocket): Promise<any> {
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        client.removeListener('message', onMessage);
        reject(new Error('Timeout aguardando mensagem WebSocket'));
      }, 2000);

      const onMessage = (raw: WebSocket.RawData) => {
        clearTimeout(timeout);
        resolve(JSON.parse(raw.toString()));
      };

      client.once('message', onMessage);
    });
  }

  it('deve aceitar um cliente WebSocket em /live', async () => {
    const client = await connectClient('/live');

    expect(client.readyState).toBe(WebSocket.OPEN);
  });

  it('deve transmitir telemetria no canal correto', async () => {
    const client = await connectClient();

    const receivedAt = new Date().toISOString();

    const envelope = {
      data: {
        robot_id: 'MOUSE_01',
        seq: 10,
        battery_pct: 75,
        speed_mm_s: 300,
      },
      recebido_em: receivedAt,
    } as unknown as IngestedMessageEnvelope<TelemetryPayloadDto>;

    const messagePromise = waitForMessage(client);

    service.handleTelemetry(envelope);

    const message = await messagePromise;

    expect(message).toEqual({
      channel: 'telemetry',
      data: envelope.data,
      receivedAt,
    });
  });

  it('deve transmitir o evento RUN_START', async () => {
    const client = await connectClient();

    const envelope = {
      data: {
        robot_id: 'MOUSE_01',
        seq: 1,
        event: 'RUN_START',
      },
      recebido_em: '2026-10-25T12:00:00.000Z',
    } as unknown as IngestedMessageEnvelope<EventPayloadDto>;

    const messagePromise = waitForMessage(client);

    service.handleRunStarted(envelope);

    const message = await messagePromise;

    expect(message).toEqual({
      channel: 'event',
      event: 'RUN_START',
      data: envelope.data,
      receivedAt: envelope.recebido_em,
    });
  });

  it('deve transmitir o evento RUN_END', async () => {
    const client = await connectClient();

    const envelope = {
      data: {
        robot_id: 'MOUSE_01',
        seq: 20,
        event: 'RUN_END',
      },
      recebido_em: '2026-10-25T12:00:10.000Z',
    } as unknown as IngestedMessageEnvelope<EventPayloadDto>;

    const messagePromise = waitForMessage(client);

    service.handleRunFinished(envelope);

    const message = await messagePromise;

    expect(message).toEqual({
      channel: 'event',
      event: 'RUN_END',
      data: envelope.data,
      receivedAt: envelope.recebido_em,
    });
  });

  it('deve transmitir o evento GOAL_REACHED', async () => {
    const client = await connectClient();

    const envelope = {
      data: {
        robot_id: 'MOUSE_01',
        seq: 19,
        event: 'GOAL_REACHED',
      },
      recebido_em: '2026-10-25T12:00:09.000Z',
    } as unknown as IngestedMessageEnvelope<EventPayloadDto>;

    const messagePromise = waitForMessage(client);

    service.handleGoalReached(envelope);

    const message = await messagePromise;

    expect(message).toEqual({
      channel: 'event',
      event: 'GOAL_REACHED',
      data: envelope.data,
      receivedAt: envelope.recebido_em,
    });
  });

  it('deve transmitir a mensagem para todos os clientes conectados', async () => {
    const client1 = await connectClient();
    const client2 = await connectClient();

    const envelope = {
      data: {
        robot_id: 'MOUSE_01',
        seq: 30,
        battery_pct: 60,
      },
      recebido_em: '2026-10-25T12:00:30.000Z',
    } as unknown as IngestedMessageEnvelope<TelemetryPayloadDto>;

    const message1Promise = waitForMessage(client1);
    const message2Promise = waitForMessage(client2);

    service.handleTelemetry(envelope);

    const [message1, message2] = await Promise.all([
      message1Promise,
      message2Promise,
    ]);

    expect(message1).toEqual(message2);
    expect(message1.channel).toBe('telemetry');
    expect(message1.data).toEqual(envelope.data);
  });

  it('não deve falhar quando não existem clientes conectados', () => {
    const envelope = {
      data: {
        robot_id: 'MOUSE_01',
        seq: 40,
      },
      recebido_em: new Date().toISOString(),
    } as unknown as IngestedMessageEnvelope<TelemetryPayloadDto>;

    expect(() => service.handleTelemetry(envelope)).not.toThrow();
  });
});
