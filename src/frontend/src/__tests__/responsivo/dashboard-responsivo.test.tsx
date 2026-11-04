import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import Monitoramento from '@/app/page';
import { Header } from '@/components/Header';
import { MazeLegend } from '@/components/MazeLegend';
import { PainelResultados } from '@/components/Results';
import {
  CASOS_DE_LARGURA,
  classeEfetiva,
  setViewport,
} from '../helpers/viewport';

jest.mock('next/navigation', () => ({ usePathname: () => '/' }));

// O jsdom não carrega o CSS do Tailwind. Por isso o layout é conferido pelas
// classes responsivas, resolvidas na largura simulada (classeEfetiva).

describe.each(CASOS_DE_LARGURA)('Dashboard em %s (%i px)', (_nome, largura) => {
  beforeEach(() => setViewport(largura));

  it('mostra legenda, mapa e painel de resultados', () => {
    const { container } = render(<Monitoramento />);
    expect(screen.getByText('Legenda & Controle')).toBeInTheDocument();
    expect(screen.getByText('Mapa do Labirinto')).toBeInTheDocument();
    expect(container.querySelector('svg')).toBeInTheDocument();
    expect(
      screen.getByRole('heading', { name: 'Resultados' }),
    ).toBeInTheDocument();
  });

  it('empilha em coluna até o breakpoint xl e vira linha a partir dele', () => {
    const { container } = render(<Monitoramento />);
    const raiz = container.firstElementChild as HTMLElement;
    const esperado = largura >= 1280 ? 'flex-row' : 'flex-col';
    expect(classeEfetiva(raiz, ['flex-col', 'flex-row'], largura)).toBe(
      esperado,
    );
  });

  it('a legenda ocupa a largura toda em tela estreita e fica fixa em lg+', () => {
    render(<MazeLegend currentDimension="4x4" />);
    const legenda = screen.getByText('Legenda & Controle').closest('aside')!;
    const esperado = largura >= 1024 ? 'w-64' : 'w-full';
    expect(classeEfetiva(legenda, ['w-full', 'w-64'], largura)).toBe(esperado);
  });

  it('o painel de resultados ocupa a largura toda em tela estreita e fica fixo em lg+', () => {
    render(
      <PainelResultados>
        <p>x</p>
      </PainelResultados>,
    );
    const painel = screen.getByRole('heading', { name: 'Resultados' })
      .parentElement as HTMLElement;
    const esperado = largura >= 1024 ? 'w-154' : 'w-full';
    expect(classeEfetiva(painel, ['w-full', 'w-154'], largura)).toBe(esperado);
  });

  it('os botões de labirinto 4x4, 8x4 e 12x4 continuam acessíveis e funcionam', async () => {
    const user = userEvent.setup();
    render(<Monitoramento />);
    for (const dim of ['8x4', '12x4', '4x4']) {
      const botao = screen.getByRole('button', { name: dim });
      expect(botao).toBeVisible();
      await user.click(botao);
      expect(screen.getByText(`Tipo de labirinto: ${dim}`)).toBeInTheDocument();
    }
  });

  it('o botão de reproduzir a corrida continua disponível', () => {
    render(<Monitoramento />);
    expect(
      screen.getByRole('button', { name: /Reproduzir Corrida/ }),
    ).toBeInTheDocument();
  });
});

describe.each(CASOS_DE_LARGURA)('Header em %s (%i px)', (_nome, largura) => {
  beforeEach(() => setViewport(largura));

  it('permite quebrar linha (flex-wrap) e mantém navegação e conexão', () => {
    const { container } = render(<Header />);
    expect(container.querySelector('header > div')).toHaveClass('flex-wrap');
    expect(screen.getByRole('link', { name: 'Dashboard' })).toBeVisible();
    expect(screen.getByRole('link', { name: 'Histórico' })).toBeVisible();
    // Sem TelemetryProvider o contexto padrão é "desconectado".
    expect(screen.getByText(/Desconectado/)).toBeInTheDocument();
  });
});
