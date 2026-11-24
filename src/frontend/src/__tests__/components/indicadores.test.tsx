import { render, screen } from '@testing-library/react';
import { Battery } from '@/components/Battery';
import { BatteryLevel, getBatteryColor } from '@/components/BatteryLevel';
import { ChallengeFlag } from '@/components/ChallengeFlag';
import { Connection } from '@/components/Connection';
import { MazeType } from '@/components/MazeType';
import { PainelResultados } from '@/components/Results';
import { Speed } from '@/components/Speed';
import { Time } from '@/components/Time';

describe('Speed (velocímetro)', () => {
  it('exibe a velocidade recebida', () => {
    render(<Speed speed={2.5} />);
    expect(screen.getByText('Velocidade média: 2.5 m/s')).toBeInTheDocument();
  });

  it('aceita velocidade zero', () => {
    render(<Speed speed={0} />);
    expect(screen.getByText('Velocidade média: 0 m/s')).toBeInTheDocument();
  });
});

describe('BatteryLevel', () => {
  it.each([0, 5, 67, 100])('exibe %s%% e a barra proporcional', (nivel) => {
    render(<BatteryLevel level={nivel} />);
    expect(screen.getByText(`${nivel}%`)).toBeInTheDocument();
    const barra = screen.getByTestId('battery-level-bar');
    expect(barra).toHaveStyle({
      width: `${nivel}%`,
    });
  });

  it('aplica a cor correta de acordo com o nível da bateria', () => {
    const { rerender } = render(<BatteryLevel level={80} />);
    expect(screen.getByTestId('battery-level-bar')).toHaveClass('bg-green-400');

    rerender(<BatteryLevel level={45} />);
    expect(screen.getByTestId('battery-level-bar')).toHaveClass('bg-yellow-400');

    rerender(<BatteryLevel level={15} />);
    expect(screen.getByTestId('battery-level-bar')).toHaveClass('bg-red-500');
  });

  it('avalia as faixas de cor na função getBatteryColor', () => {
    expect(getBatteryColor(100)).toBe('bg-green-400');
    expect(getBatteryColor(51)).toBe('bg-green-400');
    expect(getBatteryColor(50)).toBe('bg-yellow-400');
    expect(getBatteryColor(21)).toBe('bg-yellow-400');
    expect(getBatteryColor(20)).toBe('bg-red-500');
    expect(getBatteryColor(0)).toBe('bg-red-500');
  });
});

describe('Battery (consumo)', () => {
  it('exibe o consumo de bateria', () => {
    render(<Battery consumo={12} />);
    expect(screen.getByText('Consumo de bateria: 12%')).toBeInTheDocument();
  });
});

describe('Time', () => {
  it.each([
    [0, '00:00'],
    [60, '01:00'],
    [95, '01:35'],
  ])('exibe %s s como %s', (segundos, esperado) => {
    render(<Time time={segundos} />);
    expect(
      screen.getByText(`Tempo de conclusão: ${esperado}`),
    ).toBeInTheDocument();
  });
});

describe('ChallengeFlag', () => {
  it('mostra S quando o desafio foi cumprido', () => {
    render(<ChallengeFlag completed />);
    expect(screen.getByText('Desafio cumprido: S')).toBeInTheDocument();
  });

  it('mostra N quando não foi cumprido', () => {
    render(<ChallengeFlag completed={false} />);
    expect(screen.getByText('Desafio cumprido: N')).toBeInTheDocument();
  });
});

describe('MazeType', () => {
  it.each(['4x4', '8x4', '12x4'] as const)('exibe o labirinto %s', (tipo) => {
    render(<MazeType type={tipo} />);
    expect(screen.getByText(`Tipo de labirinto: ${tipo}`)).toBeInTheDocument();
  });
});

describe('Connection', () => {
  it('mostra Conectado com indicador verde', () => {
    const { container } = render(<Connection connected />);
    expect(screen.getByText('Conectado')).toBeInTheDocument();
    expect(container.querySelector('.bg-green-400')).toBeInTheDocument();
  });

  it('mostra Desconectado com indicador vermelho', () => {
    const { container } = render(<Connection connected={false} />);
    expect(screen.getByText('Desconectado')).toBeInTheDocument();
    expect(container.querySelector('.bg-red-500')).toBeInTheDocument();
  });
});

describe('PainelResultados', () => {
  it('renderiza o título e os filhos', () => {
    render(
      <PainelResultados>
        <p>conteúdo interno</p>
      </PainelResultados>,
    );
    expect(
      screen.getByRole('heading', { name: 'Resultados' }),
    ).toBeInTheDocument();
    expect(screen.getByText('conteúdo interno')).toBeInTheDocument();
  });
});
