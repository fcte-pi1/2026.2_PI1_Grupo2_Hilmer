'use client';

import React, { createContext, useContext, useEffect, useState } from 'react';
import {
  CellWalls,
  HeadingDirection,
  MazeDimension,
  mockData,
} from '@/lib/mock-data';

export interface LiveTelemetryPayload {
  v: number;
  type: 'telemetry';
  robot_id: string;
  seq: number;
  t_ms: number;
  pos: { x: number; y: number };
  heading: HeadingDirection;
  speed_mm_s: number;
  odo_mm?: number;
  battery_pct: number;
  walls?: CellWalls;
  state: string;
}

/** Sem mensagem do robô por mais que isso, o cabeçalho mostra "Desconectado" (mesmo limite do watchdog do backend). */
export const ROBOT_TIMEOUT_MS = 1000;
/** Espera entre tentativas de reconectar ao /live. */
export const RECONNECT_DELAY_MS = 2000;

const MAZE_DIMENSIONS: MazeDimension[] = ['4x4', '8x4', '12x4'];

interface TelemetryContextType {
  isConnected: boolean;
  batteryLevel: number;
  isLiveActive: boolean;
  liveMaze: MazeDimension | null;
  livePos: { x: number; y: number };
  liveHeading: HeadingDirection;
  livePath: Array<{ x: number; y: number }>;
  liveWalls: Record<string, CellWalls>;
  liveSpeed: number;
  liveTime: number;
  liveBatteryConsumo: number;
  liveGoalReached: boolean;
  resetLiveState: () => void;
}

const TelemetryContext = createContext<TelemetryContextType>({
  isConnected: false,
  batteryLevel: mockData.batteryLevel,
  isLiveActive: false,
  liveMaze: null,
  livePos: { x: 0, y: 0 },
  liveHeading: 'N',
  livePath: [{ x: 0, y: 0 }],
  liveWalls: {},
  liveSpeed: 0,
  liveTime: 0,
  liveBatteryConsumo: 0,
  liveGoalReached: false,
  resetLiveState: () => {},
});

/**
 * Endereço do WebSocket /live. NEXT_PUBLIC_LIVE_URL tem prioridade; sem ela,
 * usa o mesmo host da página na porta 3002 (funciona em localhost, na rede
 * local e no Codespaces, onde a porta faz parte do hostname).
 */
export function resolveLiveUrl(): string {
  if (process.env.NEXT_PUBLIC_LIVE_URL) return process.env.NEXT_PUBLIC_LIVE_URL;

  const { hostname, protocol } = window.location;
  if (hostname.includes('.app.github.dev')) {
    return `wss://${hostname.replace('-3000.', '-3002.')}/live`;
  }
  const wsProtocol = protocol === 'https:' ? 'wss' : 'ws';
  return `${wsProtocol}://${hostname || 'localhost'}:3002/live`;
}

