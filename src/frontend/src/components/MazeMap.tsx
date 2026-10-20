import { cellKey, type Cell, type MapState } from '@/lib/telemetry';

interface MazeMapProps {
  cols: number;
  rows: number;
  state: MapState;
}

// Cores do protótipo (docs/figs/DashboardPrototipo.png).
export const MAP_COLORS = {
  cell: '#393A99',
  path: '#10D582',
  start: '#D41717',
  goal: '#2BEB00',
  wall: '#1A8CAD',
  grid: 'rgba(255, 255, 255, 0.07)',
} as const;

const CELL = 100; // unidades do viewBox por célula
const WALL_WIDTH = 8;

/** Origem (0,0) no canto inferior esquerdo e N para cima, como no mock do robô. */
function cellOrigin(c: Cell, rows: number) {
  return { px: c.x * CELL, py: (rows - 1 - c.y) * CELL };
}

function CellRect({
  cell,
  rows,
  fill,
}: {
  cell: Cell;
  rows: number;
  fill: string;
}) {
  const { px, py } = cellOrigin(cell, rows);
  return <rect x={px} y={py} width={CELL} height={CELL} fill={fill} />;
}

export function MazeMap({ cols, rows, state }: MazeMapProps) {
  const { robot, start, goal, path, walls } = state;
  const width = cols * CELL;
  const height = rows * CELL;

  const robotCenter = robot
    ? (() => {
        const { px, py } = cellOrigin(robot.pos, rows);
        return { cx: px + CELL / 2, cy: py + CELL / 2 };
      })()
    : null;

  return (
    <svg
      viewBox={`0 0 ${width} ${height}`}
      className="block w-full h-auto rounded-md"
      role="img"
      aria-label={
        robot
          ? `Mapa do labirinto ${cols}x${rows}. Robô na célula (${robot.pos.x}, ${robot.pos.y}), direção ${robot.heading}.`
          : `Mapa do labirinto ${cols}x${rows}. Aguardando o robô.`
      }
    >
      <rect width={width} height={height} fill={MAP_COLORS.cell} />

      {/* Caminho percorrido */}
      {path.map((c) => (
        <CellRect
          key={cellKey(c)}
          cell={c}
          rows={rows}
          fill={MAP_COLORS.path}
        />
      ))}

      {/* Começo e fim (o fim só aparece depois do evento GOAL_REACHED) */}
      {start && <CellRect cell={start} rows={rows} fill={MAP_COLORS.start} />}
      {goal && <CellRect cell={goal} rows={rows} fill={MAP_COLORS.goal} />}

      {/* Grade */}
      {Array.from({ length: cols - 1 }, (_, i) => (
        <line
          key={`v${i}`}
          x1={(i + 1) * CELL}
          y1={0}
          x2={(i + 1) * CELL}
          y2={height}
          stroke={MAP_COLORS.grid}
        />
      ))}
      {Array.from({ length: rows - 1 }, (_, i) => (
        <line
          key={`h${i}`}
          x1={0}
          y1={(i + 1) * CELL}
          x2={width}
          y2={(i + 1) * CELL}
          stroke={MAP_COLORS.grid}
        />
      ))}

      {/* Paredes detectadas */}
      <g
        stroke={MAP_COLORS.wall}
        strokeWidth={WALL_WIDTH}
        strokeLinecap="square"
      >
        {Object.entries(walls).map(([key, w]) => {
          const [x, y] = key.split(',').map(Number);
          const { px, py } = cellOrigin({ x, y }, rows);
          return (
            <g key={key}>
              {w.n && <line x1={px} y1={py} x2={px + CELL} y2={py} />}
              {w.s && (
                <line x1={px} y1={py + CELL} x2={px + CELL} y2={py + CELL} />
              )}
              {w.w && <line x1={px} y1={py} x2={px} y2={py + CELL} />}
              {w.e && (
                <line x1={px + CELL} y1={py} x2={px + CELL} y2={py + CELL} />
              )}
            </g>
          );
        })}
      </g>

      {/* Borda externa */}
      <rect
        x={WALL_WIDTH / 2}
        y={WALL_WIDTH / 2}
        width={width - WALL_WIDTH}
        height={height - WALL_WIDTH}
        fill="none"
        stroke={MAP_COLORS.wall}
        strokeWidth={WALL_WIDTH}
      />

      {/* Robô: translação anima o movimento; rotação, a mudança de direção */}
      {robot && robotCenter && (
        <g
          style={{
            transform: `translate(${robotCenter.cx}px, ${robotCenter.cy}px)`,
            transition: 'transform 100ms linear',
          }}
        >
          <g
            style={{
              transform: `rotate(${robot.angle}deg)`,
              transition: 'transform 100ms linear',
            }}
          >
            <polygon
              points="0,-30 22,26 0,14 -22,26"
              fill="#FFFFFF"
              stroke="#13143C"
              strokeWidth={4}
              strokeLinejoin="round"
            />
          </g>
        </g>
      )}
    </svg>
  );
}
