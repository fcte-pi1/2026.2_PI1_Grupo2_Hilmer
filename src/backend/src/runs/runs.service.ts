import { Injectable, Logger, OnModuleInit } from '@nestjs/common';
import { OnEvent } from '@nestjs/event-emitter';
import { Corrida, Telemetria } from '@prisma/client';
import { PrismaService } from '../prisma/prisma.service';
import {
  EventPayloadDto,
  TelemetryPayloadDto,
} from '../telemetry/dto/telemetry.dto';
import {
  IngestedMessageEnvelope,
  TELEMETRY_EVENTS,
} from '../telemetry/services/telemetry-validation.service';

export interface StartRunOptions {
  robotId?: string;
  dimensaoLabirinto?: string;
  iniciadaEm?: Date;
}

export interface FinishRunOptions {
  robotId?: string;
  corridaId?: string;
  goalReached?: boolean;
  finalizadaEm?: Date;
}

export interface RecordTelemetryOptions {
  robotId?: string;
  corridaId?: string;
  payload: TelemetryPayloadDto;
  recebidoEm?: Date;
}

@Injectable()
export class RunsService implements OnModuleInit {
  private readonly logger = new Logger(RunsService.name);

  /**
   * Mapeamento em memória da corrida atualmente ativa por robotId (ou 'default').
   * Permite associação em tempo real O(1) de pacotes de telemetria sem queries extras.
   */
  private readonly activeRuns = new Map<string, string>();

  /**
   * Cache em memória de corridas finalizadas com sucesso (FINALIZADA) na sessão.
   * Evita race condition onde RUN_END concorrente sobrescreve status para ABORTADA.
   */
  private readonly finalizedRuns = new Set<string>();

  constructor(private readonly prisma: PrismaService) {}

  /**
   * Limpa corridas órfãs deixadas em 'EM_ANDAMENTO' no banco (ex: reinicialização abrupta).
   */
  async onModuleInit(): Promise<void> {
    if (!process.env.DATABASE_URL) return;
    try {
      const result = await this.prisma.corrida.updateMany({
        where: { status: 'EM_ANDAMENTO' },
        data: {
          status: 'ABORTADA',
          finalizadaEm: new Date(),
        },
      });
      if (result.count > 0) {
        this.logger.warn(
          `[RunsService] Inicialização: ${result.count} corrida(s) órfã(s) em 'EM_ANDAMENTO' foram marcadas como 'ABORTADA'.`,
        );
      }
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      this.logger.error(
        `[RunsService] Falha ao encerrar corridas órfãs na inicialização: ${msg}`,
      );
    }
  }

  /**
   * Inicia uma nova corrida para o robô.
   * Se já houver uma corrida ativa em andamento para o robô, encerra-a antes de iniciar a nova.
   */
  async startRun(options: StartRunOptions = {}): Promise<Corrida | null> {
    const robotKey = options.robotId ?? 'default';
    const dimensao = options.dimensaoLabirinto ?? '4x4';
    const iniciadaEm = options.iniciadaEm ?? new Date();

    try {
      // 1. Encerra qualquer corrida anterior que ainda esteja aberta para este robô
      if (this.activeRuns.has(robotKey)) {
        const previousId = this.activeRuns.get(robotKey)!;
        await this.finishRun({
          robotId: robotKey,
          corridaId: previousId,
          goalReached: false,
          finalizadaEm: iniciadaEm,
        });
      }

      // 2. Garante existência do labirinto no banco
      const labirinto = await this.prisma.labirinto.upsert({
        where: { dimensao },
        update: {},
        create: { dimensao },
      });

      // 3. Cria a nova corrida com status 'EM_ANDAMENTO'
      const corrida = await this.prisma.corrida.create({
        data: {
          labirintoId: labirinto.id,
          status: 'EM_ANDAMENTO',
          iniciadaEm,
        },
      });

      this.activeRuns.set(robotKey, corrida.id);
      this.logger.log(
        `[RunsService] Corrida iniciada: ${corrida.id} (Robô: ${robotKey}, Labirinto: ${dimensao})`,
      );
      return corrida;
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      this.logger.error(
        `[RunsService] Falha ao iniciar corrida no banco: ${msg}`,
      );
      return null;
    }
  }

