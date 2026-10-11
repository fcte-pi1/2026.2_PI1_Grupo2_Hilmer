import { Injectable, Logger, OnModuleDestroy, OnModuleInit } from '@nestjs/common';
import { EventEmitter2, OnEvent } from '@nestjs/event-emitter';
import {
  CONNECTION_EVENTS,
  RateLowPayload,
  RobotDisconnectedPayload,
  RobotReconnectedPayload,
} from '../telemetry-connection.events';
import { IngestedMessageEnvelope, TELEMETRY_EVENTS } from './telemetry-validation.service';

/** Sem mensagens por mais que isso => robot.disconnected (RNF18). */
export const DISCONNECT_AFTER_MS = 1000;
/** Resolução do watchdog: a queda é detectada entre 1,0 s e 1,1 s. */
export const TICK_MS = 100;
/** Janela de medição da taxa (RNF15). */
export const RATE_WINDOW_MS = 1000;
export const MIN_RATE_HZ = 10;
/**
 * Tolerância de medição: um emissor a "10 Hz" real mede 9,7–10,3 Hz por causa de
 * jitter e deriva do sleep, então só consideramos baixa a taxa < 9,5 Hz.
 */
export const RATE_TOLERANCE_HZ = 0.5;
/** Taxa baixa por MAIS que isso => telemetry.rate_low. */
export const RATE_LOW_AFTER_MS = 2000;

type LinkState = 'online' | 'offline' | 'finished';

interface RobotLink {
  state: LinkState;
  lastSeenAt: number;
  windowStart: number;
  windowCount: number;
  hz: number;
  lowSince: number | null;
  rateLowEmitted: boolean;
}

/**
 * Watchdog de conexão e medição de taxa. Só observa e emite eventos:
 * nunca escreve nada no robô (RNF13).
 */
@Injectable()
export class ConnectionMonitorService implements OnModuleInit, OnModuleDestroy {
  private readonly logger = new Logger(ConnectionMonitorService.name);
  private readonly links = new Map<string, RobotLink>();
  private timer?: ReturnType<typeof setInterval>;

  constructor(private readonly events: EventEmitter2) {}

  onModuleInit(): void {
    this.timer = setInterval(() => this.tick(), TICK_MS);
    this.timer.unref?.();
  }

  onModuleDestroy(): void {
    if (this.timer) clearInterval(this.timer);
  }

  /** Cada pacote de telemetria válido (e não duplicado) prova que o robô está vivo e conta para o Hz. */
  @OnEvent(TELEMETRY_EVENTS.TELEMETRY_RECEIVED)
  onTelemetry(envelope: IngestedMessageEnvelope): void {
    this.recordMessage(envelope.data.robot_id, true);
  }

  /**
   * O fim da corrida (RUN_END) encerra a transmissão de propósito: o silêncio depois
   * dele não é queda. O TelemetryValidationService também emite run.finished no
   * GOAL_REACHED, mas ali o robô ainda volta à origem, então só RUN_END conta.
   */
  @OnEvent(TELEMETRY_EVENTS.RUN_FINISHED)
  onRunFinished(envelope: IngestedMessageEnvelope): void {
    if ((envelope.data as unknown as { event?: string }).event === 'RUN_END') {
      this.markFinished(envelope.data.robot_id);
    }
  }

  /**
   * Registra que chegou dado do robô (chamado pelos listeners acima; público para os testes).
   * @param countsForRate true só para pacotes de telemetria (eventos não entram no Hz)
   */
  recordMessage(robotId: string, countsForRate: boolean): void {
    const now = Date.now();
    let link = this.links.get(robotId);

    if (!link) {
      link = this.newLink(now);
      this.links.set(robotId, link);
      this.logger.log(`Robô ${robotId}: primeira mensagem recebida`);
    } else if (link.state !== 'online') {
      const wasOffline = link.state === 'offline';
      const downtimeMs = now - link.lastSeenAt;
      Object.assign(link, this.newLink(now));
      if (wasOffline) {
        this.logger.warn(`Robô ${robotId}: dados voltaram após ${downtimeMs} ms`);
        const payload: RobotReconnectedPayload = {
          robotId,
          downtimeMs,
          at: new Date(now),
        };
        this.events.emit(CONNECTION_EVENTS.ROBOT_RECONNECTED, payload);
      }
    }

    link.lastSeenAt = now;
    if (countsForRate) link.windowCount++;
  }

  /**
   * RUN_END: o robô para de transmitir de propósito, então o silêncio
   * depois disso não é queda de conexão.
   */
  markFinished(robotId: string): void {
    const link = this.links.get(robotId);
    if (link) link.state = 'finished';
  }

  /** Última taxa medida (janela de 1 s já fechada), em Hz. */
  getRate(robotId: string): number | undefined {
    return this.links.get(robotId)?.hz;
  }

  getState(robotId: string): LinkState | undefined {
    return this.links.get(robotId)?.state;
  }

  /** Público apenas para facilitar testes; em produção roda pelo setInterval. */
  tick(now: number = Date.now()): void {
    for (const [robotId, link] of this.links) {
      if (link.state !== 'online') continue;

      if (now - link.lastSeenAt > DISCONNECT_AFTER_MS) {
        link.state = 'offline';
        this.logger.warn(
          `Robô ${robotId}: sem mensagens há ${now - link.lastSeenAt} ms (robot.disconnected)`,
        );
        const payload: RobotDisconnectedPayload = {
          robotId,
          lastSeenAt: new Date(link.lastSeenAt),
          at: new Date(now),
        };
        this.events.emit(CONNECTION_EVENTS.ROBOT_DISCONNECTED, payload);
        continue;
      }

      if (now - link.windowStart >= RATE_WINDOW_MS) {
        this.closeWindow(robotId, link, now);
      }
    }
  }

  private closeWindow(robotId: string, link: RobotLink, now: number): void {
    const elapsedMs = now - link.windowStart;
    link.hz = link.windowCount / (elapsedMs / 1000);
    this.logger.debug(`Robô ${robotId}: ${link.hz.toFixed(1)} Hz`);

    if (link.hz < MIN_RATE_HZ - RATE_TOLERANCE_HZ) {
      if (link.lowSince === null) link.lowSince = link.windowStart;
      const lowForMs = now - link.lowSince;
      if (!link.rateLowEmitted && lowForMs > RATE_LOW_AFTER_MS) {
        link.rateLowEmitted = true;
        this.logger.warn(
          `Robô ${robotId}: taxa de ${link.hz.toFixed(1)} Hz há ${lowForMs} ms (telemetry.rate_low)`,
        );
        const payload: RateLowPayload = {
          robotId,
          hz: link.hz,
          lowForMs,
          at: new Date(now),
        };
        this.events.emit(CONNECTION_EVENTS.RATE_LOW, payload);
      }
    } else {
      link.lowSince = null;
      link.rateLowEmitted = false;
    }

    link.windowStart = now;
    link.windowCount = 0;
  }

  private newLink(now: number): RobotLink {
    return {
      state: 'online',
      lastSeenAt: now,
      windowStart: now,
      windowCount: 0,
      hz: 0,
      lowSince: null,
      rateLowEmitted: false,
    };
  }
}
