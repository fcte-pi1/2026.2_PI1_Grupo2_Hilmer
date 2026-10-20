'use client';

import { MazeMap } from './MazeMap';
import {
  TELEMETRY_SOURCE,
  useTelemetry,
  type LinkStatus,
} from '@/hooks/useTelemetry';
import type { DimensaoLabirinto } from '@/lib/corridas';
import { MAZE_SIZES } from '@/lib/telemetry';

const STATUS_LABEL: Record<LinkStatus, string> = {
  connecting: 'Conectando…',
  connected: 'Conectado',
  disconnected: 'Reconectando…',
};

interface LiveMapProps {
  maze: DimensaoLabirinto;
}

export function LiveMap({ maze }: LiveMapProps) {
  const { cols, rows } = MAZE_SIZES[maze];
  const { state, status } = useTelemetry(maze);
  const robot = state.robot;

  return (
    <section className="lg:flex-1 w-full max-w-200 font-mono">
      <MazeMap cols={cols} rows={rows} state={state} />
      <p className="mt-2 text-sm opacity-80">
        {TELEMETRY_SOURCE === 'simulated'
          ? 'Simulação local'
          : STATUS_LABEL[status]}
        {robot
          ? ` · célula (${robot.pos.x}, ${robot.pos.y}) · direção ${robot.heading} · ${robot.state}`
          : ' · aguardando o robô'}
      </p>
    </section>
  );
}
