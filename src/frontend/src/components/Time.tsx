interface TimeProps {
  time: number;
}

function formatarTempo(segundos: number) {
  const minutos = Math.floor(segundos / 60);
  const resto = Math.floor(segundos % 60);
  return `${String(minutos).padStart(2, '0')}:${String(resto).padStart(2, '0')}`;
}

export function Time({ time }: TimeProps) {
  return (
    <div>
      <p className="text-xl text-white">
        Tempo de conclusão: {formatarTempo(time)}
      </p>
    </div>
  );
}
