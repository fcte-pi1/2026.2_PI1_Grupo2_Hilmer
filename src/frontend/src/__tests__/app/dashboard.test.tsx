import { act, render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import Monitoramento from '@/app/page';

// Dados esperados de cada labirinto em lib/mock-data.ts (estado parado = fim do trajeto).
const CASOS = [
  {
    dim: '4x4',
    speed: '2.5',
    tempo: '01:00',
    consumo: '12',
    pos: '(3, 3)',
    passos: '7',
  },
  {
    dim: '8x4',
    speed: '3.1',
    tempo: '01:35',
    consumo: '18',
    pos: '(7, 3)',
    passos: '11',
  },
  {
    dim: '12x4',
    speed: '3.8',
    tempo: '02:20',
    consumo: '26',
    pos: '(11, 3)',
    passos: '17',
  },
] as const;

describe('Dashboard: troca de labirinto', () => {
  it('abre no labirinto 4x4 com o painel de resultados', () => {
    render(<Monitoramento />);
    expect(screen.getByText('Tipo de labirinto: 4x4')).toBeInTheDocument();
    expect(screen.getByText('Mapa do Labirinto')).toBeInTheDocument();
    expect(screen.getByText('Legenda & Controle')).toBeInTheDocument();
    expect(
      screen.getByRole('heading', { name: 'Resultados' }),
    ).toBeInTheDocument();
  });

  it.each(CASOS)(
    'ao clicar em $dim, mapa e resultados mudam juntos',
    async ({ dim, speed, tempo, consumo, pos, passos }) => {
      const user = userEvent.setup();
      const { container } = render(<Monitoramento />);

      await user.click(screen.getByRole('button', { name: dim }));

      expect(screen.getByText(`Tipo de labirinto: ${dim}`)).toBeInTheDocument();
      expect(
        screen.getByText(`Velocidade média: ${speed} m/s`),
      ).toBeInTheDocument();
      expect(
        screen.getByText(`Tempo de conclusão: ${tempo}`),
      ).toBeInTheDocument();
      expect(
        screen.getByText(`Consumo de bateria: ${consumo}%`),
      ).toBeInTheDocument();
      expect(screen.getByText('Desafio cumprido: S')).toBeInTheDocument();
      expect(screen.getByText(pos)).toBeInTheDocument();
      expect(screen.getByText('Passos no trajeto:')).toHaveTextContent(
        `Passos no trajeto: ${passos}`,
      );
      expect(container.querySelector('svg')).toBeInTheDocument();
    },
  );

  it('permite percorrer os três labirintos em sequência sem quebrar', async () => {
    const user = userEvent.setup();
    render(<Monitoramento />);

    for (const dim of ['8x4', '12x4', '4x4', '12x4', '8x4']) {
      await user.click(screen.getByRole('button', { name: dim }));
      expect(screen.getByText(`Tipo de labirinto: ${dim}`)).toBeInTheDocument();
    }
  });
});

describe('Dashboard: simulação da corrida', () => {
  beforeEach(() => jest.useFakeTimers());
  afterEach(() => jest.useRealTimers());

  it('anima o trajeto passo a passo e termina sozinha', async () => {
    const user = userEvent.setup({ advanceTimers: jest.advanceTimersByTime });
    render(<Monitoramento />);

    await user.click(
      screen.getByRole('button', { name: /Reproduzir Corrida/ }),
    );
    expect(
      screen.getByRole('button', { name: /Pausar Simulação/ }),
    ).toBeInTheDocument();
    expect(screen.getByText('Passos no trajeto:')).toHaveTextContent(
      'Passos no trajeto: 1',
    );

    act(() => {
      jest.advanceTimersByTime(450);
    });
    expect(screen.getByText('Passos no trajeto:')).toHaveTextContent(
      'Passos no trajeto: 2',
    );

    act(() => {
      jest.advanceTimersByTime(450 * 10);
    });
    expect(
      screen.getByRole('button', { name: /Reproduzir Corrida/ }),
    ).toBeInTheDocument();
    expect(screen.getByText('Passos no trajeto:')).toHaveTextContent(
      'Passos no trajeto: 7',
    );
  });

  it('pausa a simulação ao clicar de novo', async () => {
    const user = userEvent.setup({ advanceTimers: jest.advanceTimersByTime });
    render(<Monitoramento />);

    await user.click(
      screen.getByRole('button', { name: /Reproduzir Corrida/ }),
    );
    await user.click(screen.getByRole('button', { name: /Pausar Simulação/ }));

    expect(
      screen.getByRole('button', { name: /Reproduzir Corrida/ }),
    ).toBeInTheDocument();
  });

  it('trocar de labirinto durante a simulação interrompe a animação', async () => {
    const user = userEvent.setup({ advanceTimers: jest.advanceTimersByTime });
    render(<Monitoramento />);

    await user.click(
      screen.getByRole('button', { name: /Reproduzir Corrida/ }),
    );
    await user.click(screen.getByRole('button', { name: '8x4' }));

    expect(
      screen.getByRole('button', { name: /Reproduzir Corrida/ }),
    ).toBeInTheDocument();
    expect(screen.getByText('Tipo de labirinto: 8x4')).toBeInTheDocument();
  });
});
