import 'reflect-metadata';
import { validate } from 'class-validator';
import { plainToInstance } from 'class-transformer';
import {
  EventPayloadDto,
  TelemetryPayloadDto,
} from './telemetry.dto';

describe('Telemetry DTOs Validation', () => {
  describe('TelemetryPayloadDto', () => {
    const validTelemetry = {
      v: 1,
      type: 'telemetry',
      robot_id: 'mm-01',
      seq: 10,
      t_ms: 1000,
      pos: { x: 2, y: 3 },
      heading: 'N',
      speed_mm_s: 300.5,
      odo_mm: 1200,
      battery_pct: 95.5,
      walls: { n: true, e: false, s: false, w: true },
      state: 'EXPLORING',
    };

    it('deve validar com sucesso um payload completo e correto de telemetria', async () => {
      const dto = plainToInstance(TelemetryPayloadDto, validTelemetry);
      const errors = await validate(dto);
      expect(errors.length).toBe(0);
    });

    it('deve falhar se a versão do protocolo não for 1', async () => {
      const invalid = { ...validTelemetry, v: 2 };
      const dto = plainToInstance(TelemetryPayloadDto, invalid);
      const errors = await validate(dto);
      expect(errors.length).toBeGreaterThan(0);
      expect(errors[0].property).toBe('v');
    });

    it('deve falhar se a orientação (heading) for inválida', async () => {
      const invalid = { ...validTelemetry, heading: 'NORTE' };
      const dto = plainToInstance(TelemetryPayloadDto, invalid);
      const errors = await validate(dto);
      expect(errors.some((e) => e.property === 'heading')).toBe(true);
    });

    it('deve falhar se a bateria for menor que 0 ou maior que 100', async () => {
      const invalidNegative = { ...validTelemetry, battery_pct: -5 };
      const dtoNegative = plainToInstance(TelemetryPayloadDto, invalidNegative);
      const errorsNegative = await validate(dtoNegative);
      expect(errorsNegative.some((e) => e.property === 'battery_pct')).toBe(true);

      const invalidOver = { ...validTelemetry, battery_pct: 105 };
      const dtoOver = plainToInstance(TelemetryPayloadDto, invalidOver);
      const errorsOver = await validate(dtoOver);
      expect(errorsOver.some((e) => e.property === 'battery_pct')).toBe(true);
    });

    it('deve falhar se a velocidade for negativa', async () => {
      const invalid = { ...validTelemetry, speed_mm_s: -10 };
      const dto = plainToInstance(TelemetryPayloadDto, invalid);
      const errors = await validate(dto);
      expect(errors.some((e) => e.property === 'speed_mm_s')).toBe(true);
    });

    it('deve falhar se o estado do robô não estiver no enum', async () => {
      const invalid = { ...validTelemetry, state: 'FLYING' };
      const dto = plainToInstance(TelemetryPayloadDto, invalid);
      const errors = await validate(dto);
      expect(errors.some((e) => e.property === 'state')).toBe(true);
    });

    it('deve validar coordenadas de posição aninhadas', async () => {
      const invalidPos = { ...validTelemetry, pos: { x: -1, y: 2 } };
      const dto = plainToInstance(TelemetryPayloadDto, invalidPos);
      const errors = await validate(dto);
      expect(errors.some((e) => e.property === 'pos')).toBe(true);
    });
  });

  describe('EventPayloadDto', () => {
    it('deve validar evento CALIBRATION_DONE', async () => {
      const payload = {
        v: 1,
        type: 'event',
        robot_id: 'mm-01',
        seq: 0,
        t_ms: 0,
        event: 'CALIBRATION_DONE',
        ok: true,
      };
      const dto = plainToInstance(EventPayloadDto, payload);
      const errors = await validate(dto);
      expect(errors.length).toBe(0);
    });

    it('deve validar evento RUN_START', async () => {
      const payload = {
        v: 1,
        type: 'event',
        robot_id: 'mm-01',
        seq: 1,
        t_ms: 0,
        event: 'RUN_START',
        battery_pct: 98.0,
      };
      const dto = plainToInstance(EventPayloadDto, payload);
      const errors = await validate(dto);
      expect(errors.length).toBe(0);
    });

    it('deve validar evento GOAL_REACHED com posição', async () => {
      const payload = {
        v: 1,
        type: 'event',
        robot_id: 'mm-01',
        seq: 150,
        t_ms: 15000,
        event: 'GOAL_REACHED',
        pos: { x: 3, y: 3 },
      };
      const dto = plainToInstance(EventPayloadDto, payload);
      const errors = await validate(dto);
      expect(errors.length).toBe(0);
    });

    it('deve falhar para nome de evento inválido', async () => {
      const payload = {
        v: 1,
        type: 'event',
        robot_id: 'mm-01',
        seq: 1,
        t_ms: 0,
        event: 'INVALID_EVENT',
      };
      const dto = plainToInstance(EventPayloadDto, payload);
      const errors = await validate(dto);
      expect(errors.some((e) => e.property === 'event')).toBe(true);
    });
  });
});
