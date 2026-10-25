import { render, screen, within } from '@testing-library/react';
import { HistoricoDashboard } from '@/components/historico/HistoricoDashboard';
import type { CorridaResumo, RecordeLabirinto } from '@/lib/corridas';

// Os gráficos têm teste próprio; aqui só interessa quantos pontos recebem.
jest.mock('@/components/historico/GraficoCorridas', () => {
  const React = jest.requireActual('react');
  return {
    GraficoCorridas: ({
      titulo,
      dados,
    }: {
      titulo: string;
      dados: unknown[];
    }) =>
      React.createElement('div', {
        'data-testid': `grafico-${titulo}`,
        'data-pontos': dados.length,
      }),
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

// Ordem cronológica: c1 (#1), c2 (#2), c3 (#3), c4 (#4), c5 (#5).
const corridas: CorridaResumo[] = [
  corrida({ id: 'c1', iniciadaEm: '2026-10-01T12:00:00Z' }),
  corrida({
    id: 'c2',
    iniciadaEm: '2026-10-02T12:00:00Z',
    duracaoSegundos: 80,
    velocidadeMedia: 0.4,
    bateriaInicial: 90,
    bateriaFinal: 84,
  }),
  corrida({
    id: 'c3',
    iniciadaEm: '2026-10-03T12:00:00Z',
    status: 'ABORTADA',
    duracaoSegundos: 30,
    velocidadeMedia: 0.3,
    bateriaInicial: 84,
    bateriaFinal: 82,
  }),
  corrida({
    id: 'c4',
    labirinto: '8x4',
    iniciadaEm: '2026-10-04T12:00:00Z',
    duracaoSegundos: 150,
    velocidadeMedia: 0.5,
    bateriaInicial: 100,
    bateriaFinal: 80,
  }),
  corrida({
    id: 'c5',
    labirinto: '8x4',
    iniciadaEm: '2026-10-05T12:00:00Z',
    status: 'EM_ANDAMENTO',
    finalizadaEm: null,
    duracaoSegundos: null,
    velocidadeMedia: 0.1,
    bateriaInicial: 80,
    bateriaFinal: 79,
  }),
];

// Formato de GET /runs/best: o menor tempo finalizado de cada labirinto.
const recordes: RecordeLabirinto[] = [
  {
    labirinto: '4x4',
    labirintoId: 'lab-4x4',
    corridaId: 'c2',
    status: 'FINALIZADA',
    tempoSegundos: 80,
    iniciadaEm: '2026-10-02T12:00:00Z',
    finalizadaEm: '2026-10-02T12:01:20Z',
  },
  {
    labirinto: '8x4',
    labirintoId: 'lab-8x4',
    corridaId: 'c4',
    status: 'FINALIZADA',
    tempoSegundos: 150,
    iniciadaEm: '2026-10-04T12:00:00Z',
    finalizadaEm: '2026-10-04T12:02:30Z',
  },
];

// O backend devolve as corridas já filtradas (GET /runs?mazeId=).
const doLabirinto = (labirinto: string) =>
  corridas.filter((c) => c.labirinto === labirinto);

// O rótulo também aparece em títulos de gráfico/tabela; o indicador é o <p>.
function indicador(rotulo: string | RegExp) {
  const p = screen
    .getAllByText(rotulo)
    .find((el) => el.tagName === 'P') as HTMLElement;
  return p.parentElement as HTMLElement;
}

function linhasDaTabela() {
  return within(screen.getByRole('table')).getAllByRole('row').slice(1);
}

describe('HistoricoDashboard: indicadores', () => {
  it('calcula os indicadores de todas as corridas', () => {
    render(<HistoricoDashboard corridas={corridas} recordes={recordes} />);
    expect(indicador('Corridas')).toHaveTextContent('5');
    expect(indicador('Taxa de sucesso')).toHaveTextContent('75%');
    expect(indicador('Velocidade média')).toHaveTextContent('0.35 m/s');
    expect(indicador('Consumo médio de bateria')).toHaveTextContent('9.5%');
  });

  it('mostra traços quando não há corridas', () => {
    render(<HistoricoDashboard corridas={[]} />);
    expect(indicador('Corridas')).toHaveTextContent('0');
    expect(indicador('Taxa de sucesso')).toHaveTextContent('—');
    expect(indicador('Recorde')).toHaveTextContent('—');
    expect(indicador('Velocidade média')).toHaveTextContent('—');
    expect(indicador('Consumo médio de bateria')).toHaveTextContent('—');
  });

  it('ignora corridas em andamento nas médias e na taxa de sucesso', () => {
    render(
      <HistoricoDashboard
        corridas={[
          corrida({
            id: 'x',
            status: 'EM_ANDAMENTO',
            duracaoSegundos: null,
            finalizadaEm: null,
          }),
        ]}
      />,
    );
    expect(indicador('Corridas')).toHaveTextContent('1');
    expect(indicador('Taxa de sucesso')).toHaveTextContent('—');
    expect(indicador('Velocidade média')).toHaveTextContent('—');
  });

  it('conta CONCLUIDA como sucesso e FALHA como insucesso', () => {
    render(
      <HistoricoDashboard
        corridas={[
          corrida({ id: 'a', status: 'CONCLUIDA' }),
          corrida({ id: 'b', status: 'FALHA' }),
        ]}
      />,
    );
    expect(indicador('Taxa de sucesso')).toHaveTextContent('50%');
  });

  it('ignora no consumo médio as corridas sem telemetria de bateria', () => {
    render(
      <HistoricoDashboard
        corridas={[
          corrida({ id: 'a', bateriaInicial: 100, bateriaFinal: 90 }),
          corrida({ id: 'b', bateriaInicial: null, bateriaFinal: null }),
        ]}
      />,
    );
    expect(indicador('Consumo médio de bateria')).toHaveTextContent('10.0%');
  });
});

describe('HistoricoDashboard: recorde', () => {
  it('em Todos, mostra o menor tempo entre os recordes e o labirinto dele', () => {
    render(<HistoricoDashboard corridas={corridas} recordes={recordes} />);
    expect(indicador('Recorde (4x4)')).toHaveTextContent('01:20');
  });

  it('com filtro, mostra o recorde do labirinto filtrado', () => {
    render(
      <HistoricoDashboard
        corridas={doLabirinto('8x4')}
        recordes={recordes}
        filtro="8x4"
      />,
    );
    expect(indicador('Recorde')).toHaveTextContent('02:30');
  });

  it('mostra traço quando o labirinto filtrado não tem recorde', () => {
    render(
      <HistoricoDashboard corridas={[]} recordes={recordes} filtro="12x4" />,
    );
    expect(indicador('Recorde')).toHaveTextContent('—');
  });

  it('usa o recorde da API, e não o menor tempo das corridas exibidas', () => {
    render(
      <HistoricoDashboard
        corridas={[corrida({ id: 'a', duracaoSegundos: 10 })]}
        recordes={[{ ...recordes[0], tempoSegundos: 65 }]}
        filtro="4x4"
      />,
    );
    expect(indicador('Recorde')).toHaveTextContent('01:05');
  });
});

describe('HistoricoDashboard: tabela', () => {
  it('lista a corrida mais recente primeiro', () => {
    render(<HistoricoDashboard corridas={corridas} />);
    const linhas = linhasDaTabela();
    expect(linhas).toHaveLength(5);
    expect(linhas[0]).toHaveTextContent('#5');
    expect(linhas[4]).toHaveTextContent('#1');
  });

  it('numera pela ordem cronológica mesmo com a lista desordenada', () => {
    render(<HistoricoDashboard corridas={[...corridas].reverse()} />);
    expect(linhasDaTabela()[0]).toHaveTextContent('#5');
  });

  it('mostra status, tempo, velocidade e bateria de cada corrida', () => {
    render(<HistoricoDashboard corridas={corridas} />);
    const [c5, , c3] = linhasDaTabela();

    expect(c5).toHaveTextContent('Em andamento');
    expect(c5).toHaveTextContent('—');
    expect(c5).toHaveTextContent('0.10 m/s');
    expect(c5).toHaveTextContent('80% → 79%');

    expect(c3).toHaveTextContent('Abortada');
    expect(c3).toHaveTextContent('00:30');
    expect(c3).toHaveTextContent('84% → 82%');
  });

  it('mostra Finalizada para corridas concluídas', () => {
    render(<HistoricoDashboard corridas={corridas} />);
    expect(linhasDaTabela()[3]).toHaveTextContent('Finalizada');
  });

  it('mostra os rótulos dos status usados pelo seed', () => {
    render(
      <HistoricoDashboard
        corridas={[
          corrida({ id: 'a', status: 'CONCLUIDA' }),
          corrida({
            id: 'b',
            status: 'FALHA',
            iniciadaEm: '2026-10-02T12:00:00Z',
          }),
        ]}
      />,
    );
    const [falha, concluida] = linhasDaTabela();
    expect(falha).toHaveTextContent('Falha');
    expect(concluida).toHaveTextContent('Concluída');
  });

  it('mostra traço na bateria de corrida sem telemetria', () => {
    render(
      <HistoricoDashboard
        corridas={[
          corrida({ id: 'a', bateriaInicial: null, bateriaFinal: null }),
        ]}
      />,
    );
    expect(linhasDaTabela()[0]).not.toHaveTextContent('%');
  });

  it('avisa quando não há corridas', () => {
    render(<HistoricoDashboard corridas={[]} />);
    const linhas = linhasDaTabela();
    expect(linhas).toHaveLength(1);
    expect(linhas[0]).toHaveTextContent('Nenhuma corrida registrada.');
  });

  it('avisa quando o labirinto filtrado não tem corridas', () => {
    render(<HistoricoDashboard corridas={[]} filtro="12x4" />);
    expect(linhasDaTabela()[0]).toHaveTextContent(
      'Nenhuma corrida registrada para o labirinto 12x4.',
    );
  });
});

describe('HistoricoDashboard: gráficos', () => {
  it('manda finalizadas ao gráfico de tempo e encerradas aos demais', () => {
    render(<HistoricoDashboard corridas={corridas} />);
    expect(screen.getByTestId('grafico-Tempo de conclusão')).toHaveAttribute(
      'data-pontos',
      '3',
    );
    expect(screen.getByTestId('grafico-Velocidade média')).toHaveAttribute(
      'data-pontos',
      '4',
    );
    expect(screen.getByTestId('grafico-Consumo de bateria')).toHaveAttribute(
      'data-pontos',
      '4',
    );
  });
});

describe('HistoricoDashboard: filtro por labirinto', () => {
  it('cada filtro é um link que refaz a consulta pela URL', () => {
    render(<HistoricoDashboard corridas={corridas} />);
    expect(screen.getByRole('link', { name: 'Todos' })).toHaveAttribute(
      'href',
      '/historico',
    );
    for (const labirinto of ['4x4', '8x4', '12x4']) {
      expect(screen.getByRole('link', { name: labirinto })).toHaveAttribute(
        'href',
        `/historico?labirinto=${labirinto}`,
      );
    }
  });

  it('começa em Todos', () => {
    render(<HistoricoDashboard corridas={corridas} />);
    expect(screen.getByRole('link', { name: 'Todos' })).toHaveAttribute(
      'aria-current',
      'page',
    );
    expect(screen.getByRole('link', { name: '4x4' })).not.toHaveAttribute(
      'aria-current',
    );
  });

  it('marca o filtro ativo e calcula os indicadores das corridas recebidas', () => {
    render(
      <HistoricoDashboard
        corridas={doLabirinto('4x4')}
        recordes={recordes}
        filtro="4x4"
      />,
    );

    expect(screen.getByRole('link', { name: '4x4' })).toHaveAttribute(
      'aria-current',
      'page',
    );
    expect(screen.getByRole('link', { name: 'Todos' })).not.toHaveAttribute(
      'aria-current',
    );
    expect(linhasDaTabela()).toHaveLength(3);
    expect(indicador('Corridas')).toHaveTextContent('3');
    expect(indicador('Taxa de sucesso')).toHaveTextContent('67%');
    expect(indicador('Recorde')).toHaveTextContent('01:20');
    expect(indicador('Velocidade média')).toHaveTextContent('0.30 m/s');
    expect(indicador('Consumo médio de bateria')).toHaveTextContent('6.0%');
  });

  it('numera as corridas dentro do labirinto filtrado', () => {
    render(
      <HistoricoDashboard
        corridas={doLabirinto('8x4')}
        recordes={recordes}
        filtro="8x4"
      />,
    );

    const linhas = linhasDaTabela();
    expect(linhas).toHaveLength(2);
    expect(linhas[0]).toHaveTextContent('#2');
    expect(linhas[1]).toHaveTextContent('#1');
    expect(indicador('Taxa de sucesso')).toHaveTextContent('100%');
  });
});
