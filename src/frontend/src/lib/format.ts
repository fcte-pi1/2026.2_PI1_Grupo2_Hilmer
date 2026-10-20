/** Formata segundos como mm:ss. */
export function formatarTempo(segundos: number) {
  const minutos = Math.floor(segundos / 60);
  const resto = Math.floor(segundos % 60);
  return `${String(minutos).padStart(2, '0')}:${String(resto).padStart(2, '0')}`;
}

export function formatarVelocidade(metrosPorSegundo: number) {
  return `${metrosPorSegundo.toFixed(2)} m/s`;
}

export function formatarData(iso: string) {
  return new Date(iso).toLocaleString('pt-BR', {
    day: '2-digit',
    month: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    timeZone: 'America/Sao_Paulo',
  });
}
