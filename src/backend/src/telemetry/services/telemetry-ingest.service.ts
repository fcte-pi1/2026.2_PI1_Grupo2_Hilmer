import { Injectable, Logger, OnModuleDestroy, OnModuleInit } from '@nestjs/common';
import { RawData, WebSocket, WebSocketServer } from 'ws';
import { TelemetryValidationService } from './telemetry-validation.service';

export const INGEST_PATH = '/ingest';
export const DEFAULT_INGEST_PORT = 3001;
/** Mensagens reais têm ~250 bytes; o limite só protege contra lixo gigante. */
const MAX_PAYLOAD_BYTES = 64 * 1024;

/**
 * Servidor WebSocket puro (`ws`) de ingestão: ws://<host>:3001/ingest.
 *
 * Fica separado do adaptador global do NestJS para não conflitar com o Socket.IO
 * que o navegador vai usar. Só aceita conexões, lê o texto das mensagens e entrega
 * ao TelemetryValidationService. Não grava no banco, não fala com o navegador e,
 * por RNF13, NUNCA envia nada ao robô (não existe socket.send neste arquivo).
 */
@Injectable()
export class TelemetryIngestService implements OnModuleInit, OnModuleDestroy {
  private readonly logger = new Logger(TelemetryIngestService.name);
  private wss?: WebSocketServer;

  constructor(private readonly validation: TelemetryValidationService) {}

  /** Porta efetiva (útil em testes com TELEMETRY_WS_PORT=0). */
  get port(): number | undefined {
    const address = this.wss?.address();
    return address && typeof address === 'object' ? address.port : undefined;
  }

  async onModuleInit(): Promise<void> {
    const port = Number(process.env.TELEMETRY_WS_PORT ?? DEFAULT_INGEST_PORT);

    await new Promise<void>((resolve, reject) => {
      const wss = new WebSocketServer({ port, path: INGEST_PATH, maxPayload: MAX_PAYLOAD_BYTES });
      const onStartupError = (err: Error) => reject(err);
      wss.once('error', onStartupError);
      wss.once('listening', () => {
        wss.off('error', onStartupError);
        wss.on('error', (err) => this.logger.error(`Erro no servidor WebSocket: ${err.message}`));
        resolve();
      });
      this.wss = wss;
    });

    this.wss?.on('connection', (socket, req) =>
      this.onConnection(socket, req.socket.remoteAddress ?? 'desconhecido'),
    );
    this.logger.log(`Ingestão WebSocket ouvindo em ws://0.0.0.0:${this.port}${INGEST_PATH}`);
  }

  async onModuleDestroy(): Promise<void> {
    const wss = this.wss;
    if (!wss) return;
    for (const client of wss.clients) client.terminate();
    await new Promise<void>((resolve) => wss.close(() => resolve()));
  }

  private onConnection(socket: WebSocket, remote: string): void {
    this.logger.log(`Cliente conectado em ${INGEST_PATH}: ${remote}`);

    // processRawMessage é assíncrono (class-validator) e o controle de seq depende da
    // ordem de chegada. Uma fila por conexão garante que cada mensagem só começa a ser
    // processada quando a anterior terminou.
    let queue: Promise<unknown> = Promise.resolve();

    socket.on('message', (data: RawData) => {
      const raw = rawToString(data);
      queue = queue
        .then(() => this.validation.processRawMessage(raw))
        // Um bug nosso não pode derrubar o processo nem a conexão do robô.
        .catch((err: Error) =>
          this.logger.error(`Falha inesperada ao processar mensagem: ${err.message}`),
        );
    });
    socket.on('error', (err) => this.logger.warn(`Erro no socket de ${remote}: ${err.message}`));
    socket.on('close', (code) => this.logger.log(`Cliente desconectado: ${remote} (código ${code})`));
  }
}

function rawToString(data: RawData): string {
  if (Array.isArray(data)) return Buffer.concat(data).toString('utf8');
  if (data instanceof ArrayBuffer) return Buffer.from(data).toString('utf8');
  return data.toString('utf8');
}
