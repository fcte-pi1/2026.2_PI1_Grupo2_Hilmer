'use client';

import { useState } from 'react';
import type { CorridaResumo, DimensaoLabirinto } from '@/lib/corridas';
import { formatarData, formatarTempo, formatarVelocidade } from '@/lib/format';
import { GraficoCorridas, type PontoGrafico } from './GraficoCorridas';

type Filtro = 'TODOS' | DimensaoLabirinto;
const FILTROS: Filtro[] = ['TODOS', '4x4', '8x4', '12x4'];

const COR_TEMPO = '#3987e5';
const COR_VELOCIDADE = '#199e70';
const COR_BATERIA = '#d95926';

const ROTULO_STATUS: Record<CorridaResumo['status'], string> = {
  EM_ANDAMENTO: 'Em andamento',
  FINALIZADA: 'Finalizada',
  ABORTADA: 'Abortada',
};

interface HistoricoDashboardProps {
  corridas: CorridaResumo[];
}

export function HistoricoDashboard({ corridas }: HistoricoDashboardProps) {
  const [filtro, setFiltro] = useState<Filtro>('TODOS');

  // Numera pela ordem cronológica de todo o histórico, para o "#n" de uma
  // corrida não mudar quando o filtro muda.
  const numeradas = [...corridas]
    .sort((a, b) => a.iniciadaEm.localeCompare(b.iniciadaEm))
    .map((c, i) => ({ ...c, numero: `#${i + 1}` }));
  const visiveis = numeradas.filter(
    (c) => filtro === 'TODOS' || c.labirinto === filtro,
  );
  const encerradas = visiveis.filter((c) => c.status !== 'EM_ANDAMENTO');
  const finalizadas = visiveis.filter((c) => c.status === 'FINALIZADA');

  const pontos = (
    lista: typeof visiveis,
    valor: (c: CorridaResumo) => number,
  ): PontoGrafico[] =>
    lista.map((c) => ({
      corrida: c.numero,
      labirinto: c.labirinto,
      valor: valor(c),
    }));

  const consumo = (c: CorridaResumo) => c.bateriaInicial - c.bateriaFinal;
  const media = (valores: number[]) =>
    valores.length === 0
      ? null
      : valores.reduce((soma, v) => soma + v, 0) / valores.length;

  const melhorTempo = finalizadas.length
    ? Math.min(...finalizadas.map((c) => c.duracaoSegundos ?? Infinity))
    : null;
  const velocidadeMedia = media(encerradas.map((c) => c.velocidadeMedia));
  const consumoMedio = media(encerradas.map(consumo));
  const taxaSucesso = encerradas.length
    ? (finalizadas.length / encerradas.length) * 100
    : null;

  return (
    <div className="flex flex-col gap-6 font-mono">
      <div className="flex flex-wrap items-center gap-2">
        <span className="opacity-70">Labirinto:</span>
        {FILTROS.map((f) => (
          <button
            key={f}
            type="button"
            onClick={() => setFiltro(f)}
            aria-pressed={filtro === f}
            className={`rounded px-3 py-1 border border-white/30 ${
              filtro === f ? 'bg-[#393A99] font-bold' : 'opacity-60'
            }`}
          >
            {f === 'TODOS' ? 'Todos' : f}
          </button>
        ))}
      </div>

      <div className="grid grid-cols-2 lg:grid-cols-5 gap-4">
        <Indicador rotulo="Corridas" valor={String(visiveis.length)} />
        <Indicador
          rotulo="Taxa de sucesso"
          valor={taxaSucesso === null ? '—' : `${taxaSucesso.toFixed(0)}%`}
        />
        <Indicador
          rotulo="Melhor tempo"
          valor={melhorTempo === null ? '—' : formatarTempo(melhorTempo)}
        />
        <Indicador
          rotulo="Velocidade média"
          valor={
            velocidadeMedia === null ? '—' : formatarVelocidade(velocidadeMedia)
          }
        />
        <Indicador
          rotulo="Consumo médio de bateria"
          valor={consumoMedio === null ? '—' : `${consumoMedio.toFixed(1)}%`}
        />
      </div>

      <div className="grid grid-cols-1 xl:grid-cols-3 gap-4">
        <GraficoCorridas
          titulo="Tempo de conclusão"
          tipo="barra"
          cor={COR_TEMPO}
          dados={pontos(finalizadas, (c) => c.duracaoSegundos ?? 0)}
          formatarValor={formatarTempo}
        />
        <GraficoCorridas
          titulo="Velocidade média"
          tipo="linha"
          cor={COR_VELOCIDADE}
          dados={pontos(encerradas, (c) => c.velocidadeMedia)}
          formatarValor={formatarVelocidade}
        />
        <GraficoCorridas
          titulo="Consumo de bateria"
          tipo="barra"
          cor={COR_BATERIA}
          dados={pontos(encerradas, consumo)}
          formatarValor={(v) => `${v}%`}
        />
      </div>

      <section className="rounded-xl overflow-hidden bg-[#393A99]">
        <h2 className="bg-[#172860] px-4 py-2 text-lg">Corridas</h2>
        <div className="overflow-x-auto">
          <table className="w-full text-left text-sm">
            <thead className="opacity-70">
              <tr>
                <th className="px-4 py-2">Corrida</th>
                <th className="px-4 py-2">Data</th>
                <th className="px-4 py-2">Labirinto</th>
                <th className="px-4 py-2">Status</th>
                <th className="px-4 py-2 text-right">Tempo</th>
                <th className="px-4 py-2 text-right">Vel. média</th>
                <th className="px-4 py-2 text-right">Bateria</th>
              </tr>
            </thead>
            <tbody>
              {[...visiveis].reverse().map((c) => (
                <tr key={c.id} className="border-t border-white/10">
                  <td className="px-4 py-2">{c.numero}</td>
                  <td className="px-4 py-2">{formatarData(c.iniciadaEm)}</td>
                  <td className="px-4 py-2">{c.labirinto}</td>
                  <td className="px-4 py-2">{ROTULO_STATUS[c.status]}</td>
                  <td className="px-4 py-2 text-right">
                    {c.duracaoSegundos === null
                      ? '—'
                      : formatarTempo(c.duracaoSegundos)}
                  </td>
                  <td className="px-4 py-2 text-right">
                    {formatarVelocidade(c.velocidadeMedia)}
                  </td>
                  <td className="px-4 py-2 text-right">
                    {c.bateriaInicial}% → {c.bateriaFinal}%
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </section>
    </div>
  );
}

function Indicador({ rotulo, valor }: { rotulo: string; valor: string }) {
  return (
    <div className="rounded-xl bg-[#393A99] px-4 py-3">
      <p className="text-sm opacity-70">{rotulo}</p>
      <p className="text-2xl font-bold">{valor}</p>
    </div>
  );
}
