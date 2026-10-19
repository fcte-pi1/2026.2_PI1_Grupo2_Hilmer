import { Test, TestingModule } from '@nestjs/testing';
import { PrismaService } from '../prisma/prisma.service';
import { TelemetryPayloadDto } from '../telemetry/dto/telemetry.dto';
import { RunsService } from './runs.service';

describe('RunsService', () => {
  let service: RunsService;
  let prisma: {
    labirinto: { upsert: jest.Mock };
    corrida: {
      create: jest.Mock;
      update: jest.Mock;
      findFirst: jest.Mock;
      findMany: jest.Mock;
      findUnique: jest.Mock;
      count: jest.Mock;
    };
    telemetria: { create: jest.Mock };
  };

  const mockLabirinto = { id: 'lab-1', dimensao: '4x4' };
  const mockCorrida = {
    id: 'run-1',
    labirintoId: 'lab-1',
    status: 'EM_ANDAMENTO',
    iniciadaEm: new Date('2026-10-17T12:00:00Z'),
    finalizadaEm: null,
  };

  beforeEach(async () => {
    prisma = {
      labirinto: {
        upsert: jest.fn().mockResolvedValue(mockLabirinto),
      },
      corrida: {
        create: jest.fn().mockResolvedValue(mockCorrida),
        update: jest.fn().mockImplementation(({ where, data }) => ({
          ...mockCorrida,
          id: where.id,
          status: data.status,
          finalizadaEm: data.finalizadaEm,
        })),
        findFirst: jest.fn().mockResolvedValue(mockCorrida),
        findMany: jest.fn().mockResolvedValue([mockCorrida]),
        findUnique: jest.fn().mockResolvedValue(mockCorrida),
        count: jest.fn().mockResolvedValue(1),
      },
      telemetria: {
        create: jest.fn().mockImplementation(({ data }) => ({
          id: 'telem-1',
          ...data,
        })),
      },
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        RunsService,
        {
          provide: PrismaService,
          useValue: prisma,
        },
      ],
    }).compile();

    service = module.get<RunsService>(RunsService);
  });

  describe('startRun', () => {
    it('deve iniciar uma nova corrida com status EM_ANDAMENTO', async () => {
      const result = await service.startRun({
        robotId: 'ROBOT_1',
        dimensaoLabirinto: '4x4',
      });

      expect(prisma.labirinto.upsert).toHaveBeenCalledWith({
        where: { dimensao: '4x4' },
        update: {},
        create: { dimensao: '4x4' },
      });
      expect(prisma.corrida.create).toHaveBeenCalledWith({
        data: expect.objectContaining({
          labirintoId: 'lab-1',
          status: 'EM_ANDAMENTO',
        }),
      });
      expect(result).toBeDefined();
      expect(service.getActiveRunId('ROBOT_1')).toBe('run-1');
    });

    it('deve finalizar corrida anterior se o robô já possuir corrida aberta', async () => {
      await service.startRun({ robotId: 'ROBOT_1' });
      prisma.corrida.create.mockResolvedValueOnce({
        ...mockCorrida,
        id: 'run-2',
      });

      await service.startRun({ robotId: 'ROBOT_1' });

      expect(prisma.corrida.update).toHaveBeenCalledWith(
        expect.objectContaining({
          where: { id: 'run-1' },
          data: expect.objectContaining({ status: 'ABORTADA' }),
        }),
      );
      expect(service.getActiveRunId('ROBOT_1')).toBe('run-2');
    });

    it('deve retornar null sem lançar exceção se o Prisma falhar', async () => {
      prisma.corrida.create.mockRejectedValueOnce(new Error('DB Timeout'));

      const result = await service.startRun({ robotId: 'ROBOT_ERR' });
      expect(result).toBeNull();
    });
  });

  describe('finishRun', () => {
    it('deve marcar a corrida como FINALIZADA quando goalReached for true', async () => {
      await service.startRun({ robotId: 'ROBOT_1' });

      const result = await service.finishRun({
        robotId: 'ROBOT_1',
        goalReached: true,
      });

      expect(prisma.corrida.update).toHaveBeenCalledWith({
        where: { id: 'run-1' },
        data: expect.objectContaining({
          status: 'FINALIZADA',
        }),
      });
      expect(result?.status).toBe('FINALIZADA');
      expect(service.getActiveRunId('ROBOT_1')).toBeUndefined();
    });

    it('deve marcar a corrida como ABORTADA quando goalReached for false', async () => {
      await service.startRun({ robotId: 'ROBOT_1' });

      const result = await service.finishRun({
        robotId: 'ROBOT_1',
        goalReached: false,
      });

      expect(prisma.corrida.update).toHaveBeenCalledWith({
        where: { id: 'run-1' },
        data: expect.objectContaining({
          status: 'ABORTADA',
        }),
      });
      expect(result?.status).toBe('ABORTADA');
      expect(service.getActiveRunId('ROBOT_1')).toBeUndefined();
    });

    it('deve buscar no banco se não houver corrida no mapa em memória', async () => {
      const result = await service.finishRun({
        robotId: 'ROBOT_DESCONHECIDO',
        goalReached: true,
      });

      expect(prisma.corrida.findFirst).toHaveBeenCalledWith({
        where: { status: 'EM_ANDAMENTO' },
        orderBy: { iniciadaEm: 'desc' },
      });
      expect(result).toBeDefined();
    });
  });

  describe('recordTelemetry', () => {
    const mockTelemetryPayload: TelemetryPayloadDto = {
      v: 1,
      type: 'telemetry',
      robot_id: 'ROBOT_1',
      seq: 10,
      t_ms: 1000,
      pos: { x: 2, y: 3 },
      heading: 'N',
      speed_mm_s: 350,
      odo_mm: 500,
      battery_pct: 95.5,
      state: 'EXPLORING',
      walls: { n: false, e: true, s: false, w: true },
    };

    it('deve gravar telemetria associada à corrida ativa', async () => {
      await service.startRun({ robotId: 'ROBOT_1' });

      const result = await service.recordTelemetry({
        robotId: 'ROBOT_1',
        payload: mockTelemetryPayload,
      });

      expect(prisma.telemetria.create).toHaveBeenCalledWith({
        data: expect.objectContaining({
          corridaId: 'run-1',
          posX: 2,
          posY: 3,
          direcao: 'N',
          sensores: expect.objectContaining({
            speed_mm_s: 350,
            battery_pct: 95.5,
            odo_mm: 500,
            seq: 10,
          }),
        }),
      });
      expect(result).toBeDefined();
    });

    it('deve criar automaticamente uma corrida se não houver nenhuma ativa', async () => {
      const result = await service.recordTelemetry({
        robotId: 'ROBOT_NOVO',
        payload: mockTelemetryPayload,
      });

      expect(prisma.corrida.create).toHaveBeenCalled();
      expect(prisma.telemetria.create).toHaveBeenCalled();
      expect(result).toBeDefined();
    });

    it('deve retornar null sem lançar exceção se a persistência falhar', async () => {
      await service.startRun({ robotId: 'ROBOT_1' });
      prisma.telemetria.create.mockRejectedValueOnce(new Error('DB Error'));

      const result = await service.recordTelemetry({
        robotId: 'ROBOT_1',
        payload: mockTelemetryPayload,
      });

      expect(result).toBeNull();
    });
  });

  describe('Event Listeners', () => {
    it('deve iniciar corrida ao receber RUN_START', async () => {
      const spy = jest.spyOn(service, 'startRun');
      const envelope = {
        data: {
          v: 1 as const,
          type: 'event' as const,
          robot_id: 'ROBOT_EV',
          seq: 1,
          t_ms: 0,
          event: 'RUN_START' as const,
        },
        recebido_em: new Date(),
      };

      await service.handleRunStarted(envelope);
      expect(spy).toHaveBeenCalledWith({
        robotId: 'ROBOT_EV',
        iniciadaEm: envelope.recebido_em,
      });
    });

    it('deve concluir corrida como bem-sucedida ao receber GOAL_REACHED', async () => {
      const spy = jest.spyOn(service, 'finishRun');
      const envelope = {
        data: {
          v: 1 as const,
          type: 'event' as const,
          robot_id: 'ROBOT_EV',
          seq: 99,
          t_ms: 12000,
          event: 'GOAL_REACHED' as const,
        },
        recebido_em: new Date(),
      };

      await service.handleGoalReached(envelope);
      expect(spy).toHaveBeenCalledWith({
        robotId: 'ROBOT_EV',
        goalReached: true,
        finalizadaEm: envelope.recebido_em,
      });
    });

    it('deve registrar telemetria ao receber TELEMETRY_RECEIVED', async () => {
      const spy = jest.spyOn(service, 'recordTelemetry');
      const envelope = {
        data: {
          v: 1 as const,
          type: 'telemetry' as const,
          robot_id: 'ROBOT_EV',
          seq: 12,
          t_ms: 1200,
          pos: { x: 1, y: 1 },
          heading: 'E' as const,
          speed_mm_s: 300,
          battery_pct: 90,
          state: 'EXPLORING' as const,
        },
        recebido_em: new Date(),
      };

      await service.handleTelemetryReceived(envelope);
      expect(spy).toHaveBeenCalledWith({
        robotId: 'ROBOT_EV',
        payload: envelope.data,
        recebidoEm: envelope.recebido_em,
      });
    });
  });
});
