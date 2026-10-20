import { describe, expect, it } from 'vitest';
import { buildSimulatedRun } from './simulated-robot';
import {
  initialMapState,
  mapReducer,
  type MapState,
  type RobotMessage,
  type TelemetryMessage,
} from './telemetry';

function run(msgs: RobotMessage[], from: MapState = initialMapState) {
  return msgs.reduce(mapReducer, from);
}

function telemetry(
  seq: number,
  x: number,
  y: number,
  extra: Partial<TelemetryMessage> = {},
): TelemetryMessage {
  return {
    v: 1,
    type: 'telemetry',
    robot_id: 'mm-01',
    seq,
    t_ms: seq * 100,
    pos: { x, y },
    heading: 'N',
    speed_mm_s: 300,
    battery_pct: 90,
    state: 'EXPLORING',
    ...extra,
  };
}

describe('mapReducer', () => {
  it('guarda a primeira posição como começo e acompanha o robô', () => {
    const s = run([telemetry(0, 0, 0), telemetry(1, 0, 1)]);
    expect(s.start).toEqual({ x: 0, y: 0 });
    expect(s.robot?.pos).toEqual({ x: 0, y: 1 });
  });

  it('remove repetições consecutivas do caminho', () => {
    const s = run([
      telemetry(0, 0, 0),
      telemetry(1, 0, 0),
      telemetry(2, 0, 1),
      telemetry(3, 0, 1),
    ]);
    expect(s.path).toEqual([
      { x: 0, y: 0 },
      { x: 0, y: 1 },
    ]);
  });

  it('ignora telemetria repetida ou fora de ordem', () => {
    const s = run([telemetry(5, 0, 0), telemetry(5, 3, 3), telemetry(4, 2, 2)]);
    expect(s.robot?.pos).toEqual({ x: 0, y: 0 });
    expect(s.path).toHaveLength(1);
  });

  it('acumula as paredes por célula', () => {
    const s = run([
      telemetry(0, 1, 1, { walls: { n: true } }),
      telemetry(1, 1, 1, { walls: { w: true } }),
    ]);
    expect(s.walls['1,1']).toEqual({ n: true, w: true });
  });

  it('marca o fim só com GOAL_REACHED', () => {
    const before = run([telemetry(0, 0, 0)]);
    expect(before.goal).toBeNull();
    const after = mapReducer(before, {
      v: 1,
      type: 'event',
      robot_id: 'mm-01',
      seq: 1,
      t_ms: 100,
      event: 'GOAL_REACHED',
      pos: { x: 3, y: 3 },
    });
    expect(after.goal).toEqual({ x: 3, y: 3 });
  });

  it('limpa o mapa em RUN_START mesmo com o seq recomeçando', () => {
    const first = run(buildSimulatedRun('4x4'));
    const second = run(buildSimulatedRun('4x4').slice(0, 10), first);
    expect(second.path.length).toBeLessThan(first.path.length);
    expect(second.goal).toBeNull();
  });

  it('gira pelo menor caminho (N para W é -90°, não +270°)', () => {
    const s = run([
      telemetry(0, 0, 0, { heading: 'N' }),
      telemetry(1, 0, 0, { heading: 'W' }),
    ]);
    expect(s.robot?.angle).toBe(-90);
  });

  it('reset volta ao estado inicial', () => {
    expect(mapReducer(run([telemetry(0, 1, 1)]), { type: 'reset' })).toBe(
      initialMapState,
    );
  });
});

describe('buildSimulatedRun', () => {
  it.each([
    ['4x4', { x: 3, y: 3 }],
    ['8x4', { x: 7, y: 3 }],
    ['12x4', { x: 11, y: 3 }],
  ] as const)('%s termina no objetivo', (maze, goal) => {
    const msgs = buildSimulatedRun(maze);
    const s = run(msgs);
    expect(s.goal).toEqual(goal);
    expect(s.robot?.pos).toEqual(goal);
    expect(s.robot?.state).toBe('GOAL_REACHED');
  });

  it('gera seq crescente a partir de 0', () => {
    const seqs = buildSimulatedRun('4x4').map((m) => m.seq);
    expect(seqs).toEqual(seqs.map((_, i) => i));
  });

  it('só usa células dentro do labirinto', () => {
    for (const m of buildSimulatedRun('8x4')) {
      if (m.type === 'telemetry') {
        expect(m.pos.x).toBeGreaterThanOrEqual(0);
        expect(m.pos.x).toBeLessThan(8);
        expect(m.pos.y).toBeLessThan(4);
      }
    }
  });
});
