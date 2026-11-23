'use client';

import { useEffect } from 'react';

interface ModalExcluirCorridasProps {
  aberto: boolean;
  quantidade: number;
  carregando: boolean;
  erro: string | null;
  onFechar: () => void;
  onConfirmar: () => Promise<void>;
}

export function ModalExcluirCorridas({
  aberto,
  quantidade,
  carregando,
  erro,
  onFechar,
  onConfirmar,
}: ModalExcluirCorridasProps) {
  useEffect(() => {
    if (!aberto) return;
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'Escape' && !carregando) {
        onFechar();
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [aberto, carregando, onFechar]);

  if (!aberto) return null;

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black/70 p-4 font-mono"
      role="dialog"
      aria-modal="true"
      aria-labelledby="modal-excluir-titulo"
    >
      <div className="w-full max-w-md rounded-xl border border-white/20 bg-[#172860] shadow-2xl">
        <div className="flex items-center justify-between border-b border-white/15 px-5 py-4">
          <h3 id="modal-excluir-titulo" className="text-lg font-bold text-white">
            Confirmar exclusão
          </h3>
          <button
            type="button"
            onClick={onFechar}
            disabled={carregando}
            aria-label="Fechar modal"
            className="cursor-pointer rounded px-2 py-1 text-white/70 transition hover:bg-white/10 hover:text-white disabled:opacity-40"
          >
            ✕
          </button>
        </div>

        <div className="flex flex-col gap-4 p-5">
          <p className="text-sm leading-relaxed text-white/90">
            Deseja realmente excluir{' '}
            <strong className="text-red-400">
              {quantidade}{' '}
              {quantidade === 1
                ? 'corrida selecionada'
                : 'corridas selecionadas'}
            </strong>
            ? Todas as telemetrias associadas também serão removidas
            permanentemente.
          </p>

          {erro && (
            <p
              role="alert"
              className="rounded-lg border border-red-500/40 bg-red-950/60 px-3 py-2 text-xs text-red-200"
            >
              {erro}
            </p>
          )}

          <div className="mt-2 flex items-center justify-end gap-3">
            <button
              type="button"
              onClick={onFechar}
              disabled={carregando}
              className="cursor-pointer rounded-lg border border-white/25 px-4 py-2 text-sm text-white/80 transition hover:bg-white/10 hover:text-white disabled:opacity-40"
            >
              Cancelar
            </button>
            <button
              type="button"
              onClick={onConfirmar}
              disabled={carregando}
              className="cursor-pointer rounded-lg bg-red-600 px-4 py-2 text-sm font-bold text-white transition hover:bg-red-500 disabled:cursor-not-allowed disabled:opacity-40"
            >
              {carregando ? 'Excluindo...' : 'Confirmar exclusão'}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}