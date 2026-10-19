import { mockCorridas } from './mock-corridas';

export type DimensaoLabirinto = '4x4' | '8x4' | '12x4';
export type StatusCorrida = 'EM_ANDAMENTO' | 'FINALIZADA' | 'ABORTADA';

/**
 * Resumo de uma corrida, no formato esperado da rota GET /corridas do backend.
 * Os valores agregados (velocidade média, bateria) são calculados pelo backend
 * a partir das telemetrias da corrida.
 */
export interface CorridaResumo {
  id: string;
  labirinto: DimensaoLabirinto;
  status: StatusCorrida;
  /** Data/hora ISO 8601. */
  iniciadaEm: string;
  finalizadaEm: string | null;
  /** Duração em segundos; null enquanto a corrida está em andamento. */
  duracaoSegundos: number | null;
  /** Média de speed_mm_s das telemetrias, convertida para m/s. */
  velocidadeMedia: number;
  /** battery_pct da primeira telemetria da corrida. */
  bateriaInicial: number;
  /** battery_pct da última telemetria da corrida. */
  bateriaFinal: number;
}

const API_URL = process.env.NEXT_PUBLIC_API_URL;
// Enquanto a rota REST não existe no backend, o padrão é usar os dados mock.
const USAR_MOCK = process.env.NEXT_PUBLIC_USE_MOCK !== 'false';

export async function getCorridas(): Promise<CorridaResumo[]> {
  if (USAR_MOCK) return mockCorridas;

  if (!API_URL) throw new Error('NEXT_PUBLIC_API_URL não está definida');
  const res = await fetch(`${API_URL}/corridas`);
  if (!res.ok) throw new Error(`GET /corridas falhou (HTTP ${res.status})`);
  return res.json();
}