export function TelemetryProvider({ children }: { children: React.ReactNode }) {
  const [isConnected, setIsConnected] = useState<boolean>(false);
  const [batteryLevel, setBatteryLevel] = useState<number>(
    mockData.batteryLevel,
  );

  const [isLiveActive, setIsLiveActive] = useState<boolean>(false);
  const [liveMaze, setLiveMaze] = useState<MazeDimension | null>(null);
  const [livePos, setLivePos] = useState<{ x: number; y: number }>({
    x: 0,
    y: 0,
  });
  const [liveHeading, setLiveHeading] = useState<HeadingDirection>('N');
  const [livePath, setLivePath] = useState<Array<{ x: number; y: number }>>([
    { x: 0, y: 0 },
  ]);
  const [liveWalls, setLiveWalls] = useState<Record<string, CellWalls>>({});
  const [liveSpeed, setLiveSpeed] = useState<number>(0);
  const [liveTime, setLiveTime] = useState<number>(0);
  const [liveBatteryConsumo, setLiveBatteryConsumo] = useState<number>(0);
  const [liveGoalReached, setLiveGoalReached] = useState<boolean>(false);

  const resetLiveState = () => {
    setIsLiveActive(false);
    setLiveMaze(null);
    setLiveGoalReached(false);
    setLivePath([{ x: 0, y: 0 }]);
    setLiveWalls({});
    setLivePos({ x: 0, y: 0 });
    setLiveHeading('N');
    setLiveSpeed(0);
    setLiveTime(0);
    setLiveBatteryConsumo(0);
  };

  useEffect(() => {
    if (typeof window === 'undefined') return;

    const wsUrl = resolveLiveUrl();
    let ws: WebSocket | null = null;
    let reconnectTimer: ReturnType<typeof setTimeout> | null = null;
    let robotTimer: ReturnType<typeof setTimeout> | null = null;
    let disposed = false;

    // "Conectado" = o robô está mandando dados. Cada mensagem renova o prazo.
    const markRobotAlive = () => {
      setIsConnected(true);
      if (robotTimer) clearTimeout(robotTimer);
      robotTimer = setTimeout(() => setIsConnected(false), ROBOT_TIMEOUT_MS);
    };

    const markRobotOffline = () => {
      if (robotTimer) clearTimeout(robotTimer);
      robotTimer = null;
      setIsConnected(false);
    };

    const connect = () => {
      ws = new WebSocket(wsUrl);

      ws.onopen = () => {
        console.log('[Live WS] Conectado ao servidor de broadcast.');
      };

      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);

          if (msg.channel === 'event') {
            markRobotAlive();
            if (msg.event === 'RUN_START') {
              const maze = msg.data?.maze;
              setIsLiveActive(true);
              setLiveMaze(MAZE_DIMENSIONS.includes(maze) ? maze : null);
              setLiveGoalReached(false);
              setLivePath([{ x: 0, y: 0 }]);
              setLiveWalls({});
              setLivePos({ x: 0, y: 0 });
              setLiveHeading('N');
            } else if (msg.event === 'GOAL_REACHED') {
              setLiveGoalReached(true);
            }
          }

          if (msg.channel === 'telemetry') {
            markRobotAlive();
            const telemetry: LiveTelemetryPayload = msg.data;
            setIsLiveActive(true);

            // Atualiza bateria do robô e cabeçalho
            if (typeof telemetry.battery_pct === 'number') {
              setBatteryLevel(Math.round(telemetry.battery_pct));
              setLiveBatteryConsumo(
                Math.max(0, Math.round(100 - telemetry.battery_pct)),
              );
            }

            setLivePos(telemetry.pos);
            setLiveHeading(telemetry.heading);
            setLiveSpeed(Number((telemetry.speed_mm_s / 1000).toFixed(2)));
            setLiveTime(Math.round(telemetry.t_ms / 1000));

            const walls = telemetry.walls;
            if (walls) {
              setLiveWalls((prev) => ({
                ...prev,
                [`${telemetry.pos.x},${telemetry.pos.y}`]: walls,
              }));
            }

            setLivePath((prev) => {
              const last = prev[prev.length - 1];
              if (
                !last ||
                last.x !== telemetry.pos.x ||
                last.y !== telemetry.pos.y
              ) {
                return [...prev, telemetry.pos];
              }
              return prev;
            });
          }
        } catch (err) {
          console.error('[Live WS] Falha ao processar mensagem:', err);
        }
      };

      ws.onerror = (err) => {
        console.warn('[Live WS] Erro de conexão:', err);
      };

      // Sem o backend não dá para saber do robô; tenta de novo até voltar.
      ws.onclose = () => {
        markRobotOffline();
        if (disposed) return;
        reconnectTimer = setTimeout(connect, RECONNECT_DELAY_MS);
      };
    };

    connect();

    return () => {
      disposed = true;
      if (reconnectTimer) clearTimeout(reconnectTimer);
      if (robotTimer) clearTimeout(robotTimer);
      ws?.close();
    };
  }, []);

  return (
    <TelemetryContext.Provider
      value={{
        isConnected,
        batteryLevel,
        isLiveActive,
        liveMaze,
        livePos,
        liveHeading,
        livePath,
        liveWalls,
        liveSpeed,
        liveTime,
        liveBatteryConsumo,
        liveGoalReached,
        resetLiveState,
      }}
    >
      {children}
    </TelemetryContext.Provider>
  );
}

export function useTelemetry() {
  return useContext(TelemetryContext);
}
