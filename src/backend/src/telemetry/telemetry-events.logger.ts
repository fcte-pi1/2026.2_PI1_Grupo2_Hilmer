import { Injectable, Logger } from '@nestjs/common';
import { OnEvent } from '@nestjs/event-emitter';
import {
  CONNECTION_EVENTS,
  RateLowPayload,
  RobotDisconnectedPayload,
  RobotReconnectedPayload,
} from './telemetry-connection.events';
import {
  IngestedMessageEnvelope,
  TELEMETRY_EVENTS,
} from './services/telemetry-validation.service';

/**
 * LISTENER TEMPORÁRIO, só para conferir os eventos internos (critério de aceitação).
 * Ativado com TELEMETRY_DEBUG_EVENTS=true. Remover quando Realtime, Persistence e
 * Alerts estiverem consumindo os eventos de verdade.
 */
@Injectable()
export class TelemetryEventsLogger {
  private readonly logger = new Logger('TelemetryEvents');
  private received = 0;

  @OnEvent(TELEMETRY_EVENTS.TELEMETRY_RECEIVED)
  onReceived(e: IngestedMessageEnvelope): void {
    this.received++;
    // 10 Hz geraria 10 linhas por segundo; loga 1 a cada 10 pacotes.
    if (this.received % 10 === 0) {
      this.logger.log(`telemetry.received #${this.received} seq=${e.data.seq}`);
    }
  }

  @OnEvent(TELEMETRY_EVENTS.RUN_CALIBRATED)
  onCalibrated(e: IngestedMessageEnvelope): void {
    this.logger.log(`run.calibrated seq=${e.data.seq}`);
  }

  @OnEvent(TELEMETRY_EVENTS.RUN_STARTED)
  onStarted(e: IngestedMessageEnvelope): void {
    this.logger.log(`run.started seq=${e.data.seq}`);
  }

  @OnEvent(TELEMETRY_EVENTS.RUN_GOAL_REACHED)
  onGoal(e: IngestedMessageEnvelope): void {
    this.logger.log(`run.goal_reached t=${e.data.t_ms}ms`);
  }

  @OnEvent(TELEMETRY_EVENTS.RUN_FINISHED)
  onFinished(e: IngestedMessageEnvelope): void {
    this.logger.log(`run.finished seq=${e.data.seq}`);
  }

  @OnEvent(CONNECTION_EVENTS.ROBOT_DISCONNECTED)
  onDisconnected(p: RobotDisconnectedPayload): void {
    this.logger.warn(`robot.disconnected ${p.robotId}`);
  }

  @OnEvent(CONNECTION_EVENTS.ROBOT_RECONNECTED)
  onReconnected(p: RobotReconnectedPayload): void {
    this.logger.warn(`robot.reconnected ${p.robotId} após ${p.downtimeMs} ms`);
  }

  @OnEvent(CONNECTION_EVENTS.RATE_LOW)
  onRateLow(p: RateLowPayload): void {
    this.logger.warn(`telemetry.rate_low ${p.hz.toFixed(1)} Hz`);
  }
}
