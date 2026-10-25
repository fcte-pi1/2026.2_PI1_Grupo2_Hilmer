import { render, screen } from '@testing-library/react';
import {
  GraficoCorridas,
  type PontoGrafico,
} from '@/components/historico/GraficoCorridas';

// O Recharts depende de medidas reais de layout, que o jsdom não tem. Aqui os
// componentes dele viram stubs simples e o teste confere o que o nosso
// componente decide: título, tipo de gráfico, dados, formatação e tooltip.
jest.mock('recharts', () => {
  const React = jest.requireActual('react');
  const h = React.createElement;
  const Vazio = () => null;
  return {
    ResponsiveContainer: ({ children }: { children: unknown }) =>
      h('div', { 'data-testid': 'responsive' }, children),
    BarChart: ({ data, children }: { data: unknown[]; children: unknown }) =>
      h(
        'div',
        { 'data-testid': 'grafico-barra', 'data-pontos': data.length },
        children,
      ),
    LineChart: ({ data, children }: { data: unknown[]; children: unknown }) =>
      h(
        'div',
        { 'data-testid': 'grafico-linha', 'data-pontos': data.length },
        children,
      ),
    Bar: Vazio,
    Line: Vazio,
    CartesianGrid: Vazio,
    XAxis: Vazio,
    YAxis: ({ tickFormatter }: { tickFormatter: (v: number) => string }) =>
      h('span', { 'data-testid': 'tick-y' }, tickFormatter(90)),
    Tooltip: ({
      content,
    }: {
      content: (props: Record<string, unknown>) => unknown;
    }) =>
      h(
        'div',
        { 'data-testid': 'tooltip' },
        content({
          active: true,
          payload: [
            { payload: { corrida: '#7', labirinto: '8x4', valor: 90 } },
          ],
        }),
      ),
  };
});

const dados: PontoGrafico[] = [
  { corrida: '#1', labirinto: '4x4', valor: 60 },
  { corrida: '#2', labirinto: '4x4', valor: 90 },
];
const formatar = (v: number) => `${v} u`;

describe('GraficoCorridas', () => {
  it('mostra o título', () => {
    render(
      <GraficoCorridas
        titulo="Tempo de conclusão"
        dados={dados}
        tipo="barra"
        cor="#3987e5"
        formatarValor={formatar}
      />,
    );
    expect(
      screen.getByRole('heading', { name: 'Tempo de conclusão' }),
    ).toBeInTheDocument();
  });

  it('mostra mensagem e não desenha gráfico sem dados', () => {
    render(
      <GraficoCorridas
        titulo="Vazio"
        dados={[]}
        tipo="barra"
        cor="#fff"
        formatarValor={formatar}
      />,
    );
    expect(screen.getByText('Sem corridas para exibir')).toBeInTheDocument();
    expect(screen.queryByTestId('responsive')).not.toBeInTheDocument();
  });

  it('desenha gráfico de barras com os pontos recebidos', () => {
    render(
      <GraficoCorridas
        titulo="Barras"
        dados={dados}
        tipo="barra"
        cor="#fff"
        formatarValor={formatar}
      />,
    );
    expect(screen.getByTestId('grafico-barra')).toHaveAttribute(
      'data-pontos',
      '2',
    );
    expect(screen.queryByTestId('grafico-linha')).not.toBeInTheDocument();
  });

  it('desenha gráfico de linha quando o tipo é linha', () => {
    render(
      <GraficoCorridas
        titulo="Linha"
        dados={dados}
        tipo="linha"
        cor="#fff"
        formatarValor={formatar}
      />,
    );
    expect(screen.getByTestId('grafico-linha')).toHaveAttribute(
      'data-pontos',
      '2',
    );
    expect(screen.queryByTestId('grafico-barra')).not.toBeInTheDocument();
  });

  it('formata o eixo Y com formatarValor', () => {
    render(
      <GraficoCorridas
        titulo="Eixo"
        dados={dados}
        tipo="barra"
        cor="#fff"
        formatarValor={formatar}
      />,
    );
    expect(screen.getByTestId('tick-y')).toHaveTextContent('90 u');
  });

  it('mostra corrida, labirinto e valor formatado na dica', () => {
    render(
      <GraficoCorridas
        titulo="Dica"
        dados={dados}
        tipo="linha"
        cor="#fff"
        formatarValor={formatar}
      />,
    );
    const dica = screen.getByTestId('tooltip');
    expect(dica).toHaveTextContent('Corrida #7 · 8x4');
    expect(dica).toHaveTextContent('90 u');
  });
});
