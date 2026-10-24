import { render, screen } from '@testing-library/react';
import { MazeMap } from '@/components/MazeMap';

// Geometria do componente: célula = 70 px, margem = 30 px, origem (0,0) no canto
// inferior esquerdo. Centro da célula (x, y) = (65 + 70x, 65 + 70(3 - y)).
const celulas = (container: HTMLElement) =>
  container.querySelectorAll('rect[stroke-dasharray="2,2"]');
const robo = (container: HTMLElement) =>
  container.querySelector('g[filter="url(#robot-glow)"]');

describe('MazeMap: grade', () => {
  it.each([
    ['4x4', 16, '0 0 340 340'],
    ['8x4', 32, '0 0 620 340'],
    ['12x4', 48, '0 0 900 340'],
  ] as const)('desenha %s com %s células', (dim, total, viewBox) => {
    const { container } = render(<MazeMap dimension={dim} />);
    expect(celulas(container)).toHaveLength(total);
    expect(container.querySelector('svg')).toHaveAttribute('viewBox', viewBox);
    expect(screen.getByText(dim)).toBeInTheDocument();
  });

  it('usa 4x4 por padrão', () => {
    const { container } = render(<MazeMap />);
    expect(celulas(container)).toHaveLength(16);
  });

  it('mostra a escala física do labirinto', () => {
    render(<MazeMap dimension="8x4" />);
    expect(screen.getByText(/Células: 8 × 4/)).toBeInTheDocument();
    expect(screen.getByText(/144 cm × 72 cm/)).toBeInTheDocument();
  });

  it('rotula os eixos X e Y', () => {
    render(<MazeMap dimension="4x4" />);
    ['X=0', 'X=1', 'X=2', 'X=3', 'Y=0', 'Y=1', 'Y=2', 'Y=3'].forEach((r) =>
      expect(screen.getByText(r)).toBeInTheDocument(),
    );
    expect(screen.queryByText('X=4')).not.toBeInTheDocument();
  });

  it('marca largada e chegada uma vez cada', () => {
    render(<MazeMap dimension="12x4" />);
    expect(screen.getAllByText('INÍCIO')).toHaveLength(1);
    expect(screen.getAllByText('CHEGADA')).toHaveLength(1);
  });

  it('desenha o perímetro com a largura da grade', () => {
    const { container } = render(<MazeMap dimension="12x4" />);
    const perimetro = container.querySelector('rect[stroke="#818CF8"]');
    expect(perimetro).toHaveAttribute('width', '840');
    expect(perimetro).toHaveAttribute('height', '280');
  });
});

describe('MazeMap: robô', () => {
  it('começa em (0,0) apontando para o norte por padrão', () => {
    const { container } = render(<MazeMap />);
    expect(robo(container)).toHaveAttribute(
      'transform',
      'translate(65, 275) rotate(0)',
    );
  });

  it('posiciona o robô na célula informada', () => {
    const { container } = render(
      <MazeMap dimension="4x4" robotPosition={{ x: 3, y: 3, heading: 'E' }} />,
    );
    expect(robo(container)).toHaveAttribute(
      'transform',
      'translate(275, 65) rotate(90)',
    );
  });

  it('posiciona o robô no fim do labirinto 12x4', () => {
    const { container } = render(
      <MazeMap
        dimension="12x4"
        robotPosition={{ x: 11, y: 3, heading: 'E' }}
      />,
    );
    expect(robo(container)).toHaveAttribute(
      'transform',
      'translate(835, 65) rotate(90)',
    );
  });

  it.each([
    ['N', 0],
    ['E', 90],
    ['S', 180],
    ['W', 270],
  ] as const)('orientação %s gira %s°', (heading, angulo) => {
    const { container } = render(
      <MazeMap robotPosition={{ x: 0, y: 0, heading }} />,
    );
    expect(robo(container)?.getAttribute('transform')).toContain(
      `rotate(${angulo})`,
    );
  });

  it('mostra posição e orientação na barra de status', () => {
    render(<MazeMap robotPosition={{ x: 2, y: 1, heading: 'W' }} />);
    expect(screen.getByText('(2, 1)')).toBeInTheDocument();
    expect(screen.getByText('W')).toBeInTheDocument();
  });

  it('não quebra com o robô fora dos limites', () => {
    expect(() =>
      render(<MazeMap robotPosition={{ x: 99, y: 99, heading: 'N' }} />),
    ).not.toThrow();
  });
});

describe('MazeMap: trajeto', () => {
  it('não desenha trajeto quando o caminho está vazio', () => {
    const { container } = render(<MazeMap path={[]} />);
    expect(container.querySelector('polyline')).not.toBeInTheDocument();
    expect(screen.getByText('Passos no trajeto:')).toHaveTextContent(
      'Passos no trajeto: 0',
    );
  });

  it('desenha o polyline e um ponto por passo', () => {
    const path = [
      { x: 0, y: 0 },
      { x: 0, y: 1 },
      { x: 1, y: 1 },
    ];
    const { container } = render(<MazeMap path={path} />);
    expect(container.querySelector('polyline')).toHaveAttribute(
      'points',
      '65,275 65,205 135,205',
    );
    expect(container.querySelectorAll('circle[r="3"]')).toHaveLength(3);
    expect(screen.getByText('Passos no trajeto:')).toHaveTextContent(
      'Passos no trajeto: 3',
    );
  });
});

describe('MazeMap: paredes descobertas', () => {
  it('não desenha paredes internas sem dados', () => {
    const { container } = render(<MazeMap />);
    expect(container.querySelectorAll('g[stroke="#EF4444"]')).toHaveLength(0);
  });

  it('desenha uma linha por parede informada', () => {
    const { container } = render(
      <MazeMap
        discoveredWalls={{
          '0,0': { e: true },
          '1,1': { n: true, w: true },
          '2,2': { n: true, e: true, s: true, w: true },
        }}
      />,
    );
    expect(container.querySelectorAll('g[stroke="#EF4444"]')).toHaveLength(3);
    expect(container.querySelectorAll('g[stroke="#EF4444"] line')).toHaveLength(
      1 + 2 + 4,
    );
  });

  it('ignora lados marcados como falsos', () => {
    const { container } = render(
      <MazeMap discoveredWalls={{ '0,0': { n: true, e: false } }} />,
    );
    expect(container.querySelectorAll('g[stroke="#EF4444"] line')).toHaveLength(
      1,
    );
  });
});
