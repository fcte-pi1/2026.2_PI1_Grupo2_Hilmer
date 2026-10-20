'use client';

import { useEffect, useReducer, useState } from 'react';
import { io } from 'socket.io-client';
import type { DimensaoLabirinto } from '@/lib/corridas';
import { buildSimulatedRun, SIMULATED_TICK_MS } from '@/lib/simulated-robot';
import {
  initialMapState,
  mapReducer,
  type RobotMessage,
} from '@/lib/telemetry';

export type LinkStatus = 'connecting' | 'connected' | 'disconnected';
export type TelemetrySource = 'simulated' | 'socket';

// 'simulated' (padrão): corrida gerada no navegador, não precisa de backend.
// 'socket': escuta o Socket.IO do backend (eventos "telemetry" e "event").
export const TELEMETRY_SOURCE: TelemetrySource =
  process.env.NEXT_PUBLIC_TELEMETRY_SOURCE === 'socket'
    ? 'socket'
    : 'simulated';

const SOCKET_URL = process.env.NEXT_PUBLIC_API_URL ?? 'http://localhost:3001';
/** Pausa (em ticks) entre o fim de uma corrida simulada e o início da próxima. */
const LOOP_PAUSE_TICKS = 30;

export function useTelemetry(maze: DimensaoLabirinto) {
  const [state, dispatch] = useReducer(mapReducer, initialMapState);
  const [socketStatus, setSocketStatus] = useState<LinkStatus>('connecting');

  // Fonte simulada: reproduz a corrida a 10 Hz e recomeça sozinha.
  useEffect(() => {
    if (TELEMETRY_SOURCE !== 'simulated') return;

    let msgs = buildSimulatedRun(maze);
    let index = 0;
    let pause = 0;

    const timer = setInterval(() => {
      if (index >= msgs.length) {
        if (++pause < LOOP_PAUSE_TICKS) return;
        // O CALIBRATION_DONE do início da nova corrida limpa o mapa.
        msgs = buildSimulatedRun(maze);
        index = 0;
        pause = 0;
      }
      // Eventos saem junto com a próxima telemetria, como no robot_mock.py.
      while (index < msgs.length) {
        const msg = msgs[index++];
        dispatch(msg);
        if (msg.type === 'telemetry') break;
      }
    }, SIMULATED_TICK_MS);

    return () => clearInterval(timer);
  }, [maze]);

  // Fonte real: Socket.IO do backend.
  useEffect(() => {
    if (TELEMETRY_SOURCE !== 'socket') return;

    const socket = io(SOCKET_URL, { transports: ['websocket'] });
    socket.on('connect', () => setSocketStatus('connected'));
    socket.on('disconnect', () => setSocketStatus('disconnected'));
    socket.on('connect_error', () => setSocketStatus('disconnected'));
    socket.on('telemetry', (msg: RobotMessage) => dispatch(msg));
    socket.on('event', (msg: RobotMessage) => dispatch(msg));

    return () => {
      socket.disconnect();
    };
  }, []);

  const status: LinkStatus =
    TELEMETRY_SOURCE === 'simulated' ? 'connected' : socketStatus;

  return { state, status };
}
