import { Logger } from '@nestjs/common';
import { WebSocket } from 'ws';
import { TelemetryIngestService } from './telemetry-ingest.service';
import { TelemetryValidationService } from './telemetry-validation.service';

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

async function waitFor(condition: () => boolean, timeoutMs = 1000) {
  const start = Date.now();
  while (!condition()) {
    if (Date.now() - start > timeoutMs)
      throw new Error('timeout esperando a condição');
    await sleep(5);
  }
}

/** Teste de integração com um servidor `ws` real numa porta aleatória e um validador falso. */
describe('TelemetryIngestService (/ingest)', () => {
  let ingest: TelemetryIngestService;
  let processRawMessage: jest.Mock;
  const clients: WebSocket[] = [];

  const connect = (path = '/ingest') =>
    new Promise<WebSocket>((resolve, reject) => {
      const ws = new WebSocket(`ws://127.0.0.1:${ingest.port}${path}`);
      clients.push(ws);
      ws.once('open', () => resolve(ws));
      ws.once('error', reject);
    });

  beforeEach(async () => {
    jest.spyOn(Logger.prototype, 'log').mockImplementation();
    jest.spyOn(Logger.prototype, 'warn').mockImplementation();
    jest.spyOn(Logger.prototype, 'error').mockImplementation();
    process.env.TELEMETRY_WS_PORT = '0';
    processRawMessage = jest.fn().mockResolvedValue(null);
    ingest = new TelemetryIngestService({
      processRawMessage,
    } as unknown as TelemetryValidationService);
    await ingest.onModuleInit();
  });

  afterEach(async () => {
    clients.splice(0).forEach((c) => c.terminate());
    await ingest.onModuleDestroy();
    delete process.env.TELEMETRY_WS_PORT;
    jest.restoreAllMocks();
  });

  it('entrega o texto de cada mensagem ao TelemetryValidationService', async () => {
    const ws = await connect();
    ws.send('{"v":1}');
    await waitFor(() => processRawMessage.mock.calls.length === 1);
    expect(processRawMessage).toHaveBeenCalledWith('{"v":1}');
  });

  it('processa as mensagens em ordem, mesmo que a primeira demore mais que a segunda', async () => {
    const log: string[] = [];
    processRawMessage.mockImplementation(async (raw: string) => {
      log.push(`início ${raw}`);
      if (raw === 'a') await sleep(50);
      log.push(`fim ${raw}`);
      return null;
    });

    const ws = await connect();
    ws.send('a');
    ws.send('b');
    await waitFor(() => log.length === 4);

    expect(log).toEqual(['início a', 'fim a', 'início b', 'fim b']);
  });

  it('erro ao processar uma mensagem não derruba a conexão nem as próximas', async () => {
    processRawMessage.mockRejectedValueOnce(new Error('bug'));

    const ws = await connect();
    ws.send('primeira');
    ws.send('segunda');
    await waitFor(() => processRawMessage.mock.calls.length === 2);

    expect(ws.readyState).toBe(WebSocket.OPEN);
  });

  it('RNF13: nunca envia nada de volta ao robô', async () => {
    const ws = await connect();
    const onMessage = jest.fn();
    ws.on('message', onMessage);

    ws.send('lixo');
    ws.send('{"v":1}');
    await waitFor(() => processRawMessage.mock.calls.length === 2);
    await sleep(100);

    expect(onMessage).not.toHaveBeenCalled();
  });

  it('só aceita conexões no caminho /ingest', async () => {
    await expect(connect('/outro')).rejects.toBeDefined();
  });
});
