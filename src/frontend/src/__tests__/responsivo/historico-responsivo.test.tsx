import { render, screen, within } from '@testing-library/react';
import { HistoricoDashboard } from '@/components/historico/HistoricoDashboard';
import type { CorridaResumo, RecordeLabirinto } from '@/lib/corridas';
import {
  CASOS_DE_LARGURA,
  classeEfetiva,
  setViewport,
} from '../helpers/viewport';

// Os gráficos têm testes próprios (graficos-responsivos e GraficoCorridas).
jest.mock('@/components/historico/GraficoCorridas', () => {
  const React = jest.requireActual('react');
  return {
    GraficoCorridas: ({ titulo }: { titulo: string }) =>
      React.createElement('div', { 'data-testid': `grafico-${titulo}` }),
  };
});

function corrida(parcial: Partial<CorridaResumo> & { id: string }) {
  return {
    labirinto: '4x4',
    status: 'FINALIZADA',
    iniciadaEm: '2026-10-01T12:00:00Z',
    finalizadaEm: '2026-10-01T12:02:00Z',
    duracaoSegundos: 100,
    velocidadeMedia: 0.2,
    bateriaInicial: 100,
    bateriaFinal: 90,
    ...parcial,
  } as CorridaResumo;
}

const corridas = [
  corrida({ id: 'a', iniciadaEm: '2026-10-01T12:00:00Z' }),
  corrida({
    id: 'b',
    iniciadaEm: '2026-10-02T12:00:00Z',
    duracaoSegundos: 80,
    labirinto: '8x4',
  }),
];
const recordes: RecordeLabirinto[] = [
  {
    labirinto: '8x4',
    labirintoId: 'lab-8x4',
    corridaId: 'b',
    status: 'FINALIZADA',
    tempoSegundos: 80,
    iniciadaEm: '2026-10-02T12:00:00Z',
    finalizadaEm: '2026-10-02T12:01:20Z',
  },
];

const COLUNAS = [
  'Corrida',
  'Data',
  'Labirinto',
  'Status',
  'Tempo',
  'Vel. média',
  'Bateria',
];

describe.each(CASOS_DE_LARGURA)('Histórico em %s (%i px)', (_nome, largura) => {
  beforeEach(() => setViewport(largura));

  it('a tabela fica dentro de um contêiner com rolagem horizontal', () => {
    render(<HistoricoDashboard corridas={corridas} recordes={recordes} />);
    const tabela = screen.getByRole('table');
    expect(tabela.parentElement).toHaveClass('overflow-x-auto');
  });

  it('mantém todas as colunas e todas as corridas', () => {
    render(<HistoricoDashboard corridas={corridas} recordes={recordes} />);
    const tabela = screen.getByRole('table');
    const cabecalhos = within(tabela)
      .getAllByRole('columnheader')
      .map((th) => th.textContent);
    expect(cabecalhos).toEqual(COLUNAS);
    expect(within(tabela).getAllByRole('row')).toHaveLength(
      1 + corridas.length,
    );
  });

  it('indicadores: 2 colunas até lg e 5 colunas a partir de lg', () => {
    render(<HistoricoDashboard corridas={corridas} recordes={recordes} />);
    const grade = screen.getByText('Taxa de sucesso').closest('div')!
      .parentElement as HTMLElement;
    const esperado = largura >= 1024 ? 'grid-cols-5' : 'grid-cols-2';
    expect(classeEfetiva(grade, ['grid-cols-2', 'grid-cols-5'], largura)).toBe(
      esperado,
    );
  });

  it('gráficos: 1 coluna até lg e 3 colunas a partir de xl', () => {
    render(<HistoricoDashboard corridas={corridas} recordes={recordes} />);
    const grade = screen.getByTestId('grafico-Tempo de conclusão')
      .parentElement as HTMLElement;
    expect(grade).toContainElement(
      screen.getByTestId('grafico-Consumo de bateria'),
    );
    const esperado = largura >= 1280 ? 'grid-cols-3' : 'grid-cols-1';
    expect(classeEfetiva(grade, ['grid-cols-1', 'grid-cols-3'], largura)).toBe(
      esperado,
    );
  });

  it('filtros por labirinto continuam visíveis, com links certos e quebra de linha', () => {
    render(<HistoricoDashboard corridas={corridas} recordes={recordes} />);
    const todos = screen.getByRole('link', { name: 'Todos' });
    expect(todos.parentElement).toHaveClass('flex-wrap');
    expect(todos).toHaveAttribute('href', '/historico');
    for (const lab of ['4x4', '8x4', '12x4']) {
      const link = screen.getByRole('link', { name: lab });
      expect(link).toBeVisible();
      expect(link).toHaveAttribute('href', `/historico?labirinto=${lab}`);
    }
  });

  // O código atual não destaca a melhor corrida na tabela (o recorde só
  // aparece no indicador). Ver divergência D-08 na documentação.
  it.todo('destaca o melhor tempo na tabela (ainda não implementado)');
});
