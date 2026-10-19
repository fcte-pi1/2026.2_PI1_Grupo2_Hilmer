import { Injectable, Logger } from '@nestjs/common';
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
export class RunsService {
  private readonly logger = new Logger(RunsService.name);

  /**
   * Mapeamento em memória da corrida atualmente ativa por robotId (ou 'default').
   * Permite associação em tempo real O(1) de pacotes de telemetria sem queries extras.
   */
  private readonly activeRuns = new Map<string, string>();

  constructor(private readonly prisma: PrismaService) {}

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
    let corridaId = options.corridaId ?? this.activeRuns.get(robotKey);

    try {
      if (!corridaId) {
        // Tenta buscar a última corrida 'EM_ANDAMENTO' no banco
        const lastOpen = await this.prisma.corrida.findFirst({
          where: { status: 'EM_ANDAMENTO' },
          orderBy: { iniciadaEm: 'desc' },
        });
        if (!lastOpen) {
          this.logger.debug(
            `[RunsService] Nenhuma corrida ativa encontrada para finalizar (robô: ${robotKey}).`,
          );
          return null;
        }
        corridaId = lastOpen.id;
      }

      const status = options.goalReached ? 'FINALIZADA' : 'ABORTADA';
      const finalizadaEm = options.finalizadaEm ?? new Date();

      const corrida = await this.prisma.corrida.update({
        where: { id: corridaId },
        data: {
          status,
          finalizadaEm,
        },
      });

      this.activeRuns.delete(robotKey);
      this.logger.log(
        `[RunsService] Corrida encerrada: ${corrida.id} (Status: ${status})`,
      );
      return corrida;
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
  // EVENT LISTENERS (@nestjs/event-emitter)
  // ==========================================

  @OnEvent(TELEMETRY_EVENTS.RUN_STARTED)
  async handleRunStarted(
    envelope: IngestedMessageEnvelope<EventPayloadDto>,
  ): Promise<void> {
    await this.startRun({
      robotId: envelope.data.robot_id,
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
    // Se o evento foi GOAL_REACHED, goalReached é true; caso contrário (RUN_END avulso), encerra normalmente
    const isGoal = envelope.data.event === 'GOAL_REACHED';
    await this.finishRun({
      robotId: envelope.data.robot_id,
      goalReached: isGoal,
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
