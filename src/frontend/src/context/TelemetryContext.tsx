'use client';

import React, { createContext, useContext, useEffect, useState } from 'react';
import { HeadingDirection, mockData } from '@/lib/mock-data';

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
  state: string;
}

interface TelemetryContextType {
  isConnected: boolean;
  batteryLevel: number;
  isLiveActive: boolean;
  livePos: { x: number; y: number };
  liveHeading: HeadingDirection;
  livePath: Array<{ x: number; y: number }>;
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
  livePos: { x: 0, y: 0 },
  liveHeading: 'N',
  livePath: [{ x: 0, y: 0 }],
  liveSpeed: 0,
  liveTime: 0,
  liveBatteryConsumo: 0,
  liveGoalReached: false,
  resetLiveState: () => {},
});

export function TelemetryProvider({ children }: { children: React.ReactNode }) {
  const [isConnected, setIsConnected] = useState<boolean>(false);
  const [batteryLevel, setBatteryLevel] = useState<number>(mockData.batteryLevel);

  const [isLiveActive, setIsLiveActive] = useState<boolean>(false);
  const [livePos, setLivePos] = useState<{ x: number; y: number }>({ x: 0, y: 0 });
  const [liveHeading, setLiveHeading] = useState<HeadingDirection>('N');
  const [livePath, setLivePath] = useState<Array<{ x: number; y: number }>>([{ x: 0, y: 0 }]);
  const [liveSpeed, setLiveSpeed] = useState<number>(0);
  const [liveTime, setLiveTime] = useState<number>(0);
  const [liveBatteryConsumo, setLiveBatteryConsumo] = useState<number>(0);
  const [liveGoalReached, setLiveGoalReached] = useState<boolean>(false);

  const resetLiveState = () => {
    setIsLiveActive(false);
    setLiveGoalReached(false);
    setLivePath([{ x: 0, y: 0 }]);
    setLivePos({ x: 0, y: 0 });
    setLiveHeading('N');
    setLiveSpeed(0);
    setLiveTime(0);
    setLiveBatteryConsumo(0);
  };

  useEffect(() => {
    if (typeof window === 'undefined') return;

    let wsUrl = 'ws://localhost:3002/live';
    if (window.location.hostname.includes('.app.github.dev')) {
      const codespaceHost = window.location.hostname.replace('-3000.', '-3002.');
      wsUrl = `wss://${codespaceHost}/live`;
    }

    const ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      setIsConnected(true);
      console.log('[Live WS] Conectado ao servidor de broadcast.');
    };

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);

        if (msg.channel === 'event') {
          if (msg.event === 'RUN_START') {
            setIsLiveActive(true);
            setLiveGoalReached(false);
            setLivePath([{ x: 0, y: 0 }]);
            setLivePos({ x: 0, y: 0 });
            setLiveHeading('N');
          } else if (msg.event === 'GOAL_REACHED') {
            setLiveGoalReached(true);
          }
        }

        if (msg.channel === 'telemetry') {
          const telemetry: LiveTelemetryPayload = msg.data;
          setIsLiveActive(true);

          // Atualiza bateria do robô e cabeçalho
          if (typeof telemetry.battery_pct === 'number') {
            setBatteryLevel(Math.round(telemetry.battery_pct));
            setLiveBatteryConsumo(Math.max(0, Math.round(100 - telemetry.battery_pct)));
          }

          setLivePos(telemetry.pos);
          setLiveHeading(telemetry.heading);
          setLiveSpeed(Number((telemetry.speed_mm_s / 1000).toFixed(2)));
          setLiveTime(Math.round(telemetry.t_ms / 1000));

          setLivePath((prev) => {
            const last = prev[prev.length - 1];
            if (!last || last.x !== telemetry.pos.x || last.y !== telemetry.pos.y) {
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
      console.error('[Live WS] Erro de conexão:', err);
      setIsConnected(false);
    };

    ws.onclose = () => {
      setIsConnected(false);
    };

    return () => {
      ws.close();
    };
  }, []);

  return (
    <TelemetryContext.Provider
      value={{
        isConnected,
        batteryLevel,
        isLiveActive,
        livePos,
        liveHeading,
        livePath,
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