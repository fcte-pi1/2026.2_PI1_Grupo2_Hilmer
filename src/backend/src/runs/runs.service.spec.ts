import { Test, TestingModule } from '@nestjs/testing';
import { PrismaService } from '../prisma/prisma.service';
import { TelemetryPayloadDto } from '../telemetry/dto/telemetry.dto';
import { RunsService } from './runs.service';

describe('RunsService', () => {
  let service: RunsService;
  let prisma: {
    labirinto: {
      upsert: jest.Mock;
      findMany: jest.Mock;
    };
    corrida: {
      create: jest.Mock;
      update: jest.Mock;
      updateMany: jest.Mock;
      findFirst: jest.Mock;
      findMany: jest.Mock;
      findUnique: jest.Mock;
      count: jest.Mock;
    };
    telemetria: {
      create: jest.Mock;
      findMany: jest.Mock;
    };
  };

  const mockLabirinto = { id: 'lab-1', dimensao: '4x4' };
  const mockCorrida = {
    id: 'run-1',
    labirintoId: 'lab-1',
    status: 'EM_ANDAMENTO',
    iniciadaEm: new Date('2026-10-17T12:00:00Z'),
    finalizadaEm: null,
    labirinto: mockLabirinto,
    _count: { telemetrias: 15 },
  };

  beforeEach(async () => {
    prisma = {
      labirinto: {
        upsert: jest.fn().mockResolvedValue(mockLabirinto),
        findMany: jest.fn().mockResolvedValue([mockLabirinto]),
      },
      corrida: {
        create: jest.fn().mockResolvedValue(mockCorrida),
        update: jest.fn().mockImplementation(({ where, data }) => ({
          ...mockCorrida,
          id: where.id,
          status: data.status,
          finalizadaEm: data.finalizadaEm,
        })),
        updateMany: jest.fn().mockResolvedValue({ count: 1 }),
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
        findMany: jest
          .fn()
          .mockResolvedValue([
            { id: 'telem-1', corridaId: 'run-1', timestamp: new Date() },
          ]),
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

  describe('onModuleInit', () => {
    // onModuleInit não faz nada sem DATABASE_URL; o teste não pode depender do
    // ambiente de quem roda a suíte.
    const databaseUrlOriginal = process.env.DATABASE_URL;
    beforeEach(() => {
      process.env.DATABASE_URL = 'postgresql://teste';
    });
    afterEach(() => {
      if (databaseUrlOriginal === undefined) delete process.env.DATABASE_URL;
      else process.env.DATABASE_URL = databaseUrlOriginal;
    });

    it('deve marcar corridas órfãs em EM_ANDAMENTO como ABORTADA', async () => {
      await service.onModuleInit();
      expect(prisma.corrida.updateMany).toHaveBeenCalledWith({
        where: { status: 'EM_ANDAMENTO' },
        data: expect.objectContaining({ status: 'ABORTADA' }),
      });
    });

    it('deve tratar erro na inicialização sem propagar exceção', async () => {
      prisma.corrida.updateMany.mockRejectedValueOnce(
        new Error('DB Init Error'),
      );
      await expect(service.onModuleInit()).resolves.toBeUndefined();
    });
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

    it('deve aceitar dimensão personalizada como 8x4 e 12x4', async () => {
      await service.startRun({
        robotId: 'ROBOT_8x4',
        dimensaoLabirinto: '8x4',
      });

      expect(prisma.labirinto.upsert).toHaveBeenCalledWith({
        where: { dimensao: '8x4' },
        update: {},
        create: { dimensao: '8x4' },
      });
    });

    it('deve finalizar corrida anterior se o robô já possuir corrida aberta', async () => {
      await service.startRun({ robotId: 'ROBOT_1' });
      prisma.corrida.create.mockResolvedValueOnce({
        ...mockCorrida,
        id: 'run-2',
      });

      await service.startRun({ robotId: 'ROBOT_1' });

      expect(prisma.corrida.updateMany).toHaveBeenCalledWith(
        expect.objectContaining({
          where: { id: 'run-1', status: 'EM_ANDAMENTO' },
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

    it('deve marcar a corrida como ABORTADA quando goalReached for false de forma atômica', async () => {
      await service.startRun({ robotId: 'ROBOT_1' });
      prisma.corrida.findUnique.mockResolvedValueOnce({
        ...mockCorrida,
        status: 'ABORTADA',
      });

      const result = await service.finishRun({
        robotId: 'ROBOT_1',
        goalReached: false,
      });

      expect(prisma.corrida.updateMany).toHaveBeenCalledWith({
        where: { id: 'run-1', status: 'EM_ANDAMENTO' },
        data: expect.objectContaining({
          status: 'ABORTADA',
        }),
      });
      expect(result?.status).toBe('ABORTADA');
      expect(service.getActiveRunId('ROBOT_1')).toBeUndefined();
    });

    it('deve retornar null se não houver corrida ativa para o robô', async () => {
      const result = await service.finishRun({
        robotId: 'ROBOT_SEM_CORRIDA',
        goalReached: true,
      });

      expect(result).toBeNull();
    });

    it('não deve sobrescrever corrida FINALIZADA por ABORTADA via cache em memória', async () => {
      await service.startRun({ robotId: 'ROBOT_1' });
      await service.finishRun({
        robotId: 'ROBOT_1',
        goalReached: true,
      });

      // Segunda chamada com false para o mesmo ID
      const secondCall = await service.finishRun({
        corridaId: 'run-1',
        goalReached: false,
      });
      expect(secondCall).toBeNull();
    });

    it('deve retornar null se updateMany retornar count 0 para ABORTADA (já era FINALIZADA no banco)', async () => {
      prisma.corrida.updateMany.mockResolvedValueOnce({ count: 0 });

      const result = await service.finishRun({
        corridaId: 'run-outra',
        goalReached: false,
      });
      expect(result).toBeNull();
    });

    it('deve tratar erro do banco sem estourar exceção', async () => {
      prisma.corrida.update.mockRejectedValueOnce(new Error('DB Finish Error'));
      const result = await service.finishRun({
        corridaId: 'run-fail',
        goalReached: true,
      });
      expect(result).toBeNull();
    });
  });

  describe('encerramento por eventos', () => {
    const evento = (event: 'GOAL_REACHED' | 'RUN_END') => ({
      data: {
        v: 1 as const,
        type: 'event' as const,
        robot_id: 'ROBOT_1',
        seq: 1,
        t_ms: 0,
        event,
      },
      recebido_em: new Date(),
    });

    it('não deve sobrescrever FINALIZADA quando RUN_END chega logo após GOAL_REACHED', async () => {
      await service.startRun({ robotId: 'ROBOT_1' });

      // Como no EventEmitter: os handlers não são aguardados um pelo outro.
      await Promise.all([
        service.handleGoalReached(evento('GOAL_REACHED')),
        service.handleRunFinished(evento('GOAL_REACHED')),
        service.handleRunFinished(evento('RUN_END')),
      ]);

      expect(prisma.corrida.update).toHaveBeenCalledTimes(1);
      expect(prisma.corrida.update).toHaveBeenCalledWith({
        where: { id: 'run-1' },
        data: expect.objectContaining({ status: 'FINALIZADA' }),
      });
      expect(prisma.corrida.updateMany).not.toHaveBeenCalled();
      expect(prisma.corrida.findFirst).not.toHaveBeenCalled();
    });

    it('deve abortar a corrida ativa quando RUN_END chega sem GOAL_REACHED', async () => {
      await service.startRun({ robotId: 'ROBOT_1' });

      await service.handleRunFinished(evento('RUN_END'));

      expect(prisma.corrida.updateMany).toHaveBeenCalledWith({
        where: { id: 'run-1', status: 'EM_ANDAMENTO' },
        data: expect.objectContaining({ status: 'ABORTADA' }),
      });
    });

    it('RUN_END sem corrida ativa não deve abortar outra corrida do banco', async () => {
      await service.handleRunFinished(evento('RUN_END'));

      expect(prisma.corrida.findFirst).not.toHaveBeenCalled();
      expect(prisma.corrida.update).not.toHaveBeenCalled();
      expect(prisma.corrida.updateMany).not.toHaveBeenCalled();
    });
  });

  describe('findAll: métricas das telemetrias', () => {
    const corridaComTelemetrias = {
      ...mockCorrida,
      status: 'FINALIZADA',
      finalizadaEm: new Date('2026-10-17T12:00:30Z'),
      labirinto: { id: 'lab-2', dimensao: '8x4' },
      labirintoId: 'lab-2',
      telemetrias: [
        { sensores: { speed_mm_s: 200, battery_pct: 98 } },
        { sensores: { speed_mm_s: 400, battery_pct: 96 } },
        { sensores: { speed_mm_s: 300, battery_pct: 95 } },
      ],
    };

    it('deve calcular velocidade média e bateria a partir das telemetrias', async () => {
      prisma.corrida.findMany.mockResolvedValue([corridaComTelemetrias]);

      const [run] = await service.findAll();

      expect(run).toEqual({
        id: 'run-1',
        labirintoId: 'lab-2',
        labirinto: '8x4',
        status: 'FINALIZADA',
        iniciadaEm: corridaComTelemetrias.iniciadaEm,
        finalizadaEm: corridaComTelemetrias.finalizadaEm,
        duracaoSegundos: 30,
        velocidadeMedia: 0.3,
        bateriaInicial: 98,
        bateriaFinal: 95,
      });
    });

    it('deve devolver métricas vazias para corrida sem telemetria', async () => {
      prisma.corrida.findMany.mockResolvedValue([
        { ...corridaComTelemetrias, telemetrias: [] },
      ]);

      const [run] = await service.findAll();

      expect(run).toMatchObject({
        velocidadeMedia: 0,
        bateriaInicial: null,
        bateriaFinal: null,
      });
    });

    it('deve buscar só os sensores das telemetrias, em ordem cronológica', async () => {
      prisma.corrida.findMany.mockResolvedValue([]);

      await service.findAll();

      expect(prisma.corrida.findMany).toHaveBeenCalledWith(
        expect.objectContaining({
          include: expect.objectContaining({
            telemetrias: {
              select: { sensores: true },
              orderBy: { timestamp: 'asc' },
            },
          }),
        }),
      );
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

    it('deve retornar null se auto-criação da corrida falhar', async () => {
      prisma.corrida.create.mockRejectedValueOnce(new Error('DB Fail'));

      const result = await service.recordTelemetry({
        robotId: 'ROBOT_NOVO_FAIL',
        payload: mockTelemetryPayload,
      });

      expect(result).toBeNull();
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

  describe('Consultas REST e Histórico', () => {
    it('listRuns deve paginar resultados', async () => {
      const res = await service.listRuns(2, 10);
      expect(prisma.corrida.findMany).toHaveBeenCalledWith(
        expect.objectContaining({ skip: 10, take: 10 }),
      );
      expect(res.total).toBe(1);
      expect(res.runs).toHaveLength(1);
    });

    it('getRunById deve buscar corrida completa com telemetrias ordenadas', async () => {
      const res = await service.getRunById('run-1');
      expect(prisma.corrida.findUnique).toHaveBeenCalledWith({
        where: { id: 'run-1' },
        include: {
          labirinto: true,
          telemetrias: { orderBy: { timestamp: 'asc' } },
        },
      });
      expect(res?.id).toBe('run-1');
    });

    it('findAll sem filtro deve listar todas as corridas mapeadas', async () => {
      const finishedRun = {
        ...mockCorrida,
        finalizadaEm: new Date('2026-10-17T12:00:15Z'),
      };
      prisma.corrida.findMany.mockResolvedValueOnce([finishedRun]);

      const res = await service.findAll();
      expect(prisma.corrida.findMany).toHaveBeenCalledWith(
        expect.objectContaining({ where: {} }),
      );
      expect(res[0].duracaoSegundos).toBe(15);
      expect(res[0].labirinto).toBe('4x4');
    });

    it('findAll com filtro mazeId deve filtrar por labirintoId ou dimensao', async () => {
      await service.findAll('8x4');
      expect(prisma.corrida.findMany).toHaveBeenCalledWith(
        expect.objectContaining({
          where: {
            OR: [{ labirintoId: '8x4' }, { labirinto: { dimensao: '8x4' } }],
          },
        }),
      );
    });

    it('findBestRuns deve calcular a melhor corrida por labirinto', async () => {
      const labirinto8x4 = { id: 'lab-2', dimensao: '8x4' };
      prisma.labirinto.findMany.mockResolvedValueOnce([
        mockLabirinto,
        labirinto8x4,
      ]);

      // Corridas do lab-1 (4x4): run A (20s) e run B (10s)
      const runA = {
        id: 'run-A',
        labirintoId: 'lab-1',
        status: 'FINALIZADA',
        iniciadaEm: new Date('2026-10-17T12:00:00Z'),
        finalizadaEm: new Date('2026-10-17T12:00:20Z'),
      };
      const runB = {
        id: 'run-B',
        labirintoId: 'lab-1',
        status: 'FINALIZADA',
        iniciadaEm: new Date('2026-10-17T12:00:00Z'),
        finalizadaEm: new Date('2026-10-17T12:00:10Z'),
      };

      // Labirinto 8x4 sem corridas concluídas
      prisma.corrida.findMany
        .mockResolvedValueOnce([runA, runB])
        .mockResolvedValueOnce([]);

      const best = await service.findBestRuns();
      expect(best).toHaveLength(1);
      expect(best[0].corridaId).toBe('run-B');
      expect(best[0].tempoSegundos).toBe(10);
      expect(best[0].labirinto).toBe('4x4');
    });

    it('findById deve retornar corrida com metricas calculadas', async () => {
      const runWithTime = {
        ...mockCorrida,
        finalizadaEm: new Date('2026-10-17T12:00:25.500Z'),
      };
      prisma.corrida.findUnique.mockResolvedValueOnce(runWithTime);

      const res = await service.findById('run-1');
      expect(res).toBeDefined();
      expect(res?.duracaoSegundos).toBe(25.5);
      expect(res?.totalAmostras).toBe(15);
    });

    it('findById deve retornar null se a corrida nao existir', async () => {
      prisma.corrida.findUnique.mockResolvedValueOnce(null);
      const res = await service.findById('inexistente');
      expect(res).toBeNull();
    });

    it('findSamplesByRunId deve retornar telemetrias se corrida existir', async () => {
      const samples = await service.findSamplesByRunId('run-1');
      expect(prisma.telemetria.findMany).toHaveBeenCalledWith({
        where: { corridaId: 'run-1' },
        orderBy: { timestamp: 'asc' },
      });
      expect(samples).toHaveLength(1);
    });

    it('findSamplesByRunId deve retornar null se corrida nao existir', async () => {
      prisma.corrida.findUnique.mockResolvedValueOnce(null);
      const samples = await service.findSamplesByRunId('inexistente');
      expect(samples).toBeNull();
    });
  });

  describe('Event Listeners', () => {
    it('deve iniciar corrida ao receber RUN_START com dimensão de labirinto', async () => {
      const spy = jest.spyOn(service, 'startRun');
      const envelope = {
        data: {
          v: 1 as const,
          type: 'event' as const,
          robot_id: 'ROBOT_EV',
          seq: 1,
          t_ms: 0,
          event: 'RUN_START' as const,
          maze: '8x4' as const,
        },
        recebido_em: new Date(),
      };

      await service.handleRunStarted(envelope);
      expect(spy).toHaveBeenCalledWith({
        robotId: 'ROBOT_EV',
        dimensaoLabirinto: '8x4',
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

    it('deve ignorar handleRunFinished quando evento for GOAL_REACHED para evitar corrida dupla', async () => {
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

      await service.handleRunFinished(envelope);
      expect(spy).not.toHaveBeenCalled();
    });

    it('deve finalizar corrida como ABORTADA ao receber RUN_END avulso', async () => {
      const spy = jest.spyOn(service, 'finishRun');
      const envelope = {
        data: {
          v: 1 as const,
          type: 'event' as const,
          robot_id: 'ROBOT_EV',
          seq: 150,
          t_ms: 25000,
          event: 'RUN_END' as const,
        },
        recebido_em: new Date(),
      };

      await service.handleRunFinished(envelope);
      expect(spy).toHaveBeenCalledWith({
        robotId: 'ROBOT_EV',
        goalReached: false,
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
