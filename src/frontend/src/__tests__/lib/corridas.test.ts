// As variáveis de ambiente são lidas quando o módulo é carregado, então cada
// teste importa uma cópia nova de corridas.ts com o ambiente que precisa.
async function carregar(env: Record<string, string | undefined>) {
  jest.resetModules();
  process.env = { ...ambienteOriginal, ...env };
  return import('@/lib/corridas');
}

const ambienteOriginal = process.env;
const fetchMock = jest.fn();

function resposta(corpo: unknown, status = 200) {
  return { ok: status >= 200 && status < 300, status, json: async () => corpo };
}

beforeEach(() => {
  global.fetch = fetchMock;
});

afterAll(() => {
  process.env = ambienteOriginal;
});

describe('ehDimensaoLabirinto', () => {
  it.each(['4x4', '8x4', '12x4'])('aceita %s', async (valor) => {
    const { ehDimensaoLabirinto } = await carregar({});
    expect(ehDimensaoLabirinto(valor)).toBe(true);
  });

  it.each(['16x16', '', undefined, ['4x4']])('rejeita %p', async (valor) => {
    const { ehDimensaoLabirinto } = await carregar({});
    expect(ehDimensaoLabirinto(valor)).toBe(false);
  });
});

describe('com a API', () => {
  const env = { API_URL: 'http://api:3002', NEXT_PUBLIC_USE_MOCK: undefined };

  it('getCorridas busca GET /runs', async () => {
    const { getCorridas } = await carregar(env);
    fetchMock.mockResolvedValue(resposta([{ id: 'c1' }]));

    await expect(getCorridas()).resolves.toEqual([{ id: 'c1' }]);
    expect(fetchMock).toHaveBeenCalledWith('http://api:3002/runs');
  });

  it('getCorridas filtra pelo labirinto com ?mazeId=', async () => {
    const { getCorridas } = await carregar(env);
    fetchMock.mockResolvedValue(resposta([]));

    await getCorridas('8x4');
    expect(fetchMock).toHaveBeenCalledWith('http://api:3002/runs?mazeId=8x4');
  });

  it('getRecordes busca GET /runs/best', async () => {
    const { getRecordes } = await carregar(env);
    fetchMock.mockResolvedValue(resposta([{ labirinto: '4x4' }]));

    await expect(getRecordes()).resolves.toEqual([{ labirinto: '4x4' }]);
    expect(fetchMock).toHaveBeenCalledWith('http://api:3002/runs/best');
  });

  it('usa NEXT_PUBLIC_API_URL quando API_URL não está definida', async () => {
    const { getCorridas } = await carregar({
      API_URL: undefined,
      NEXT_PUBLIC_API_URL: 'http://localhost:3002',
    });
    fetchMock.mockResolvedValue(resposta([]));

    await getCorridas();
    expect(fetchMock).toHaveBeenCalledWith('http://localhost:3002/runs');
  });

  it('falha com o status HTTP quando a API responde erro', async () => {
    const { getCorridas } = await carregar(env);
    fetchMock.mockResolvedValue(resposta({}, 500));

    await expect(getCorridas()).rejects.toThrow('GET /runs falhou (HTTP 500)');
  });

  it('falha sem endereço da API configurado', async () => {
    const { getRecordes } = await carregar({
      API_URL: undefined,
      NEXT_PUBLIC_API_URL: undefined,
    });

    await expect(getRecordes()).rejects.toThrow('API_URL não está definida');
    expect(fetchMock).not.toHaveBeenCalled();
  });
});

describe('com o mock (NEXT_PUBLIC_USE_MOCK=true)', () => {
  const env = { NEXT_PUBLIC_USE_MOCK: 'true' };

  it('não chama a API', async () => {
    const { getCorridas, getRecordes } = await carregar(env);
    await getCorridas();
    await getRecordes();
    expect(fetchMock).not.toHaveBeenCalled();
  });

  it('filtra as corridas do mock por labirinto', async () => {
    const { getCorridas } = await carregar(env);
    const todas = await getCorridas();
    const de8x4 = await getCorridas('8x4');

    expect(de8x4.length).toBeGreaterThan(0);
    expect(de8x4.length).toBeLessThan(todas.length);
    expect(de8x4.every((c) => c.labirinto === '8x4')).toBe(true);
  });

  it('calcula o recorde de cada labirinto com as corridas finalizadas', async () => {
    const { getCorridas, getRecordes } = await carregar(env);
    const corridas = await getCorridas();
    const recordes = await getRecordes();

    expect(recordes.map((r) => r.labirinto)).toEqual(['4x4', '8x4', '12x4']);
    for (const recorde of recordes) {
      const menor = Math.min(
        ...corridas
          .filter(
            (c) =>
              c.labirinto === recorde.labirinto && c.status === 'FINALIZADA',
          )
          .map((c) => c.duracaoSegundos ?? Infinity),
      );
      expect(recorde.tempoSegundos).toBe(menor);
    }
  });
});
