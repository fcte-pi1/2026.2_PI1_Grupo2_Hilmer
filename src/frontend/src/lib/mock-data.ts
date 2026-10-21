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

// Trajetórias e paredes rigorosamente calibradas: o robô navega dentro dos corredores
// sem nunca colidir ou atravessar qualquer parede!
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
    // Trajeto: (0,0) -> (0,1) -> (0,2) -> (1,2) -> (2,2) -> (2,3) -> (3,3)
    path: [
      { x: 0, y: 0 },
      { x: 0, y: 1 },
      { x: 0, y: 2 },
      { x: 1, y: 2 },
      { x: 2, y: 2 },
      { x: 2, y: 3 },
      { x: 3, y: 3 },
    ],
    // Paredes que delimitam o corredor por onde o robô passa (sem bloquear a passagem)
    discoveredWalls: {
      '0,0': { e: true }, // Parede à direita força subida para (0,1)
      '0,1': { e: true }, // Parede à direita força subida para (0,2)
      '0,2': { n: true }, // Parede ao norte força curva para leste (1,2)
      '1,2': { n: true, s: true }, // Canal reto fechado em cima e embaixo
      '2,2': { s: true, e: true }, // Parede ao sul e leste força curva para norte (2,3)
      '2,3': { n: true, w: true }, // Parede ao norte força curva para leste (3,3)
      '3,3': { s: true }, // Parede ao sul delimita a célula de chegada
      // Paredes adicionais do labirinto (becos sem saída)
      '1,0': { n: true },
      '1,1': { e: true },
      '3,1': { w: true },
      '3,2': { s: true },
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
    // Trajeto: (0,0) -> (0,1) -> (1,1) -> (2,1) -> (2,2) -> (3,2) -> (4,2) -> (5,2) -> (5,3) -> (6,3) -> (7,3)
    path: [
      { x: 0, y: 0 },
      { x: 0, y: 1 },
      { x: 1, y: 1 },
      { x: 2, y: 1 },
      { x: 2, y: 2 },
      { x: 3, y: 2 },
      { x: 4, y: 2 },
      { x: 5, y: 2 },
      { x: 5, y: 3 },
      { x: 6, y: 3 },
      { x: 7, y: 3 },
    ],
    discoveredWalls: {
      '0,0': { e: true }, // Força subida para (0,1)
      '0,1': { n: true }, // Força curva para leste em (1,1)
      '1,1': { n: true, s: true }, // Canal leste
      '2,1': { s: true, e: true }, // Força curva para norte em (2,2)
      '2,2': { n: true, w: true }, // Força curva para leste em (3,2)
      '3,2': { n: true, s: true }, // Canal leste
      '4,2': { n: true, s: true }, // Canal leste
      '5,2': { s: true, e: true }, // Força subida para (5,3)
      '5,3': { n: true, w: true }, // Força curva para leste em (6,3)
      '6,3': { s: true }, // Canal reto de chegada
      '7,3': { s: true }, // Célula de chegada
      // Outras paredes do labirinto
      '0,2': { e: true },
      '1,0': { n: true },
      '3,0': { e: true },
      '4,0': { n: true },
      '6,1': { n: true, w: true },
      '7,1': { w: true },
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
    // Trajeto contínuo passo a passo sem saltos
    path: [
      { x: 0, y: 0 },
      { x: 0, y: 1 },
      { x: 0, y: 2 },
      { x: 1, y: 2 },
      { x: 2, y: 2 },
      { x: 2, y: 1 },
      { x: 3, y: 1 },
      { x: 4, y: 1 },
      { x: 5, y: 1 },
      { x: 5, y: 2 },
      { x: 6, y: 2 },
      { x: 7, y: 2 },
      { x: 8, y: 2 },
      { x: 8, y: 3 },
      { x: 9, y: 3 },
      { x: 10, y: 3 },
      { x: 11, y: 3 },
    ],
    discoveredWalls: {
      '0,0': { e: true },
      '0,1': { e: true },
      '0,2': { n: true },
      '1,2': { n: true, s: true },
      '2,2': { n: true, e: true }, // Desce para (2,1)
      '2,1': { s: true, w: true }, // Vira para leste (3,1)
      '3,1': { n: true, s: true },
      '4,1': { n: true, s: true },
      '5,1': { s: true, e: true }, // Sobe para (5,2)
      '5,2': { n: true, w: true }, // Vira para leste (6,2)
      '6,2': { n: true, s: true },
      '7,2': { n: true, s: true },
      '8,2': { s: true, e: true }, // Sobe para (8,3)
      '8,3': { w: true },
      '9,3': { s: true },
      '10,3': { s: true },
      '11,3': { s: true },
    },
  },
};

export const mockData: DashboardData = mazeMocks['4x4'];
