import { isValidElement, type ReactElement } from 'react';
import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import HistoricoPage from '@/app/historico/page';
import HistoricoErro from '@/app/historico/error';
import { HistoricoDashboard } from '@/components/historico/HistoricoDashboard';
import { Connection } from '@/components/Connection';
import type { CorridaResumo } from '@/lib/corridas';
import { CASOS_DE_LARGURA, setViewport } from '../helpers/viewport';

// O Recharts não é exercitado aqui (gráficos vazios não o renderizam).
jest.mock('recharts', () => ({}));

// A API (lib/corridas) é simulada: nada aqui depende do backend ou do robô.
const mockGetCorridas = jest.fn();
const mockGetRecordes = jest.fn();
jest.mock('@/lib/corridas', () => ({
  ...jest.requireActual('@/lib/corridas'),
  getCorridas: (...args: unknown[]) => mockGetCorridas(...args),
  getRecordes: (...args: unknown[]) => mockGetRecordes(...args),
}));

type ElementoComFilhos = ReactElement<{
  children?: unknown;
  fallback?: ReactElement;
}>;

/**
 * A página é um componente assíncrono do servidor (Suspense + <Historico>).
 * O RTL não renderiza async components, então o teste abre a árvore devolvida:
 * acha o <Suspense> para o fallback e executa o filho para ver o que ele devolve.
 */
function abrirPagina(searchParams: Record<string, string> = {}) {
  const pagina = HistoricoPage({
    searchParams: Promise.resolve(searchParams),
  }) as ElementoComFilhos;
  const filhos = [pagina.props.children].flat();
  const suspense = filhos.find(
    (f) => isValidElement(f) && (f as ElementoComFilhos).props.fallback,
  ) as ElementoComFilhos;
  const filho = suspense.props.children as ReactElement<unknown>;
  const executarFilho = () =>
    (filho.type as (props: unknown) => Promise<ReactElement>)(filho.props);
  return { pagina, suspense, executarFilho };
}

beforeEach(() => {
  mockGetCorridas.mockResolvedValue([]);
  mockGetRecordes.mockResolvedValue([]);
});

describe.each(CASOS_DE_LARGURA)('Fallbacks em %s (%i px)', (_n, largura) => {
  beforeEach(() => setViewport(largura));

  describe('carregamento', () => {
    it('a página mostra "Carregando corridas…" enquanto a API responde', () => {
      const { suspense } = abrirPagina();
      render(suspense.props.fallback as ReactElement);
      expect(screen.getByText('Carregando corridas…')).toBeInTheDocument();
    });
  });

  describe('histórico vazio', () => {
    it('lista vazia da API vira a mensagem "Nenhuma corrida registrada."', async () => {
      const { executarFilho } = abrirPagina();
      render(await executarFilho());
      expect(
        screen.getByText('Nenhuma corrida registrada.'),
      ).toBeInTheDocument();
      expect(screen.getAllByText('Sem corridas para exibir')).toHaveLength(3);
    });

    it('com filtro, a mensagem cita o labirinto', async () => {
      const { executarFilho } = abrirPagina({ labirinto: '8x4' });
      render(await executarFilho());
      expect(mockGetCorridas).toHaveBeenCalledWith('8x4');
      expect(
        screen.getByText('Nenhuma corrida registrada para o labirinto 8x4.'),
      ).toBeInTheDocument();
    });

    it('os indicadores mostram traço, sem NaN', () => {
      render(<HistoricoDashboard corridas={[]} />);
      expect(screen.queryByText(/NaN/)).not.toBeInTheDocument();
      expect(screen.getAllByText('—').length).toBeGreaterThanOrEqual(4);
    });
  });

  describe('erro', () => {
    it('falha da API chega ao error boundary (a página rejeita)', async () => {
      mockGetCorridas.mockRejectedValue(
        new Error('GET /runs falhou (HTTP 500)'),
      );
      const { executarFilho } = abrirPagina();
      await expect(executarFilho()).rejects.toThrow('HTTP 500');
    });

    it('API fora do ar: a tela de erro mostra a mensagem e o botão de nova tentativa', async () => {
      const retry = jest.fn();
      const user = userEvent.setup();
      render(<HistoricoErro error={new Error('fetch failed')} retry={retry} />);
      expect(
        screen.getByText('Não foi possível carregar o histórico: fetch failed'),
      ).toBeInTheDocument();
      await user.click(
        screen.getByRole('button', { name: 'Tentar novamente' }),
      );
      expect(retry).toHaveBeenCalledTimes(1);
    });
  });

  describe('gráficos sem dados', () => {
    it('só com corrida em andamento, os gráficos de tempo e consumo ficam vazios', () => {
      const emAndamento = {
        id: 'x',
        labirinto: '4x4',
        status: 'EM_ANDAMENTO',
        iniciadaEm: '2026-10-01T12:00:00Z',
        finalizadaEm: null,
        duracaoSegundos: null,
        velocidadeMedia: 0.1,
        bateriaInicial: 100,
        bateriaFinal: 99,
      } as CorridaResumo;
      render(<HistoricoDashboard corridas={[emAndamento]} />);
      expect(screen.getAllByText('Sem corridas para exibir')).toHaveLength(3);
    });
  });

  describe('indicador de conexão', () => {
    it('mostra "Conectado" com a bolinha verde', () => {
      const { container } = render(<Connection connected />);
      expect(screen.getByText('Conectado')).toBeInTheDocument();
      expect(container.querySelector('.bg-green-400')).toBeInTheDocument();
    });

    it('mostra "Desconectado" com a bolinha vermelha', () => {
      const { container } = render(<Connection connected={false} />);
      expect(screen.getByText('Desconectado')).toBeInTheDocument();
      expect(container.querySelector('.bg-red-500')).toBeInTheDocument();
    });
  });
});

// Divergências entre a issue #309 e o código atual (ver docs/7.4, seção 6.7):
it.todo(
  '404 em GET /runs deveria virar histórico vazio (hoje cai na tela de erro)',
);
it.todo('indicador "Reconectando…" (hoje só existe Conectado/Desconectado)');