  /**
   * Encerra a corrida ativa do robô ou pelo ID fornecido.
   */
  async finishRun(options: FinishRunOptions = {}): Promise<Corrida | null> {
    const robotKey = options.robotId ?? 'default';
    const corridaId = options.corridaId ?? this.activeRuns.get(robotKey);

    if (!corridaId) {
      this.logger.debug(
        `[RunsService] Nenhuma corrida ativa encontrada para finalizar (robô: ${robotKey}).`,
      );
      return null;
    }

    if (this.finalizedRuns.has(corridaId)) {
      this.logger.debug(
        `[RunsService] Corrida ${corridaId} já finalizada com sucesso (cache em memória).`,
      );
      return null;
    }

    if (this.activeRuns.get(robotKey) === corridaId) {
      this.activeRuns.delete(robotKey);
    }

    try {
      const finalizadaEm = options.finalizadaEm ?? new Date();

      if (options.goalReached) {
        this.finalizedRuns.add(corridaId);

        const corrida = await this.prisma.corrida.update({
          where: { id: corridaId },
          data: {
            status: 'FINALIZADA',
            finalizadaEm,
          },
        });

        this.logger.log(
          `[RunsService] Corrida encerrada com sucesso: ${corrida.id} (Status: FINALIZADA)`,
        );
        return corrida;
      } else {
        // Encerramento como ABORTADA: atualização atômica para garantir que se outra chamada
        // já definiu FINALIZADA, ela não seja sobrescrita no banco.
        const updateResult = await this.prisma.corrida.updateMany({
          where: {
            id: corridaId,
            status: 'EM_ANDAMENTO',
          },
          data: {
            status: 'ABORTADA',
            finalizadaEm,
          },
        });

        if (updateResult.count === 0) {
          this.logger.debug(
            `[RunsService] Corrida ${corridaId} não estava EM_ANDAMENTO (possivelmente já FINALIZADA). Não alterado.`,
          );
          return null;
        }

        const corrida = await this.prisma.corrida.findUnique({
          where: { id: corridaId },
        });

        this.logger.log(
          `[RunsService] Corrida encerrada: ${corridaId} (Status: ABORTADA)`,
        );
        return corrida;
      }
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      this.logger.error(
        `[RunsService] Falha ao finalizar corrida ${corridaId}: ${msg}`,
      );
      return null;
    }
  }

  /**
   * Registra um ponto de telemetria associado à corrida ativa.
   * Se nenhuma corrida estiver ativa, cria automaticamente uma corrida padrão para não descartar dados.
   */
  async recordTelemetry(
    options: RecordTelemetryOptions,
  ): Promise<Telemetria | null> {
    const robotKey = options.robotId ?? options.payload.robot_id ?? 'default';
    let corridaId = options.corridaId ?? this.activeRuns.get(robotKey);

    try {
      if (!corridaId) {
        this.logger.warn(
          `[RunsService] Telemetria recebida sem corrida ativa para ${robotKey}. Iniciando corrida automática.`,
        );
        const autoRun = await this.startRun({
          robotId: robotKey,
          iniciadaEm: options.recebidoEm,
        });
        if (!autoRun) return null;
        corridaId = autoRun.id;
      }

      const { payload } = options;
      const timestamp = options.recebidoEm ?? new Date();

      const telemetria = await this.prisma.telemetria.create({
        data: {
          corridaId,
          timestamp,
          posX: payload.pos.x,
          posY: payload.pos.y,
          direcao: payload.heading,
          sensores: {
            speed_mm_s: payload.speed_mm_s,
            battery_pct: payload.battery_pct,
            odo_mm: payload.odo_mm ?? 0,
            state: payload.state,
            walls: payload.walls ? { ...payload.walls } : null,
            seq: payload.seq,
            t_ms: payload.t_ms,
            sensores:
              typeof payload.sensores === 'object' && payload.sensores !== null
                ? payload.sensores
                : null,
          },
        },
      });

      return telemetria;
    } catch (err: unknown) {
      const msg = err instanceof Error ? err.message : String(err);
      this.logger.error(
        `[RunsService] Falha ao gravar telemetria no banco: ${msg}`,
      );
      return null;
    }
  }

  /**
   * Retorna a corrida ativa atual em memória para um robô.
   */
  getActiveRunId(robotId: string = 'default'): string | undefined {
    return this.activeRuns.get(robotId);
  }

  /**
   * Consulta corridas com suporte a paginação.
   */
  async listRuns(
    page = 1,
    limit = 20,
  ): Promise<{ runs: Corrida[]; total: number }> {
    const skip = (page - 1) * limit;
    const [runs, total] = await Promise.all([
      this.prisma.corrida.findMany({
        skip,
        take: limit,
        orderBy: { iniciadaEm: 'desc' },
        include: { labirinto: true, _count: { select: { telemetrias: true } } },
      }),
      this.prisma.corrida.count(),
    ]);

    return { runs, total };
  }

  /**
   * Busca detalhes completos de uma corrida por ID.
   */
  async getRunById(id: string): Promise<Corrida | null> {
    return this.prisma.corrida.findUnique({
      where: { id },
      include: {
        labirinto: true,
        telemetrias: {
          orderBy: { timestamp: 'asc' },
        },
      },
    });
  }

  // ==========================================
  // CONSULTAS REST / HISTÓRICO E RECORDES
  // ==========================================

  /**
   * Lista todas as corridas com suporte a filtro por labirinto (ID ou dimensão).
   */
  async findAll(mazeId?: string) {
    const where = mazeId
      ? {
          OR: [
            { labirintoId: mazeId },
            { labirinto: { dimensao: mazeId } },
          ],
        }
      : {};

    const runs = await this.prisma.corrida.findMany({
      where,
      include: {
        labirinto: true,
        _count: { select: { telemetrias: true } },
      },
      orderBy: { iniciadaEm: 'desc' },
    });

    return runs.map((run) => this.mapRunResponse(run));
  }

