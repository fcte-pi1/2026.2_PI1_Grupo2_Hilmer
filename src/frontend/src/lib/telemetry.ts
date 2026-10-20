import type { DimensaoLabirinto } from './corridas';

// Contrato de telemetria (README do projeto e backend/src/telemetry/dto).
export type Heading = 'N' | 'E' | 'S' | 'W';
export type RobotState =
  'IDLE' | 'EXPLORING' | 'RETURNING' | 'SPEED_RUN' | 'GOAL_REACHED' | 'ERROR';
export type RobotEventType =
  'CALIBRATION_DONE' | 'RUN_START' | 'GOAL_REACHED' | 'RUN_END';

export interface Cell {
  x: number;
  y: number;
}

export interface Walls {
  n?: boolean;
  e?: boolean;
  s?: boolean;
  w?: boolean;
}

interface BaseMessage {
  v: 1;
  robot_id: string;
  seq: number;
  t_ms: number;
}

export interface TelemetryMessage extends BaseMessage {
  type: 'telemetry';
  pos: Cell;
  heading: Heading;
  speed_mm_s: number;
  odo_mm?: number;
  battery_pct: number;
  walls?: Walls;
  state: RobotState;
}

export interface EventMessage extends BaseMessage {
  type: 'event';
  event: RobotEventType;
  ok?: boolean;
  battery_pct?: number;
  pos?: Cell;
}

export type RobotMessage = TelemetryMessage | EventMessage;

/** Tamanho em células (colunas x linhas) de cada labirinto. */
export const MAZE_SIZES: Record<
  DimensaoLabirinto,
  { cols: number; rows: number }
> = {
  '4x4': { cols: 4, rows: 4 },
  '8x4': { cols: 8, rows: 4 },
  '12x4': { cols: 12, rows: 4 },
};

export const HEADING_DEG: Record<Heading, number> = {
  N: 0,
  E: 90,
  S: 180,
  W: 270,
};

export function cellKey(c: Cell): string {
  return `${c.x},${c.y}`;
}

// ---------------------------------------------------------------------------
// Estado do mapa e reducer (função pura: fácil de testar e de reaproveitar)
// ---------------------------------------------------------------------------

export interface RobotView {
  pos: Cell;
  heading: Heading;
  /** Ângulo acumulado (pode passar de 360) para a rotação animar pelo menor caminho. */
  angle: number;
  state: RobotState;
}

export interface MapState {
  robot: RobotView | null;
  start: Cell | null;
  goal: Cell | null;
  /** Células visitadas, sem repetições consecutivas (mesma regra do backend, seção 2.3). */
  path: Cell[];
  walls: Record<string, Walls>;
  lastSeq: number;
}

export const initialMapState: MapState = {
  robot: null,
  start: null,
  goal: null,
  path: [],
  walls: {},
  lastSeq: -1,
};

export type MapAction = RobotMessage | { type: 'reset' };

function nextAngle(prev: number | undefined, heading: Heading): number {
  const target = HEADING_DEG[heading];
  if (prev === undefined) return target;
  const delta = ((((target - prev) % 360) + 540) % 360) - 180;
  return prev + delta;
}

export function mapReducer(state: MapState, action: MapAction): MapState {
  if (action.type === 'reset') return initialMapState;

  if (action.type === 'event') {
    switch (action.event) {
      // Nova calibração/corrida: o seq pode recomeçar, então o mapa também recomeça.
      case 'CALIBRATION_DONE':
      case 'RUN_START':
        return { ...initialMapState, lastSeq: action.seq };
      case 'GOAL_REACHED':
        return {
          ...state,
          goal: action.pos ?? state.robot?.pos ?? state.goal,
          lastSeq: action.seq,
        };
      default:
        return { ...state, lastSeq: action.seq };
    }
  }

  // Telemetria repetida ou fora de ordem é ignorada.
  if (action.seq <= state.lastSeq) return state;

  const { pos, heading } = action;
  const last = state.path[state.path.length - 1];
  const moved = !last || last.x !== pos.x || last.y !== pos.y;

  let walls = state.walls;
  if (action.walls) {
    const key = cellKey(pos);
    walls = { ...walls, [key]: { ...walls[key], ...action.walls } };
  }

  return {
    robot: {
      pos,
      heading,
      angle: nextAngle(state.robot?.angle, heading),
      state: action.state,
    },
    start: state.start ?? pos,
    goal: state.goal ?? (action.state === 'GOAL_REACHED' ? pos : null),
    path: moved ? [...state.path, pos] : state.path,
    walls,
    lastSeq: action.seq,
  };
}
