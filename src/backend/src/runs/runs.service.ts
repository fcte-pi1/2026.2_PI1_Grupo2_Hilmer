import { Injectable, Logger, OnModuleInit } from '@nestjs/common';
import { OnEvent } from '@nestjs/event-emitter';
import { Corrida, Prisma, Telemetria } from '@prisma/client';
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

/** Status de corrida que contam como resultado válido (RF-15). */
const STATUS_SUCESSO = ['CONCLUIDA', 'FINALIZADA'];

interface ExtremosCorridaRow {
  corridaId: string;
  bateriaInicial: number | null;
  bateriaFinal: number | null;
  odoInicialMm: number | null;
  odoFinalMm: number | null;
  tInicialMs: number | null;
  tFinalMs: number | null;
}

interface MelhorCorridaRow {
  labirinto: string;
  labirintoCriadoEm: Date;
  labirintoId: string;
  corridaId: string;
  status: string;
  iniciadaEm: Date;
  finalizadaEm: Date;
}

function calcularDuracaoSegundos(iniciadaEm: Date, finalizadaEm: Date) {
  const ms = new Date(finalizadaEm).getTime() - new Date(iniciadaEm).getTime();
  return Number((ms / 1000).toFixed(2));
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
    let where: Prisma.CorridaWhereInput = {};

    if (mazeId) {
      // Resolve o labirinto antes: filtrar direto por labirinto_id deixa o
      // Postgres usar o índice (labirinto_id, iniciada_em DESC).
      const labirinto = await this.prisma.labirinto.findFirst({
        where: { OR: [{ id: mazeId }, { dimensao: mazeId }] },
        select: { id: true },
      });
      if (!labirinto) return [];
      where = { labirintoId: labirinto.id };
    }

    const runs = await this.prisma.corrida.findMany({
      where,
      include: { labirinto: true },
      orderBy: { iniciadaEm: 'desc' },
    });

    const metricas = await this.buscarMetricas(runs.map((run) => run.id));

    return runs.map((run) => ({
      ...this.mapRunResponse(run),
      ...this.calcularMetricas(metricas.get(run.id)),
    }));
  }

  /**
   * Busca no banco só a primeira e a última amostra de cada corrida (dois
   * index scans em (corrida_id, timestamp)), em vez de trazer todas as
   * telemetrias para a aplicação.
   */
  private async buscarMetricas(
    corridaIds: string[],
  ): Promise<Map<string, ExtremosCorridaRow>> {
    if (corridaIds.length === 0) return new Map();

    const linhas = await this.prisma.$queryRaw<ExtremosCorridaRow[]>`
      SELECT
        c.id AS "corridaId",
        (ini.sensores->>'battery_pct')::float8 AS "bateriaInicial",
        (fim.sensores->>'battery_pct')::float8 AS "bateriaFinal",
        (ini.sensores->>'odo_mm')::float8 AS "odoInicialMm",
        (fim.sensores->>'odo_mm')::float8 AS "odoFinalMm",
        (ini.sensores->>'t_ms')::float8 AS "tInicialMs",
        (fim.sensores->>'t_ms')::float8 AS "tFinalMs"
      FROM unnest(${corridaIds}::text[]) AS c(id)
      JOIN LATERAL (
        SELECT t.sensores
        FROM telemetrias t
        WHERE t.corrida_id = c.id
        ORDER BY t."timestamp" ASC
        LIMIT 1
      ) ini ON true
      JOIN LATERAL (
        SELECT t.sensores
        FROM telemetrias t
        WHERE t.corrida_id = c.id
        ORDER BY t."timestamp" DESC
        LIMIT 1
      ) fim ON true
    `;

    return new Map(linhas.map((linha) => [linha.corridaId, linha]));
  }

  /**
   * Velocidade média = distância percorrida (odo_mm) / tempo (t_ms) entre a
   * primeira e a última amostra; mm/ms já é m/s. Sem telemetria (ou sem
   * deslocamento no tempo), as métricas ficam vazias.
   */
  private calcularMetricas(extremos?: ExtremosCorridaRow) {
    if (!extremos) {
      return { velocidadeMedia: 0, bateriaInicial: null, bateriaFinal: null };
    }

    const { odoInicialMm, odoFinalMm, tInicialMs, tFinalMs } = extremos;
    const distanciaMm =
      odoInicialMm != null && odoFinalMm != null
        ? odoFinalMm - odoInicialMm
        : 0;
    const tempoMs =
      tInicialMs != null && tFinalMs != null ? tFinalMs - tInicialMs : 0;

    return {
      velocidadeMedia:
        distanciaMm > 0 && tempoMs > 0
          ? Number((distanciaMm / tempoMs).toFixed(2))
          : 0,
      bateriaInicial: extremos.bateriaInicial,
      bateriaFinal: extremos.bateriaFinal,
    };
  }

  /**
   * Retorna a melhor corrida (menor tempo) para cada labirinto cadastrado,
   * em uma única consulta (DISTINCT ON por labirinto).
   */
  async findBestRuns() {
    const melhores = await this.prisma.$queryRaw<MelhorCorridaRow[]>`
      SELECT *
      FROM (
        SELECT DISTINCT ON (c.labirinto_id)
          l.dimensao AS labirinto,
          l.created_at AS "labirintoCriadoEm",
          c.labirinto_id AS "labirintoId",
          c.id AS "corridaId",
          c.status,
          c.iniciada_em AS "iniciadaEm",
          c.finalizada_em AS "finalizadaEm"
        FROM corridas c
        JOIN labirintos l ON l.id = c.labirinto_id
        WHERE c.status IN (${Prisma.join(STATUS_SUCESSO)})
          AND c.finalizada_em IS NOT NULL
        ORDER BY c.labirinto_id, c.finalizada_em - c.iniciada_em, c.iniciada_em
      ) melhores
      ORDER BY "labirintoCriadoEm"
    `;

    return melhores.map((best) => ({
      labirinto: best.labirinto,
      labirintoId: best.labirintoId,
      corridaId: best.corridaId,
      status: best.status,
      tempoSegundos: calcularDuracaoSegundos(
        best.iniciadaEm,
        best.finalizadaEm,
      ),
      iniciadaEm: best.iniciadaEm,
      finalizadaEm: best.finalizadaEm,
    }));
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

    return {
      ...this.mapRunResponse(run),
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

  private mapRunResponse(run: Corrida & { labirinto?: { dimensao: string } }) {
    return {
      id: run.id,
      labirintoId: run.labirintoId,
      labirinto: run.labirinto?.dimensao,
      status: run.status,
      iniciadaEm: run.iniciadaEm,
      finalizadaEm: run.finalizadaEm,
      duracaoSegundos: run.finalizadaEm
        ? calcularDuracaoSegundos(run.iniciadaEm, run.finalizadaEm)
        : null,
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