  /**
   * Retorna a melhor corrida (menor tempo) para cada labirinto cadastrado.
   */
  async findBestRuns() {
    const labirintos = await this.prisma.labirinto.findMany();
    const bestRuns = [];

    for (const lab of labirintos) {
      // Aceita corridas com status de sucesso ('CONCLUIDA' ou 'FINALIZADA')
      const runs = await this.prisma.corrida.findMany({
        where: {
          labirintoId: lab.id,
          status: { in: ['CONCLUIDA', 'FINALIZADA'] },
          finalizadaEm: { not: null },
        },
      });

      if (runs.length === 0) continue;

      const runsComDuracao = runs.map((r) => {
        const duracaoSegundos =
          (new Date(r.finalizadaEm!).getTime() - new Date(r.iniciadaEm).getTime()) / 1000;
        return {
          ...r,
          labirinto: lab,
          duracaoSegundos: Number(duracaoSegundos.toFixed(2)),
        };
      });

      runsComDuracao.sort((a, b) => a.duracaoSegundos - b.duracaoSegundos);

      const best = runsComDuracao[0];
      bestRuns.push({
        labirinto: lab.dimensao,
        labirintoId: lab.id,
        corridaId: best.id,
        status: best.status,
        tempoSegundos: best.duracaoSegundos,
        iniciadaEm: best.iniciadaEm,
        finalizadaEm: best.finalizadaEm,
      });
    }

    return bestRuns;
  }

  /**
   * Busca detalhes e métricas de uma corrida específica por ID.
   */
  async findById(id: string) {
    const run = await this.prisma.corrida.findUnique({
      where: { id },
      include: {
        labirinto: true,
        _count: { select: { telemetrias: true } },
      },
    });

    if (!run) return null;

    const duracaoSegundos = run.finalizadaEm
      ? Number(
          (
            (new Date(run.finalizadaEm).getTime() - new Date(run.iniciadaEm).getTime()) /
            1000
          ).toFixed(2),
        )
      : null;

    return {
      ...this.mapRunResponse(run),
      duracaoSegundos,
      totalAmostras: run._count.telemetrias,
    };
  }

  /**
   * Retorna as amostras de telemetria de uma corrida ordenadas no tempo.
   */
  async findSamplesByRunId(corridaId: string) {
    const exists = await this.prisma.corrida.findUnique({
      where: { id: corridaId },
      select: { id: true },
    });

    if (!exists) return null;

    return this.prisma.telemetria.findMany({
      where: { corridaId },
      orderBy: { timestamp: 'asc' },
    });
  }

  private mapRunResponse(run: any) {
    const duracaoSegundos = run.finalizadaEm
      ? Number(
          (
            (new Date(run.finalizadaEm).getTime() - new Date(run.iniciadaEm).getTime()) /
            1000
          ).toFixed(2),
        )
      : null;

    return {
      id: run.id,
      labirintoId: run.labirintoId,
      labirinto: run.labirinto?.dimensao,
      status: run.status,
      iniciadaEm: run.iniciadaEm,
      finalizadaEm: run.finalizadaEm,
      duracaoSegundos,
    };
  }

  // ==========================================
  // EVENT LISTENERS (@nestjs/event-emitter)
  // ==========================================

  @OnEvent(TELEMETRY_EVENTS.RUN_STARTED)
  async handleRunStarted(
    envelope: IngestedMessageEnvelope<EventPayloadDto>,
  ): Promise<void> {
    await this.startRun({
      robotId: envelope.data.robot_id,
      dimensaoLabirinto: envelope.data.maze,
      iniciadaEm: envelope.recebido_em,
    });
  }

  @OnEvent(TELEMETRY_EVENTS.RUN_GOAL_REACHED)
  async handleGoalReached(
    envelope: IngestedMessageEnvelope<EventPayloadDto>,
  ): Promise<void> {
    await this.finishRun({
      robotId: envelope.data.robot_id,
      goalReached: true,
      finalizadaEm: envelope.recebido_em,
    });
  }

  @OnEvent(TELEMETRY_EVENTS.RUN_FINISHED)
  async handleRunFinished(
    envelope: IngestedMessageEnvelope<EventPayloadDto>,
  ): Promise<void> {
    // Se o evento foi GOAL_REACHED, ele já foi tratado por handleGoalReached (@OnEvent RUN_GOAL_REACHED)
    if (envelope.data.event === 'GOAL_REACHED') {
      return;
    }
    await this.finishRun({
      robotId: envelope.data.robot_id,
      goalReached: false,
      finalizadaEm: envelope.recebido_em,
    });
  }

  @OnEvent(TELEMETRY_EVENTS.TELEMETRY_RECEIVED)
  async handleTelemetryReceived(
    envelope: IngestedMessageEnvelope<TelemetryPayloadDto>,
  ): Promise<void> {
    await this.recordTelemetry({
      robotId: envelope.data.robot_id,
      payload: envelope.data,
      recebidoEm: envelope.recebido_em,
    });
  }
}
