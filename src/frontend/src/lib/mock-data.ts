export interface DashboardData {
  mazeType: '4x4' | '8x4' | '12x4';
  batteryConsumption: number;
  speed: number;
  completionTime: number;
  challengeCompleted: boolean;
  batteryLevel: number;
  connected: boolean;
}

export const mockData: DashboardData = {
  mazeType: '4x4',
  speed: 2.5,
  completionTime: 60,
  batteryConsumption: 12,
  challengeCompleted: true,
  batteryLevel: 67,
  connected: true,
};
