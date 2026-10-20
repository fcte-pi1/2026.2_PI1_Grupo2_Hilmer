'use client';

import { MazeDimension } from '@/lib/mock-data';

interface MazeLegendProps {
  currentDimension: MazeDimension;
  onSelectDimension?: (dim: MazeDimension) => void;
  isSimulating?: boolean;
  onToggleSimulation?: () => void;
}

export function MazeLegend({
  currentDimension,
  onSelectDimension,
  isSimulating = false,
  onToggleSimulation,
}: MazeLegendProps) {
  const dimensions: MazeDimension[] = ['4x4', '8x4', '12x4'];

  return (
    <aside className="flex flex-col rounded-xl overflow-hidden bg-[#393A99] font-mono text-white w-full lg:w-64 shadow-xl">
      {/* Cabeçalho */}
      <h2 className="bg-[#172860] px-4 py-2.5 text-xl font-bold tracking-wide border-b border-indigo-900/50">
        Legenda & Controle
      </h2>

      <div className="p-4 flex flex-col gap-5 flex-1 justify-between">
        {/* Itens Visuais da Legenda */}
        <div className="flex flex-col gap-3 text-sm">
          {/* Robô */}
          <div className="flex items-center gap-3">
            <span className="w-5 h-5 rounded flex items-center justify-center bg-sky-400 text-slate-900 font-bold text-xs shadow-sm">
              ▲
            </span>
            <span>Robô / Posição Atual</span>
          </div>

          {/* Largada */}
          <div className="flex items-center gap-3">
            <span className="w-5 h-5 rounded border-2 border-emerald-400 bg-emerald-500/20 flex items-center justify-center text-[10px] text-emerald-300 font-bold">
              0,0
            </span>
            <span>Ponto de Partida</span>
          </div>

          {/* Chegada */}
          <div className="flex items-center gap-3">
            <span className="w-5 h-5 rounded border-2 border-amber-400 bg-amber-500/20 flex items-center justify-center text-[10px] text-amber-300 font-bold">
              ★
            </span>
            <span>Área de Chegada</span>
          </div>

          {/* Paredes */}
          <div className="flex items-center gap-3">
            <span className="w-5 h-1.5 rounded-full bg-red-500 shadow-sm" />
            <span>Paredes Detectadas</span>
          </div>

          {/* Trajetória */}
          <div className="flex items-center gap-3">
            <span className="w-5 border-b-2 border-dashed border-sky-400" />
            <span>Trajeto Percorrido</span>
          </div>

          {/* Escala */}
          <div className="pt-2 border-t border-indigo-400/20 text-xs text-indigo-200">
            📏 Escala física: <strong>18 cm × 18 cm</strong> por célula.
          </div>
        </div>

        {/* Controles de Tamanho do Labirinto */}
        <div className="flex flex-col gap-3 pt-3 border-t border-indigo-400/20">
          <label className="text-xs font-semibold text-indigo-200 uppercase tracking-wider">
            Tamanho do Labirinto:
          </label>
          <div className="grid grid-cols-3 gap-2">
            {dimensions.map((dim) => {
              const active = dim === currentDimension;
              return (
                <button
                  key={dim}
                  type="button"
                  onClick={() => onSelectDimension?.(dim)}
                  className={`py-1.5 px-2 text-xs font-bold rounded transition-all cursor-pointer ${
                    active
                      ? 'bg-sky-500 text-slate-950 shadow-md ring-2 ring-sky-300'
                      : 'bg-[#172860] hover:bg-[#1f357f] text-indigo-100 border border-indigo-500/30'
                  }`}
                >
                  {dim}
                </button>
              );
            })}
          </div>

          {/* Botão de Animação / Simulação da Corrida */}
          {onToggleSimulation && (
            <button
              type="button"
              onClick={onToggleSimulation}
              className={`w-full mt-2 py-2 px-3 text-xs font-bold rounded flex items-center justify-center gap-2 transition-all cursor-pointer shadow-md ${
                isSimulating
                  ? 'bg-amber-500 hover:bg-amber-600 text-slate-950'
                  : 'bg-emerald-600 hover:bg-emerald-500 text-white'
              }`}
            >
              {isSimulating ? (
                <>
                  <span className="animate-pulse">⏸</span> Pausar Simulação
                </>
              ) : (
                <>
                  <span>▶</span> Reproduzir Corrida
                </>
              )}
            </button>
          )}
        </div>
      </div>
    </aside>
  );
}
