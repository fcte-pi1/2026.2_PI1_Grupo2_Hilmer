import { MAP_COLORS } from './MazeMap';

const ITEMS = [
  { label: 'Começo', color: MAP_COLORS.start },
  { label: 'Fim', color: MAP_COLORS.goal },
  { label: 'Paredes', color: MAP_COLORS.wall },
  { label: 'Caminho percorrido', color: MAP_COLORS.path },
] as const;

export function MapLegend() {
  return (
    <ul className="flex flex-row flex-wrap lg:flex-col gap-3 font-mono text-sm">
      {ITEMS.map(({ label, color }) => (
        <li key={label} className="flex items-center gap-3">
          <span
            className="w-10 h-10 rounded-sm shrink-0"
            style={{ backgroundColor: color }}
            aria-hidden="true"
          />
          {label}
        </li>
      ))}
      <li className="flex items-center gap-3">
        <span
          className="w-10 h-10 rounded-sm shrink-0 flex items-center justify-center bg-[#393A99]"
          aria-hidden="true"
        >
          <svg viewBox="-30 -34 60 68" className="w-6 h-6">
            <polygon
              points="0,-30 22,26 0,14 -22,26"
              fill="#FFFFFF"
              stroke="#13143C"
              strokeWidth={4}
              strokeLinejoin="round"
            />
          </svg>
        </span>
        Robô
      </li>
    </ul>
  );
}
