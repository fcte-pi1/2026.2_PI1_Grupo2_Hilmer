/**
 * Eventos de conexão/taxa emitidos pelo ConnectionMonitorService.
 * Os eventos de telemetria e de corrida (telemetry.received, run.*) são do
 * TelemetryValidationService e ficam em TELEMETRY_EVENTS.
 */
export const CONNECTION_EVENTS = {
  ROBOT_DISCONNECTED: 'robot.disconnected',
  ROBOT_RECONNECTED: 'robot.reconnected',
  RATE_LOW: 'telemetry.rate_low',
} as const;

export interface RobotDisconnectedPayload {
  robotId: string;
  lastSeenAt: Date;
  at: Date;
}

export interface RobotReconnectedPayload {
  robotId: string;
  downtimeMs: number;
  at: Date;
}

export interface RateLowPayload {
  robotId: string;
  /** Taxa medida na última janela de 1 s. */
  hz: number;
  /** Há quanto tempo a taxa está abaixo do limite. */
  lowForMs: number;
  at: Date;
}
