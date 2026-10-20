'use client';

export default function HistoricoErro({
  error,
  retry,
}: {
  error: Error & { digest?: string };
  retry: () => void;
}) {
  return (
    <div className="flex flex-col items-start gap-4 font-mono">
      <h1 className="text-3xl">Histórico de corridas</h1>
      <p>Não foi possível carregar o histórico: {error.message}</p>
      <button
        type="button"
        onClick={() => retry()}
        className="rounded px-3 py-1 border border-white/30 bg-[#393A99]"
      >
        Tentar novamente
      </button>
    </div>
  );
}
