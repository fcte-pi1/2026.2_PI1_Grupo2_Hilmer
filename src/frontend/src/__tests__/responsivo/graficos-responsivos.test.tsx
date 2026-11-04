import { render, screen } from '@testing-library/react';
import {
  GraficoCorridas,
  type PontoGrafico,
} from '@/components/historico/GraficoCorridas';
import {
  CASOS_DE_LARGURA,
  ResizeObserverFalso,
  setViewport,
} from '../helpers/viewport';

// Aqui o Recharts é o de verdade (sem stub), só com o ResizeObserver falso
// instalado em jest.setup.ts. O jsdom não tem layout, então o que se confere é
// a estrutura responsiva (ResponsiveContainer a 100%) e os estados sem dados.
const dados: PontoGrafico[] = [
  { corrida: '#1', labirinto: '4x4', valor: 60 },
  { corrida: '#2', labirinto: '4x4', valor: 90 },
];

beforeEach(() => {
  ResizeObserverFalso.instancias = [];
  // O Recharts avisa no console quando mede 0 px (jsdom). Não é falha.
  jest.spyOn(console, 'warn').mockImplementation(() => {});
  jest.spyOn(console, 'error').mockImplementation(() => {});
});
afterEach(() => jest.restoreAllMocks());

describe.each(CASOS_DE_LARGURA)('Gráficos em %s (%i px)', (_nome, largura) => {
  beforeEach(() => setViewport(largura));

  it.each(['barra', 'linha'] as const)(
    'gráfico de %s renderiza dentro do contêiner responsivo',
    (tipo) => {
      const { container } = render(
        <GraficoCorridas
          titulo="Tempo"
          dados={dados}
          tipo={tipo}
          cor="#3987e5"
          formatarValor={(v) => `${v}s`}
        />,
      );
      const responsivo = container.querySelector(
        '.recharts-responsive-container',
      ) as HTMLElement;
      expect(responsivo).toBeInTheDocument();
      expect(responsivo).toHaveStyle({ width: '100%', height: '100%' });
    },
  );

  it('observa o redimensionamento do contêiner (ResizeObserver)', () => {
    render(
      <GraficoCorridas
        titulo="Tempo"
        dados={dados}
        tipo="barra"
        cor="#3987e5"
        formatarValor={(v) => `${v}s`}
      />,
    );
    expect(ResizeObserverFalso.instancias.length).toBeGreaterThan(0);
  });

  it('sem dados mostra a mensagem e não cria contêiner responsivo', () => {
    const { container } = render(
      <GraficoCorridas
        titulo="Vazio"
        dados={[]}
        tipo="linha"
        cor="#fff"
        formatarValor={String}
      />,
    );
    expect(screen.getByText('Sem corridas para exibir')).toBeInTheDocument();
    expect(
      container.querySelector('.recharts-responsive-container'),
    ).not.toBeInTheDocument();
  });
});
