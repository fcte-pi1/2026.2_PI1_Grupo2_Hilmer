import { Logger } from '@nestjs/common';
import { EventEmitter2 } from '@nestjs/event-emitter';
import { CONNECTION_EVENTS } from '../telemetry-connection.events';
import { ConnectionMonitorService } from './connection-monitor.service';

const ID = 'mm-01';

describe('ConnectionMonitorService', () => {
  let emit: jest.Mock;
  let monitor: ConnectionMonitorService;

  const emitted = (name: string) => emit.mock.calls.filter(([n]) => n === name);

  /** Envia `hz` mensagens por segundo durante `seconds` (relógio simulado). */
  const feed = (hz: number, seconds: number) => {
    const interval = 1000 / hz;
    for (let i = 0; i < Math.round(hz * seconds); i++) {
      jest.advanceTimersByTime(interval);
      monitor.recordMessage(ID, true);
    }
  };

  beforeEach(() => {
    jest.useFakeTimers();
    jest.spyOn(Logger.prototype, 'log').mockImplementation();
    jest.spyOn(Logger.prototype, 'warn').mockImplementation();
    emit = jest.fn();
    monitor = new ConnectionMonitorService({ emit } as unknown as EventEmitter2);
    monitor.onModuleInit();
  });

  afterEach(() => {
    monitor.onModuleDestroy();
    jest.useRealTimers();
    jest.restoreAllMocks();
  });

  describe('watchdog', () => {
    it('não emite nada enquanto os dados chegam a 10 Hz', () => {
      feed(10, 5);
      expect(emitted(CONNECTION_EVENTS.ROBOT_DISCONNECTED)).toHaveLength(0);
      expect(emitted(CONNECTION_EVENTS.RATE_LOW)).toHaveLength(0);
      expect(monitor.getRate(ID)).toBeGreaterThanOrEqual(9.5);
    });

    it('emite robot.disconnected quando passa mais de 1 s sem mensagens', () => {
      feed(10, 1);
      jest.advanceTimersByTime(1000); // exatamente 1 s de silêncio: ainda não
      expect(emitted(CONNECTION_EVENTS.ROBOT_DISCONNECTED)).toHaveLength(0);

      jest.advanceTimersByTime(100); // passou de 1 s
      expect(emit).toHaveBeenCalledWith(
        CONNECTION_EVENTS.ROBOT_DISCONNECTED,
        expect.objectContaining({ robotId: ID }),
      );
      expect(monitor.getState(ID)).toBe('offline');
    });

    it('emite robot.disconnected uma única vez por queda', () => {
      feed(10, 1);
      jest.advanceTimersByTime(10_000);
      expect(emitted(CONNECTION_EVENTS.ROBOT_DISCONNECTED)).toHaveLength(1);
    });

    it('emite robot.reconnected quando os dados voltam, com o tempo de queda', () => {
      feed(10, 1);
      jest.advanceTimersByTime(3000);
      monitor.recordMessage(ID, true);

      const calls = emitted(CONNECTION_EVENTS.ROBOT_RECONNECTED);
      expect(calls).toHaveLength(1);
      expect(calls[0][1].downtimeMs).toBeGreaterThan(1000);
      expect(monitor.getState(ID)).toBe('online');
    });

    it('detecta uma segunda queda depois de reconectar', () => {
      feed(10, 1);
      jest.advanceTimersByTime(2000);
      feed(10, 1);
      jest.advanceTimersByTime(2000);
      expect(emitted(CONNECTION_EVENTS.ROBOT_DISCONNECTED)).toHaveLength(2);
      expect(emitted(CONNECTION_EVENTS.ROBOT_RECONNECTED)).toHaveLength(1);
    });

    it('não considera queda o silêncio depois de RUN_END', () => {
      feed(10, 1);
      monitor.markFinished(ID);
      jest.advanceTimersByTime(10_000);
      expect(emitted(CONNECTION_EVENTS.ROBOT_DISCONNECTED)).toHaveLength(0);

      monitor.recordMessage(ID, false); // nova sessão: não é "reconexão"
      expect(emitted(CONNECTION_EVENTS.ROBOT_RECONNECTED)).toHaveLength(0);
      expect(monitor.getState(ID)).toBe('online');
    });
  });

  describe('taxa (Hz)', () => {
    it('mede ~10 Hz numa janela de 1 s', () => {
      feed(10, 3);
      expect(monitor.getRate(ID)).toBeCloseTo(10, 0);
    });

    it('emite telemetry.rate_low só depois de mais de 2 s abaixo de 10 Hz', () => {
      feed(5, 2.4);
      expect(emitted(CONNECTION_EVENTS.RATE_LOW)).toHaveLength(0);

      feed(5, 1.2);
      const calls = emitted(CONNECTION_EVENTS.RATE_LOW);
      expect(calls).toHaveLength(1);
      expect(calls[0][1]).toMatchObject({ robotId: ID });
      expect(calls[0][1].hz).toBeCloseTo(5, 0);
      expect(calls[0][1].lowForMs).toBeGreaterThan(2000);
    });

    it('não repete telemetry.rate_low enquanto a taxa continua baixa', () => {
      feed(5, 8);
      expect(emitted(CONNECTION_EVENTS.RATE_LOW)).toHaveLength(1);
    });

    it('uma janela ruim isolada não dispara rate_low', () => {
      feed(10, 2);
      feed(5, 1); // uma janela baixa
      feed(10, 3);
      expect(emitted(CONNECTION_EVENTS.RATE_LOW)).toHaveLength(0);
    });

    it('eventos (countsForRate=false) não entram na conta do Hz', () => {
      for (let i = 0; i < 40; i++) {
        jest.advanceTimersByTime(100);
        monitor.recordMessage(ID, i % 2 === 0); // metade telemetria, metade evento => 5 Hz
      }
      expect(emitted(CONNECTION_EVENTS.RATE_LOW)).toHaveLength(1);
    });

    it('não emite rate_low enquanto o robô está desconectado (o aviso certo é robot.disconnected)', () => {
      feed(10, 1);
      jest.advanceTimersByTime(8000);
      expect(emitted(CONNECTION_EVENTS.ROBOT_DISCONNECTED)).toHaveLength(1);
      expect(emitted(CONNECTION_EVENTS.RATE_LOW)).toHaveLength(0);
    });
  });

  describe('ligação com os eventos do TelemetryValidationService', () => {
    const envelope = (event?: string) => ({
      data: { robot_id: ID, type: event ? 'event' : 'telemetry', event } as any,
      recebido_em: new Date(),
    });

    it('telemetry.received alimenta o watchdog e o Hz', () => {
      for (let i = 0; i < 30; i++) {
        jest.advanceTimersByTime(100);
        monitor.onTelemetry(envelope());
      }
      expect(monitor.getState(ID)).toBe('online');
      expect(monitor.getRate(ID)).toBeCloseTo(10, 0);
    });

    it('run.finished de RUN_END para de vigiar o silêncio', () => {
      monitor.onTelemetry(envelope());
      monitor.onRunFinished(envelope('RUN_END'));
      jest.advanceTimersByTime(10_000);
      expect(emitted(CONNECTION_EVENTS.ROBOT_DISCONNECTED)).toHaveLength(0);
      expect(monitor.getState(ID)).toBe('finished');
    });

    it('run.finished disparado por GOAL_REACHED não para a vigilância (o robô ainda volta à origem)', () => {
      monitor.onTelemetry(envelope());
      monitor.onRunFinished(envelope('GOAL_REACHED'));
      jest.advanceTimersByTime(2000);
      expect(emitted(CONNECTION_EVENTS.ROBOT_DISCONNECTED)).toHaveLength(1);
    });
  });

});
