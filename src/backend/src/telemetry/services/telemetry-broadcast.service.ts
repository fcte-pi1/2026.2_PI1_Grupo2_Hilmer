import {
  Injectable,
  Logger,
  OnApplicationBootstrap,
  OnModuleDestroy,
} from '@nestjs/common';
import { HttpAdapterHost } from '@nestjs/core';
import { OnEvent } from '@nestjs/event-emitter';
import type { IncomingMessage, Server } from 'http';
import type { Duplex } from 'stream';
import { WebSocket, WebSocketServer } from 'ws';
import { EventPayloadDto, TelemetryPayloadDto } from '../dto/telemetry.dto';
import {
  IngestedMessageEnvelope,
  TELEMETRY_EVENTS,
} from './telemetry-validation.service';

@Injectable()
export class TelemetryBroadcastService
  implements OnApplicationBootstrap, OnModuleDestroy
{
  private readonly logger = new Logger(TelemetryBroadcastService.name);
  private wss: WebSocketServer | null = null;
  private readonly clients = new Set<WebSocket>();

  constructor(private readonly adapterHost: HttpAdapterHost) {}

  onApplicationBootstrap() {
    const server: Server | undefined =
      this.adapterHost.httpAdapter?.getHttpServer();

    if (!server) {
      this.logger.error(
        '[Broadcast] Servidor HTTP não disponível para anexar WebSocket!',
      );
      return;
    }

    // Cria o WSS em modo 'noServer' para gerenciar manualmente o handshake de upgrade
    this.wss = new WebSocketServer({ noServer: true });

    server.on(
      'upgrade',
      (req: IncomingMessage, socket: Duplex, head: Buffer) => {
        const url = req.url || '';
        // Garante que só intercepta requisições direcionadas para a rota /live
        if (url === '/live' || url.startsWith('/live?')) {
          this.wss?.handleUpgrade(req, socket, head, (ws) => {
            this.wss?.emit('connection', ws, req);
          });
        }
      },
    );

    this.wss.on('connection', (ws: WebSocket) => {
      this.clients.add(ws);
      this.logger.log(
        `[Broadcast] Cliente web conectado em /live. Total conectados: ${this.clients.size}`,
      );

      ws.on('close', () => {
        this.clients.delete(ws);
        this.logger.log(
          `[Broadcast] Cliente desconectado de /live. Restantes: ${this.clients.size}`,
        );
      });

      ws.on('error', (err) => {
        this.logger.error(`[Broadcast] Erro no socket cliente: ${err.message}`);
        this.clients.delete(ws);
      });
    });

    this.logger.log(
      '[Broadcast] Listener de upgrade WebSocket em /live ativado com sucesso.',
    );
  }

  onModuleDestroy() {
    if (this.wss) {
      for (const client of this.clients) {
        client.terminate();
      }
      this.wss.close();
    }
  }

  private broadcast(data: object) {
    if (this.clients.size === 0) return;
    const payload = JSON.stringify(data);

    for (const client of this.clients) {
      if (client.readyState === WebSocket.OPEN) {
        client.send(payload);
      }
    }
  }

  @OnEvent(TELEMETRY_EVENTS.TELEMETRY_RECEIVED)
  handleTelemetry(envelope: IngestedMessageEnvelope<TelemetryPayloadDto>) {
    this.broadcast({
      channel: 'telemetry',
      data: envelope.data,
      receivedAt: envelope.recebido_em,
    });
  }

  @OnEvent(TELEMETRY_EVENTS.RUN_STARTED)
  handleRunStarted(envelope: IngestedMessageEnvelope<EventPayloadDto>) {
    this.broadcast({
      channel: 'event',
      event: 'RUN_START',
      data: envelope.data,
      receivedAt: envelope.recebido_em,
    });
  }

  @OnEvent(TELEMETRY_EVENTS.RUN_FINISHED)
  handleRunFinished(envelope: IngestedMessageEnvelope<EventPayloadDto>) {
    this.broadcast({
      channel: 'event',
      event: 'RUN_END',
      data: envelope.data,
      receivedAt: envelope.recebido_em,
    });
  }

  @OnEvent(TELEMETRY_EVENTS.RUN_GOAL_REACHED)
  handleGoalReached(envelope: IngestedMessageEnvelope<EventPayloadDto>) {
    this.broadcast({
      channel: 'event',
      event: 'GOAL_REACHED',
      data: envelope.data,
      receivedAt: envelope.recebido_em,
    });
  }
}
