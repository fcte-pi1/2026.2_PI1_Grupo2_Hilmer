import type { DimensaoLabirinto } from './corridas';
import type { Cell, Heading, RobotMessage, Walls } from './telemetry';
import { cellKey } from './telemetry';

/**
 * Corrida simulada no próprio navegador, espelhando mocks/robot_mock.py
 * (mesmos trajetos, 10 Hz, 180 mm por célula). Serve para desenvolver o mapa
 * sem backend. Quando o Socket.IO existir, use NEXT_PUBLIC_TELEMETRY_SOURCE=socket.
 */
const PATHS: Record<DimensaoLabirinto, [number, number][]> = {
  '4x4': [
    [0, 0],
    [0, 1],
    [0, 2],
    [1, 2],
    [1, 1],
    [2, 1],
    [3, 1],
    [3, 2],
    [3, 3],
  ],
  '8x4': [
    [0, 0],
    [1, 0],
    [2, 0],
    [2, 1],
    [1, 1],
    [1, 2],
    [0, 2],
    [0, 3],
    [1, 3],
    [2, 3],
    [3, 3],
    [4, 3],
    [5, 3],
    [5, 2],
    [6, 2],
    [6, 3],
    [7, 3],
  ],
  '12x4': [
    [0, 0],
    [0, 1],
    [0, 2],
    [0, 3],
    [1, 3],
    [2, 3],
    [2, 2],
    [2, 1],
    [1, 1],
    [1, 0],
    [2, 0],
    [3, 0],
    [4, 0],
    [5, 0],
    [6, 0],
    [7, 0],
    [7, 1],
    [7, 2],
    [8, 2],
    [9, 2],
    [9, 3],
    [10, 3],
    [11, 3],
  ],
};

const CELL_MM = 180;
const SPEED_MM_S = 300;
const RATE_HZ = 10;

const DIRS: { dx: number; dy: number; heading: Heading; wall: keyof Walls }[] =
  [
    { dx: 0, dy: 1, heading: 'N', wall: 'n' },
    { dx: 1, dy: 0, heading: 'E', wall: 'e' },
    { dx: 0, dy: -1, heading: 'S', wall: 's' },
    { dx: -1, dy: 0, heading: 'W', wall: 'w' },
  ];

/**
 * Paredes só para demonstração: cada célula do trajeto fica aberta apenas
 * para a célula anterior e a seguinte; os demais lados viram parede.
 */
function demoWalls(path: Cell[]): Record<string, Walls> {
  const result: Record<string, Walls> = {};
  path.forEach((cell, i) => {
    const open = new Set<string>();
    for (const other of [path[i - 1], path[i + 1]]) {
      if (other) open.add(cellKey(other));
    }
    const walls: Walls = {};
    for (const d of DIRS) {
      const neighbour = { x: cell.x + d.dx, y: cell.y + d.dy };
      walls[d.wall] = !open.has(cellKey(neighbour));
    }
    result[cellKey(cell)] = walls;
  });
  return result;
}

/** Gera a sequência completa de mensagens (mesma ordem do robot_mock.py). */
export function buildSimulatedRun(
  maze: DimensaoLabirinto,
  robotId = 'mm-sim',
): RobotMessage[] {
  const path: Cell[] = PATHS[maze].map(([x, y]) => ({ x, y }));
  const walls = demoWalls(path);
  const tickMs = 1000 / RATE_HZ;
  const stepMm = SPEED_MM_S / RATE_HZ;

  const msgs: RobotMessage[] = [];
  let seq = 0;
  let tMs = 0;
  let odo = 0;
  let battery = 98;

  msgs.push({
    v: 1,
    robot_id: robotId,
    seq: seq++,
    t_ms: tMs,
    type: 'event',
    event: 'CALIBRATION_DONE',
    ok: true,
  });
  tMs += 200;
  msgs.push({
    v: 1,
    robot_id: robotId,
    seq: seq++,
    t_ms: tMs,
    type: 'event',
    event: 'RUN_START',
    battery_pct: battery,
  });
  tMs += 100;

  let heading: Heading = 'N';
  for (let i = 0; i < path.length - 1; i++) {
    const cur = path[i];
    const next = path[i + 1];
    heading = DIRS.find(
      (d) => d.dx === next.x - cur.x && d.dy === next.y - cur.y,
    )!.heading;
    let walked = 0;
    while (walked < CELL_MM) {
      const step = Math.min(stepMm, CELL_MM - walked);
      walked += step;
      odo += step;
      tMs += tickMs;
      battery -= 0.02;
      const pos = walked < CELL_MM / 2 ? cur : next;
      msgs.push({
        v: 1,
        robot_id: robotId,
        seq: seq++,
        t_ms: Math.round(tMs),
        type: 'telemetry',
        pos,
        heading,
        speed_mm_s: SPEED_MM_S,
        odo_mm: Math.round(odo),
        battery_pct: Math.round(battery * 10) / 10,
        walls: walls[cellKey(pos)],
        state: 'EXPLORING',
      });
    }
  }

  const goal = path[path.length - 1];
  msgs.push({
    v: 1,
    robot_id: robotId,
    seq: seq++,
    t_ms: Math.round(tMs),
    type: 'event',
    event: 'GOAL_REACHED',
    pos: goal,
  });
  tMs += 100;
  msgs.push({
    v: 1,
    robot_id: robotId,
    seq: seq++,
    t_ms: Math.round(tMs),
    type: 'telemetry',
    pos: goal,
    heading,
    speed_mm_s: 0,
    odo_mm: Math.round(odo),
    battery_pct: Math.round(battery * 10) / 10,
    walls: walls[cellKey(goal)],
    state: 'GOAL_REACHED',
  });
  tMs += 100;
  msgs.push({
    v: 1,
    robot_id: robotId,
    seq: seq++,
    t_ms: Math.round(tMs),
    type: 'event',
    event: 'RUN_END',
  });

  return msgs;
}

export const SIMULATED_TICK_MS = 1000 / RATE_HZ;
