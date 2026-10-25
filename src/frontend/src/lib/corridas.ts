import { mockCorridas } from './mock-corridas';

export const DIMENSOES_LABIRINTO = ['4x4', '8x4', '12x4'] as const;
export type DimensaoLabirinto = (typeof DIMENSOES_LABIRINTO)[number];
export type StatusCorrida =
  'EM_ANDAMENTO' | 'FINALIZADA' | 'CONCLUIDA' | 'ABORTADA' | 'FALHA';

/**
 * Resumo de uma corrida, no formato da rota GET /runs do backend.
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
  /** battery_pct da primeira telemetria; null se a corrida não tem telemetria. */
  bateriaInicial: number | null;
  /** battery_pct da última telemetria; null se a corrida não tem telemetria. */
  bateriaFinal: number | null;
}

/** Melhor corrida de um labirinto, no formato da rota GET /runs/best. */
export interface RecordeLabirinto {
  labirinto: DimensaoLabirinto;
  labirintoId: string;
  corridaId: string;
  status: StatusCorrida;
  tempoSegundos: number;
  iniciadaEm: string;
  finalizadaEm: string;
}

// As buscas rodam no servidor do Next; API_URL tem prioridade (no Docker
// aponta para o nome do serviço), com NEXT_PUBLIC_API_URL como alternativa.
const API_URL = process.env.API_URL ?? process.env.NEXT_PUBLIC_API_URL;
// true: usa os dados de mock-corridas.ts (demonstração sem backend).
const USAR_MOCK = process.env.NEXT_PUBLIC_USE_MOCK === 'true';

export function ehDimensaoLabirinto(
  valor: unknown,
): valor is DimensaoLabirinto {
  return DIMENSOES_LABIRINTO.includes(valor as DimensaoLabirinto);
}

async function buscarApi<T>(caminho: string): Promise<T> {
  if (!API_URL) throw new Error('API_URL não está definida');
  const res = await fetch(`${API_URL}${caminho}`);
  if (!res.ok) throw new Error(`GET ${caminho} falhou (HTTP ${res.status})`);
  return res.json();
}

/** Corridas do banco, opcionalmente filtradas por labirinto (GET /runs?mazeId=). */
export async function getCorridas(
  labirinto?: DimensaoLabirinto,
): Promise<CorridaResumo[]> {
  if (USAR_MOCK) {
    return mockCorridas.filter((c) => !labirinto || c.labirinto === labirinto);
  }
  return buscarApi(labirinto ? `/runs?mazeId=${labirinto}` : '/runs');
}

/** Recorde (menor tempo) de cada labirinto (GET /runs/best). */
export async function getRecordes(): Promise<RecordeLabirinto[]> {
  if (USAR_MOCK) return recordesDoMock();
  return buscarApi('/runs/best');
}

function recordesDoMock(): RecordeLabirinto[] {
  return DIMENSOES_LABIRINTO.flatMap((labirinto) => {
    const [melhor] = mockCorridas
      .filter((c) => c.labirinto === labirinto && c.status === 'FINALIZADA')
      .sort((a, b) => (a.duracaoSegundos ?? 0) - (b.duracaoSegundos ?? 0));
    if (!melhor) return [];
    return {
      labirinto,
      labirintoId: labirinto,
      corridaId: melhor.id,
      status: melhor.status,
      tempoSegundos: melhor.duracaoSegundos ?? 0,
      iniciadaEm: melhor.iniciadaEm,
      finalizadaEm: melhor.finalizadaEm ?? melhor.iniciadaEm,
    };
  });
}
