interface TimeProps {
  time: number;
}

export function Time({ time }: TimeProps) {
  return (
    <div>
      <p className="text-xl text-white">Tempo de conclusão: {time}s</p>
    </div>
  );
}
