import { EventEmitter2 } from '@nestjs/event-emitter';
import { SequenceTrackerService } from './sequence-tracker.service';
import {
  TELEMETRY_EVENTS,
  TelemetryValidationService,
} from './telemetry-validation.service';

describe('TelemetryValidationService', () => {
  let service: TelemetryValidationService;
  let sequenceTracker: SequenceTrackerService;
  let eventEmitter: EventEmitter2;

  beforeEach(() => {
    sequenceTracker = new SequenceTrackerService();
    eventEmitter = new EventEmitter2();
    jest.spyOn(eventEmitter, 'emit');

    service = new TelemetryValidationService(sequenceTracker, eventEmitter);
  });

  describe('Resiliência e Parsing de JSON (RNF13)', () => {
    it('deve descartar JSON com sintaxe inválida sem lançar exceção', async () => {
      const invalidJson = '{ v: 1, type: "telemetry", broken... ';

      const result = await service.processRawMessage(invalidJson);

      expect(result).toBeNull();
      const stats = service.getStats();
      expect(stats.totalReceived).toBe(1);
      expect(stats.malformedJson).toBe(1);
      expect(stats.validProcessed).toBe(0);
      expect(eventEmitter.emit).not.toHaveBeenCalled();
    });

    it('deve descartar JSON que não é um objeto (ex: primitivo ou array)', async () => {
      const primitiveJson = '12345';

      const result = await service.processRawMessage(primitiveJson);

      expect(result).toBeNull();
      expect(service.getStats().malformedJson).toBe(1);
    });

    it('deve processar mensagem enviada como Buffer', async () => {
      const validMsg = JSON.stringify({
        v: 1,
        type: 'event',
        robot_id: 'mm-01',
        seq: 0,
        t_ms: 0,
        event: 'CALIBRATION_DONE',
        ok: true,
      });
      const buffer = Buffer.from(validMsg, 'utf-8');

      const result = await service.processRawMessage(buffer);

      expect(result).not.toBeNull();
      expect(service.getStats().validProcessed).toBe(1);
    });
  });

  describe('Validação do Contrato DTO', () => {
    it('deve descartar pacote com tipo de mensagem inválido', async () => {
      const invalidTypeMsg = JSON.stringify({
        v: 1,
        type: 'unsupported_type',
        robot_id: 'mm-01',
        seq: 0,
        t_ms: 0,
      });

      const result = await service.processRawMessage(invalidTypeMsg);

      expect(result).toBeNull();
      expect(service.getStats().validationFailed).toBe(1);
    });

    it('deve descartar pacote com dados inválidos (ex: bateria > 100)', async () => {
      const invalidBatteryMsg = JSON.stringify({
        v: 1,
        type: 'telemetry',
        robot_id: 'mm-01',
        seq: 1,
        t_ms: 100,
        pos: { x: 0, y: 0 },
        heading: 'N',
        speed_mm_s: 300,
        battery_pct: 150, // Inválido
        state: 'EXPLORING',
      });

      const result = await service.processRawMessage(invalidBatteryMsg);

      expect(result).toBeNull();
      expect(service.getStats().validationFailed).toBe(1);
    });
  });

  describe('Filtro de Sequência (seq)', () => {
    it('deve descartar pacote duplicado e não emitir eventos', async () => {
      const validTelemetry = {
        v: 1,
        type: 'telemetry',
        robot_id: 'mm-01',
        seq: 10,
        t_ms: 1000,
        pos: { x: 1, y: 1 },
        heading: 'E',
        speed_mm_s: 290,
        battery_pct: 95,
        state: 'EXPLORING',
      };

      // Primeiro envio
      const res1 = await service.processRawMessage(JSON.stringify(validTelemetry));
      expect(res1).not.toBeNull();
      expect(eventEmitter.emit).toHaveBeenCalledTimes(1);

      // Reenvio (duplicata de seq 10)
      const res2 = await service.processRawMessage(JSON.stringify(validTelemetry));
      expect(res2).toBeNull();
      expect(service.getStats().duplicateSeq).toBe(1);
      // O contador de emissões permanece em 1
      expect(eventEmitter.emit).toHaveBeenCalledTimes(1);
    });
  });

  describe('Emissão de Eventos Internos no Barramento NestJS', () => {
    it('deve emitir telemetry.received com envelope de timestamp para pacote de telemetria', async () => {
      const msg = JSON.stringify({
        v: 1,
        type: 'telemetry',
        robot_id: 'mm-01',
        seq: 5,
        t_ms: 500,
        pos: { x: 2, y: 3 },
        heading: 'S',
        speed_mm_s: 310,
        battery_pct: 90,
        state: 'EXPLORING',
      });

      await service.processRawMessage(msg);

      expect(eventEmitter.emit).toHaveBeenCalledWith(
        TELEMETRY_EVENTS.TELEMETRY_RECEIVED,
        expect.objectContaining({
          data: expect.objectContaining({
            robot_id: 'mm-01',
            seq: 5,
            type: 'telemetry',
          }),
          recebido_em: expect.any(Date),
        }),
      );
    });

    it('deve emitir run.started para evento RUN_START', async () => {
      const msg = JSON.stringify({
        v: 1,
        type: 'event',
        robot_id: 'mm-01',
        seq: 1,
        t_ms: 0,
        event: 'RUN_START',
        battery_pct: 98,
      });

      await service.processRawMessage(msg);

      expect(eventEmitter.emit).toHaveBeenCalledWith(
        TELEMETRY_EVENTS.RUN_STARTED,
        expect.objectContaining({
          data: expect.objectContaining({ event: 'RUN_START' }),
          recebido_em: expect.any(Date),
        }),
      );
    });

    it('deve emitir run.calibrated para evento CALIBRATION_DONE', async () => {
      const msg = JSON.stringify({
        v: 1,
        type: 'event',
        robot_id: 'mm-01',
        seq: 0,
        t_ms: 0,
        event: 'CALIBRATION_DONE',
        ok: true,
      });

      await service.processRawMessage(msg);

      expect(eventEmitter.emit).toHaveBeenCalledWith(
        TELEMETRY_EVENTS.RUN_CALIBRATED,
        expect.objectContaining({
          data: expect.objectContaining({ event: 'CALIBRATION_DONE' }),
        }),
      );
    });

    it('deve emitir run.goal_reached e run.finished para evento GOAL_REACHED', async () => {
      const msg = JSON.stringify({
        v: 1,
        type: 'event',
        robot_id: 'mm-01',
        seq: 100,
        t_ms: 10000,
        event: 'GOAL_REACHED',
        pos: { x: 3, y: 3 },
      });

      await service.processRawMessage(msg);

      expect(eventEmitter.emit).toHaveBeenCalledWith(
        TELEMETRY_EVENTS.RUN_GOAL_REACHED,
        expect.any(Object),
      );
      expect(eventEmitter.emit).toHaveBeenCalledWith(
        TELEMETRY_EVENTS.RUN_FINISHED,
        expect.any(Object),
      );
    });

    it('deve emitir run.finished para evento RUN_END', async () => {
      const msg = JSON.stringify({
        v: 1,
        type: 'event',
        robot_id: 'mm-01',
        seq: 105,
        t_ms: 10500,
        event: 'RUN_END',
      });

      await service.processRawMessage(msg);

      expect(eventEmitter.emit).toHaveBeenCalledWith(
        TELEMETRY_EVENTS.RUN_FINISHED,
        expect.any(Object),
      );
    });
  });
});
