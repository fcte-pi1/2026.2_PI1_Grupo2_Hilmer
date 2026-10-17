interface MazeTypeProps {
  type: '4x4' | '8x4' | '12x4';
}

export function MazeType({ type }: MazeTypeProps) {
  return (
    <div>
      <p className="text-xl text-white">Tipo de labirinto: {type}</p>
    </div>
  );
}
