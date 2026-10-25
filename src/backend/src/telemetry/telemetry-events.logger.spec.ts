import { Test, TestingModule } from '@nestjs/testing';
import { TelemetryEventsLogger } from './telemetry-events.logger';
import { IngestedMessageEnvelope } from './services/telemetry-validation.service';
import {
  RateLowPayload,
  RobotDisconnectedPayload,
  RobotReconnectedPayload,
} from './telemetry-connection.events';

describe('TelemetryEventsLogger', () => {
  let logger: TelemetryEventsLogger;

  beforeEach(async () => {
    const module: TestingModule = await Test.createTestingModule({
      providers: [TelemetryEventsLogger],
    }).compile();

    logger = module.get<TelemetryEventsLogger>(TelemetryEventsLogger);
  });

  it('deve processar onReceived e registrar log a cada 10 pacotes', () => {
    const envelope: IngestedMessageEnvelope = {
      data: { seq: 1 } as any,
      recebido_em: new Date(),
    };

    // Chamadas de 1 a 9 (não emite log de múltiplo de 10)
    for (let i = 1; i <= 9; i++) {
      envelope.data.seq = i;
      expect(() => logger.onReceived(envelope)).not.toThrow();
    }

    // 10ª chamada (múltiplo de 10, dispara o log interno)
    envelope.data.seq = 10;
    expect(() => logger.onReceived(envelope)).not.toThrow();
  });

  it('deve processar onCalibrated', () => {
    const envelope: IngestedMessageEnvelope = {
      data: { seq: 2 } as any,
      recebido_em: new Date(),
    };
    expect(() => logger.onCalibrated(envelope)).not.toThrow();
  });

  it('deve processar onStarted', () => {
    const envelope: IngestedMessageEnvelope = {
      data: { seq: 3 } as any,
      recebido_em: new Date(),
    };
    expect(() => logger.onStarted(envelope)).not.toThrow();
  });

  it('deve processar onGoal', () => {
    const envelope: IngestedMessageEnvelope = {
      data: { t_ms: 15400 } as any,
      recebido_em: new Date(),
    };
    expect(() => logger.onGoal(envelope)).not.toThrow();
  });

  it('deve processar onFinished', () => {
    const envelope: IngestedMessageEnvelope = {
      data: { seq: 42 } as any,
      recebido_em: new Date(),
    };
    expect(() => logger.onFinished(envelope)).not.toThrow();
  });

  it('deve processar onDisconnected', () => {
    const payload: RobotDisconnectedPayload = {
      robotId: 'mm-01',
      lastSeenAt: new Date(),
      at: new Date(),
    };
    expect(() => logger.onDisconnected(payload)).not.toThrow();
  });

  it('deve processar onReconnected', () => {
    const payload: RobotReconnectedPayload = {
      robotId: 'mm-01',
      downtimeMs: 2500,
      at: new Date(),
    };
    expect(() => logger.onReconnected(payload)).not.toThrow();
  });

  it('deve processar onRateLow', () => {
    const payload: RateLowPayload = {
      robotId: 'mm-01',
      hz: 3.2,
      lowForMs: 1000,
      at: new Date(),
    };
    expect(() => logger.onRateLow(payload)).not.toThrow();
  });
});
