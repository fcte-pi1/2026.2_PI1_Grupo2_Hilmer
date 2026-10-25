import { render, screen, within } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { HistoricoDashboard } from '@/components/historico/HistoricoDashboard';
import type { CorridaResumo } from '@/lib/corridas';

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

// O rótulo também aparece em títulos de gráfico/tabela; o indicador é o <p>.
function indicador(rotulo: string) {
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
    render(<HistoricoDashboard corridas={corridas} />);
    expect(indicador('Corridas')).toHaveTextContent('5');
    expect(indicador('Taxa de sucesso')).toHaveTextContent('75%');
    expect(indicador('Melhor tempo')).toHaveTextContent('01:20');
    expect(indicador('Velocidade média')).toHaveTextContent('0.35 m/s');
    expect(indicador('Consumo médio de bateria')).toHaveTextContent('9.5%');
  });

  it('mostra traços quando não há corridas', () => {
    render(<HistoricoDashboard corridas={[]} />);
    expect(indicador('Corridas')).toHaveTextContent('0');
    expect(indicador('Taxa de sucesso')).toHaveTextContent('—');
    expect(indicador('Melhor tempo')).toHaveTextContent('—');
    expect(indicador('Velocidade média')).toHaveTextContent('—');
    expect(indicador('Consumo médio de bateria')).toHaveTextContent('—');
    expect(linhasDaTabela()).toHaveLength(0);
  });

  it('ignora corridas em andamento nas médias e no melhor tempo', () => {
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
    expect(indicador('Melhor tempo')).toHaveTextContent('—');
    expect(indicador('Taxa de sucesso')).toHaveTextContent('—');
  });

  it('não conta corrida abortada como melhor tempo', () => {
    render(
      <HistoricoDashboard
        corridas={[
          corrida({ id: 'a', duracaoSegundos: 100 }),
          corrida({ id: 'b', status: 'ABORTADA', duracaoSegundos: 10 }),
        ]}
      />,
    );
    expect(indicador('Melhor tempo')).toHaveTextContent('01:40');
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
  it('começa em Todos', () => {
    render(<HistoricoDashboard corridas={corridas} />);
    expect(screen.getByRole('button', { name: 'Todos' })).toHaveAttribute(
      'aria-pressed',
      'true',
    );
    expect(screen.getByRole('button', { name: '4x4' })).toHaveAttribute(
      'aria-pressed',
      'false',
    );
  });

  it('filtra por 4x4 e recalcula os indicadores', async () => {
    const user = userEvent.setup();
    render(<HistoricoDashboard corridas={corridas} />);

    await user.click(screen.getByRole('button', { name: '4x4' }));

    expect(screen.getByRole('button', { name: '4x4' })).toHaveAttribute(
      'aria-pressed',
      'true',
    );
    expect(linhasDaTabela()).toHaveLength(3);
    expect(indicador('Corridas')).toHaveTextContent('3');
    expect(indicador('Taxa de sucesso')).toHaveTextContent('67%');
    expect(indicador('Melhor tempo')).toHaveTextContent('01:20');
    expect(indicador('Velocidade média')).toHaveTextContent('0.30 m/s');
    expect(indicador('Consumo médio de bateria')).toHaveTextContent('6.0%');
  });

  it('filtra por 8x4 mantendo a numeração do histórico completo', async () => {
    const user = userEvent.setup();
    render(<HistoricoDashboard corridas={corridas} />);

    await user.click(screen.getByRole('button', { name: '8x4' }));

    const linhas = linhasDaTabela();
    expect(linhas).toHaveLength(2);
    expect(linhas[0]).toHaveTextContent('#5');
    expect(linhas[1]).toHaveTextContent('#4');
    expect(indicador('Melhor tempo')).toHaveTextContent('02:30');
    expect(indicador('Taxa de sucesso')).toHaveTextContent('100%');
  });

  it('mostra tabela vazia para um labirinto sem corridas', async () => {
    const user = userEvent.setup();
    render(<HistoricoDashboard corridas={corridas} />);

    await user.click(screen.getByRole('button', { name: '12x4' }));

    expect(linhasDaTabela()).toHaveLength(0);
    expect(indicador('Corridas')).toHaveTextContent('0');
    expect(indicador('Melhor tempo')).toHaveTextContent('—');
  });

  it('volta a mostrar tudo ao clicar em Todos', async () => {
    const user = userEvent.setup();
    render(<HistoricoDashboard corridas={corridas} />);

    await user.click(screen.getByRole('button', { name: '12x4' }));
    await user.click(screen.getByRole('button', { name: 'Todos' }));

    expect(linhasDaTabela()).toHaveLength(5);
  });
});
