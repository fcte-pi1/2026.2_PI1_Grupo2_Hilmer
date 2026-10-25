import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { MazeLegend } from '@/components/MazeLegend';

describe('MazeLegend: botões de labirinto', () => {
  it('mostra os três tamanhos', () => {
    render(<MazeLegend currentDimension="4x4" />);
    ['4x4', '8x4', '12x4'].forEach((dim) =>
      expect(screen.getByRole('button', { name: dim })).toBeInTheDocument(),
    );
  });

  it.each(['4x4', '8x4', '12x4'] as const)(
    'clicar em %s chama onSelectDimension',
    async (dim) => {
      const onSelect = jest.fn();
      const user = userEvent.setup();
      render(
        <MazeLegend currentDimension="4x4" onSelectDimension={onSelect} />,
      );

      await user.click(screen.getByRole('button', { name: dim }));

      expect(onSelect).toHaveBeenCalledTimes(1);
      expect(onSelect).toHaveBeenCalledWith(dim);
    },
  );

  it('destaca apenas o tamanho atual', () => {
    render(<MazeLegend currentDimension="8x4" />);
    expect(screen.getByRole('button', { name: '8x4' })).toHaveClass('ring-2');
    expect(screen.getByRole('button', { name: '4x4' })).not.toHaveClass(
      'ring-2',
    );
    expect(screen.getByRole('button', { name: '12x4' })).not.toHaveClass(
      'ring-2',
    );
  });

  it('não quebra ao clicar sem onSelectDimension', async () => {
    const user = userEvent.setup();
    render(<MazeLegend currentDimension="4x4" />);
    await user.click(screen.getByRole('button', { name: '12x4' }));
    expect(screen.getByRole('button', { name: '12x4' })).toBeInTheDocument();
  });
});

describe('MazeLegend: simulação', () => {
  it('esconde o botão de simulação sem onToggleSimulation', () => {
    render(<MazeLegend currentDimension="4x4" />);
    expect(
      screen.queryByRole('button', { name: /Simulação|Reproduzir/ }),
    ).not.toBeInTheDocument();
  });

  it('mostra Reproduzir Corrida quando parado e dispara o toggle', async () => {
    const onToggle = jest.fn();
    const user = userEvent.setup();
    render(<MazeLegend currentDimension="4x4" onToggleSimulation={onToggle} />);
    await user.click(
      screen.getByRole('button', { name: /Reproduzir Corrida/ }),
    );
    expect(onToggle).toHaveBeenCalledTimes(1);
  });

  it('mostra Pausar Simulação quando está simulando', () => {
    render(
      <MazeLegend
        currentDimension="4x4"
        isSimulating
        onToggleSimulation={() => {}}
      />,
    );
    expect(
      screen.getByRole('button', { name: /Pausar Simulação/ }),
    ).toBeInTheDocument();
  });
});

describe('MazeLegend: legenda', () => {
  it('lista os itens visuais', () => {
    render(<MazeLegend currentDimension="4x4" />);
    [
      'Robô / Posição Atual',
      'Ponto de Partida',
      'Área de Chegada',
      'Paredes Detectadas',
      'Trajeto Percorrido',
    ].forEach((item) => expect(screen.getByText(item)).toBeInTheDocument());
  });
});
