export type MazeDimension = '4x4' | '8x4' | '12x4';
export type HeadingDirection = 'N' | 'E' | 'S' | 'W';

export interface CellWalls {
  n?: boolean;
  e?: boolean;
  s?: boolean;
  w?: boolean;
}

export interface RobotPosition {
  x: number;
  y: number;
  heading: HeadingDirection;
}

export interface TrajectoryPoint {
  x: number;
  y: number;
}

export interface DashboardData {
  mazeType: MazeDimension;
  batteryConsumption: number;
  speed: number;
  completionTime: number;
  challengeCompleted: boolean;
  batteryLevel: number;
  connected: boolean;
  robotPosition: RobotPosition;
  path: TrajectoryPoint[];
  discoveredWalls: Record<string, CellWalls>;
}

// Trajetórias e paredes pré-definidas para demonstração em cada tamanho de labirinto
export const mazeMocks: Record<MazeDimension, DashboardData> = {
  '4x4': {
    mazeType: '4x4',
    speed: 2.5,
    completionTime: 60,
    batteryConsumption: 12,
    challengeCompleted: true,
    batteryLevel: 67,
    connected: true,
    robotPosition: { x: 3, y: 3, heading: 'E' },
    path: [
      { x: 0, y: 0 },
      { x: 0, y: 1 },
      { x: 0, y: 2 },
      { x: 1, y: 2 },
      { x: 2, y: 2 },
      { x: 2, y: 3 },
      { x: 3, y: 3 },
    ],
    discoveredWalls: {
      '0,0': { w: true, s: true, e: false, n: false },
      '0,1': { w: true, e: true, s: false, n: false },
      '0,2': { w: true, n: true, e: false, s: false },
      '1,2': { n: true, s: false, w: false, e: false },
      '2,2': { s: true, n: false, w: false, e: false },
      '2,3': { w: true, n: true, e: false, s: false },
      '3,3': { n: true, e: true, w: false, s: false },
    },
  },
  '8x4': {
    mazeType: '8x4',
    speed: 3.1,
    completionTime: 95,
    batteryConsumption: 18,
    challengeCompleted: true,
    batteryLevel: 58,
    connected: true,
    robotPosition: { x: 7, y: 3, heading: 'E' },
    path: [
      { x: 0, y: 0 },
      { x: 0, y: 1 },
      { x: 1, y: 1 },
      { x: 2, y: 1 },
      { x: 3, y: 1 },
      { x: 4, y: 2 },
      { x: 5, y: 2 },
      { x: 6, y: 3 },
      { x: 7, y: 3 },
    ],
    discoveredWalls: {
      '0,0': { w: true, s: true, e: false, n: false },
      '0,1': { w: true, e: true, s: false, n: false },
      '1,1': { n: false, s: true, w: false, e: false },
      '2,1': { n: true, s: false, w: false, e: false },
      '3,1': { n: false, s: true, w: false, e: false },
      '4,2': { w: true, e: false, s: false, n: false },
      '5,2': { n: true, s: false, w: false, e: false },
      '6,3': { w: true, e: false, s: false, n: false },
      '7,3': { n: true, e: true, w: false, s: false },
    },
  },
  '12x4': {
    mazeType: '12x4',
    speed: 3.8,
    completionTime: 140,
    batteryConsumption: 26,
    challengeCompleted: true,
    batteryLevel: 45,
    connected: true,
    robotPosition: { x: 11, y: 3, heading: 'E' },
    path: [
      { x: 0, y: 0 },
      { x: 0, y: 1 },
      { x: 1, y: 1 },
      { x: 2, y: 1 },
      { x: 3, y: 1 },
      { x: 4, y: 2 },
      { x: 5, y: 2 },
      { x: 6, y: 2 },
      { x: 7, y: 2 },
      { x: 8, y: 3 },
      { x: 9, y: 3 },
      { x: 10, y: 3 },
      { x: 11, y: 3 },
    ],
    discoveredWalls: {
      '0,0': { w: true, s: true, e: false, n: false },
      '0,1': { w: true, e: true, s: false, n: false },
      '2,1': { n: false, s: true, w: false, e: false },
      '4,2': { n: true, s: false, w: false, e: false },
      '6,2': { n: false, s: true, w: false, e: false },
      '8,3': { w: true, e: false, s: false, n: false },
      '10,3': { n: false, s: true, w: false, e: false },
      '11,3': { n: true, e: true, w: false, s: false },
    },
  },
};

export const mockData: DashboardData = mazeMocks['4x4'];
