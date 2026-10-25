import type {
  CorridaResumo,
  DimensaoLabirinto,
  StatusCorrida,
} from './corridas';

// Histórico fictício no formato de GET /runs, usado com NEXT_PUBLIC_USE_MOCK=true.
function corrida(
  n: number,
  labirinto: DimensaoLabirinto,
  iniciadaEm: string,
  duracaoSegundos: number,
  velocidadeMedia: number,
  bateriaInicial: number,
  bateriaFinal: number,
  status: StatusCorrida = 'FINALIZADA',
): CorridaResumo {
  const inicio = new Date(iniciadaEm);
  return {
    id: `mock-${n}`,
    labirinto,
    status,
    iniciadaEm: inicio.toISOString(),
    finalizadaEm: new Date(
      inicio.getTime() + duracaoSegundos * 1000,
    ).toISOString(),
    duracaoSegundos,
    velocidadeMedia,
    bateriaInicial,
    bateriaFinal,
  };
}

export const mockCorridas: CorridaResumo[] = [
  corrida(1, '4x4', '2026-10-01T14:05:00-03:00', 92, 0.21, 100, 94),
  corrida(2, '4x4', '2026-10-01T14:20:00-03:00', 78, 0.25, 94, 89),
  corrida(3, '4x4', '2026-10-02T10:10:00-03:00', 41, 0.18, 89, 87, 'ABORTADA'),
  corrida(4, '4x4', '2026-10-02T10:30:00-03:00', 64, 0.3, 87, 83),
  corrida(5, '8x4', '2026-10-06T15:00:00-03:00', 168, 0.22, 100, 88),
  corrida(6, '8x4', '2026-10-06T15:25:00-03:00', 143, 0.26, 88, 78),
  corrida(7, '8x4', '2026-10-07T09:40:00-03:00', 97, 0.24, 78, 72, 'ABORTADA'),
  corrida(8, '8x4', '2026-10-07T10:05:00-03:00', 121, 0.31, 72, 64),
  corrida(9, '12x4', '2026-10-13T14:00:00-03:00', 246, 0.23, 100, 81),
  corrida(10, '12x4', '2026-10-13T14:40:00-03:00', 211, 0.27, 81, 66),
  corrida(11, '12x4', '2026-10-14T11:15:00-03:00', 188, 0.3, 98, 85),
  corrida(
    12,
    '12x4',
    '2026-10-14T11:50:00-03:00',
    133,
    0.29,
    85,
    77,
    'ABORTADA',
  ),
];
