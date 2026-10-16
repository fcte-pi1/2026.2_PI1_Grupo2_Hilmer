interface SpeedProps {
  speed: number;
}

export function Speed({ speed }: SpeedProps) {
  return (
    <div>
      <p className="text-xl text-white">Velocidade média: {speed} m/s</p>
    </div>
  );
}
