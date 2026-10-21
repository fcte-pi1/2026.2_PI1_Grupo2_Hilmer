'use client';

import {
  CellWalls,
  HeadingDirection,
  MazeDimension,
  RobotPosition,
  TrajectoryPoint,
} from '@/lib/mock-data';

interface MazeMapProps {
  dimension?: MazeDimension;
  robotPosition?: RobotPosition;
  path?: TrajectoryPoint[];
  discoveredWalls?: Record<string, CellWalls>;
}

export function MazeMap({
  dimension = '4x4',
  robotPosition = { x: 0, y: 0, heading: 'N' },
  path = [],
  discoveredWalls = {},
}: MazeMapProps) {
  // Configuração das dimensões
  const cols = dimension === '4x4' ? 4 : dimension === '8x4' ? 8 : 12;
  const rows = 4;

  const cellSize = 70; // tamanho em pixels de cada célula no SVG
  const margin = 30; // espaço para os números dos eixos

  const gridWidth = cols * cellSize;
  const gridHeight = rows * cellSize;
  const svgWidth = gridWidth + margin * 2;
  const svgHeight = gridHeight + margin * 2;

  // Célula de largada (0, 0) e chegada (canto superior direito do labirinto)
  const startCell = { x: 0, y: 0 };
  const goalCell = { x: cols - 1, y: rows - 1 };

  // Função para converter coordenadas do robô (origem no canto inferior esquerdo) para SVG
  const getCellCoords = (x: number, y: number) => {
    const svgX = margin + x * cellSize;
    const svgY = margin + (rows - 1 - y) * cellSize;
    return {
      x: svgX,
      y: svgY,
      centerX: svgX + cellSize / 2,
      centerY: svgY + cellSize / 2,
    };
  };

  // Mapeamento de rotação para cada orientação
  const getHeadingAngle = (heading: HeadingDirection) => {
    switch (heading) {
      case 'N':
        return 0;
      case 'E':
        return 90;
      case 'S':
        return 180;
      case 'W':
        return 270;
      default:
        return 0;
    }
  };

  // Coordenadas da trajetória para o polyline
  const pathPoints = path
    .map((point) => {
      const { centerX, centerY } = getCellCoords(point.x, point.y);
      return `${centerX},${centerY}`;
    })
    .join(' ');

  const currentRobotCoords = getCellCoords(robotPosition.x, robotPosition.y);

  return (
    <section className="flex flex-col flex-1 rounded-xl overflow-hidden bg-[#393A99] font-mono text-white shadow-xl">
      {/* Cabeçalho do Mapa */}
      <div className="flex flex-wrap items-center justify-between bg-[#172860] px-4 py-2.5 border-b border-indigo-900/50">
        <div className="flex items-center gap-3">
          <span className="text-xl font-bold tracking-wide">
            Mapa do Labirinto
          </span>
          <span className="px-2 py-0.5 text-xs font-semibold rounded bg-indigo-500/30 text-indigo-200 border border-indigo-400/30">
            {dimension}
          </span>
        </div>
        <div className="text-xs text-indigo-200/80">
          Células: {cols} × {rows} ({cols * 18} cm × {rows * 18} cm) • Escala 18
          cm/célula
        </div>
      </div>

      {/* Área Gráfica Interativa (SVG) */}
      <div className="flex-1 p-4 flex items-center justify-center bg-[#13143C]/80 min-h-[360px] overflow-auto">
        <svg
          viewBox={`0 0 ${svgWidth} ${svgHeight}`}
          className="w-full max-w-full h-auto drop-shadow-md select-none transition-all duration-300"
          style={{ maxHeight: '520px' }}
        >
          <defs>
            {/* Brilho da posição do robô */}
            <filter
              id="robot-glow"
              x="-50%"
              y="-50%"
              width="200%"
              height="200%"
            >
              <feGaussianBlur stdDeviation="3" result="blur" />
              <feMerge>
                <feMergeNode in="blur" />
                <feMergeNode in="SourceGraphic" />
              </feMerge>
            </filter>

            {/* Marcador de seta na linha do trajeto */}
            <marker
              id="path-dot"
              viewBox="0 0 10 10"
              refX="5"
              refY="5"
              markerWidth="4"
              markerHeight="4"
            >
              <circle cx="5" cy="5" r="4" fill="#38BDF8" opacity="0.8" />
            </marker>
          </defs>

          {/* Rótulos dos eixos de coordenadas */}
          {/* Eixo X (0 .. cols-1) */}
          {Array.from({ length: cols }).map((_, x) => {
            const { centerX } = getCellCoords(x, 0);
            return (
              <text
                key={`label-x-${x}`}
                x={centerX}
                y={svgHeight - 10}
                textAnchor="middle"
                className="fill-indigo-300/70 text-[11px] font-mono"
              >
                X={x}
              </text>
            );
          })}

          {/* Eixo Y (0 .. rows-1) */}
          {Array.from({ length: rows }).map((_, y) => {
            const { centerY } = getCellCoords(0, y);
            return (
              <text
                key={`label-y-${y}`}
                x={12}
                y={centerY + 4}
                textAnchor="middle"
                className="fill-indigo-300/70 text-[11px] font-mono"
              >
                Y={y}
              </text>
            );
          })}

          {/* Grade de Células */}
          {Array.from({ length: rows }).map((_, rIndex) => {
            const y = rows - 1 - rIndex;
            return Array.from({ length: cols }).map((__, x) => {
              const { x: cellX, y: cellY } = getCellCoords(x, y);
              const isStart = x === startCell.x && y === startCell.y;
              const isGoal = x === goalCell.x && y === goalCell.y;

              return (
                <g key={`cell-${x}-${y}`}>
                  {/* Fundo da célula */}
                  <rect
                    x={cellX}
                    y={cellY}
                    width={cellSize}
                    height={cellSize}
                    fill={
                      isStart ? '#10B9811A' : isGoal ? '#F59E0B20' : '#17286040'
                    }
                    stroke="#393A9960"
                    strokeWidth="1"
                    strokeDasharray="2,2"
                    className="transition-colors duration-200"
                  />

                  {/* Destaque da Largada */}
                  {isStart && (
                    <g>
                      <rect
                        x={cellX + 2}
                        y={cellY + 2}
                        width={cellSize - 4}
                        height={cellSize - 4}
                        fill="none"
                        stroke="#10B981"
                        strokeWidth="1.5"
                        rx="4"
                      />
                      <text
                        x={cellX + 6}
                        y={cellY + 16}
                        className="fill-emerald-400 text-[10px] font-bold tracking-wider"
                      >
                        INÍCIO
                      </text>
                    </g>
                  )}

                  {/* Destaque da Chegada */}
                  {isGoal && (
                    <g>
                      <rect
                        x={cellX + 2}
                        y={cellY + 2}
                        width={cellSize - 4}
                        height={cellSize - 4}
                        fill="none"
                        stroke="#F59E0B"
                        strokeWidth="1.5"
                        rx="4"
                      />
                      <text
                        x={cellX + 6}
                        y={cellY + 16}
                        className="fill-amber-400 text-[10px] font-bold tracking-wider"
                      >
                        CHEGADA
                      </text>
                    </g>
                  )}
                </g>
              );
            });
          })}

          {/* Paredes Internas Descobertas */}
          {Object.entries(discoveredWalls).map(([coordKey, walls]) => {
            const [cx, cy] = coordKey.split(',').map(Number);
            const { x: cellX, y: cellY } = getCellCoords(cx, cy);

            return (
              <g
                key={`walls-${coordKey}`}
                stroke="#EF4444"
                strokeWidth="3.5"
                strokeLinecap="round"
              >
                {/* Parede Norte */}
                {walls.n && (
                  <line
                    x1={cellX}
                    y1={cellY}
                    x2={cellX + cellSize}
                    y2={cellY}
                  />
                )}
                {/* Parede Leste */}
                {walls.e && (
                  <line
                    x1={cellX + cellSize}
                    y1={cellY}
                    x2={cellX + cellSize}
                    y2={cellY + cellSize}
                  />
                )}
                {/* Parede Sul */}
                {walls.s && (
                  <line
                    x1={cellX}
                    y1={cellY + cellSize}
                    x2={cellX + cellSize}
                    y2={cellY + cellSize}
                  />
                )}
                {/* Parede Oeste */}
                {walls.w && (
                  <line
                    x1={cellX}
                    y1={cellY}
                    x2={cellX}
                    y2={cellY + cellSize}
                  />
                )}
              </g>
            );
          })}

          {/* Rastro da Trajetória Percorrida */}
          {pathPoints && (
            <polyline
              points={pathPoints}
              fill="none"
              stroke="#38BDF8"
              strokeWidth="2.5"
              strokeLinecap="round"
              strokeLinejoin="round"
              strokeDasharray="5,4"
              opacity="0.85"
            />
          )}

          {/* Pontos de passagem da trajetória */}
          {path.map((pt, idx) => {
            const { centerX, centerY } = getCellCoords(pt.x, pt.y);
            return (
              <circle
                key={`crumb-${idx}`}
                cx={centerX}
                cy={centerY}
                r="3"
                fill="#38BDF8"
                opacity="0.7"
              />
            );
          })}

          {/* Paredes Perimetrais Externas do Labirinto */}
          <rect
            x={margin}
            y={margin}
            width={gridWidth}
            height={gridHeight}
            fill="none"
            stroke="#818CF8"
            strokeWidth="3.5"
            rx="2"
          />

          {/* Robô Micromouse (Posição Atual + Orientação) */}
          <g
            transform={`translate(${currentRobotCoords.centerX}, ${currentRobotCoords.centerY}) rotate(${getHeadingAngle(robotPosition.heading)})`}
            filter="url(#robot-glow)"
            className="transition-transform duration-300 ease-out"
          >
            {/* Círculo de pulso do robô */}
            <circle
              cx="0"
              cy="0"
              r="18"
              fill="#38BDF820"
              stroke="#38BDF8"
              strokeWidth="1.5"
            />

            {/* Corpo do robô */}
            <rect
              x="-11"
              y="-13"
              width="22"
              height="26"
              rx="4"
              fill="#1E293B"
              stroke="#F8FAFC"
              strokeWidth="1.5"
            />

            {/* Farol / Direcionador frontal (aponta para Norte relativo) */}
            <polygon points="0,-18 5,-12 -5,-12" fill="#38BDF8" />

            {/* Rodas laterais do micromouse */}
            <rect
              x="-14"
              y="-8"
              width="3"
              height="16"
              rx="1.5"
              fill="#94A3B8"
            />
            <rect x="11" y="-8" width="3" height="16" rx="1.5" fill="#94A3B8" />
          </g>
        </svg>
      </div>

      {/* Barra de Status Inferior do Mapa */}
      <div className="flex flex-wrap items-center justify-between bg-[#172860] px-4 py-2 border-t border-indigo-900/50 text-xs">
        <div className="flex items-center gap-4">
          <span>
            Posição Atual:{' '}
            <strong className="text-sky-300">
              ({robotPosition.x}, {robotPosition.y})
            </strong>
          </span>
          <span>
            Orientação:{' '}
            <strong className="text-sky-300">{robotPosition.heading}</strong>
          </span>
        </div>
        <div className="text-indigo-300/80">
          Passos no trajeto:{' '}
          <strong className="text-white">{path.length}</strong>
        </div>
      </div>
    </section>
  );
}
