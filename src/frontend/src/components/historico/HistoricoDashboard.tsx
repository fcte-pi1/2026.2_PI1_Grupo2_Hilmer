'use client';

import Link from 'next/link';
import { useRouter } from 'next/navigation';
import { useEffect, useState } from 'react';
import {
  excluirCorridas,
  type CorridaResumo,
  type DimensaoLabirinto,
  type RecordeLabirinto,
} from '@/lib/corridas';
import { formatarData, formatarTempo, formatarVelocidade } from '@/lib/format';
import { GraficoCorridas, type PontoGrafico } from './GraficoCorridas';
import { ModalExcluirCorridas } from './ModalExcluirCorridas';

export type Filtro = 'TODOS' | DimensaoLabirinto;
const FILTROS: Filtro[] = ['TODOS', '4x4', '8x4', '12x4'];

const COR_TEMPO = '#3987e5';
const COR_VELOCIDADE = '#199e70';
const COR_BATERIA = '#d95926';

const ROTULO_STATUS: Record<CorridaResumo['status'], string> = {
  EM_ANDAMENTO: 'Em andamento',
  FINALIZADA: 'Finalizada',
  CONCLUIDA: 'Concluída',
  ABORTADA: 'Abortada',
  FALHA: 'Falha',
};

const ehSucesso = (c: CorridaResumo) =>
  c.status === 'FINALIZADA' || c.status === 'CONCLUIDA';

interface HistoricoDashboardProps {
  /** Corridas já filtradas pelo backend (GET /runs?mazeId=). */
  corridas: CorridaResumo[];
  /** Recorde de cada labirinto (GET /runs/best). */
  recordes?: RecordeLabirinto[];
  filtro?: Filtro;
}

export function HistoricoDashboard({
  corridas,
  recordes = [],
  filtro = 'TODOS',
}: HistoricoDashboardProps) {
  const router = useRouter();
  const [selecionadas, setSelecionadas] = useState<Set<string>>(new Set());
  const [modalAberto, setModalAberto] = useState(false);
  const [excluindo, setExcluindo] = useState(false);
  const [erroExclusao, setErroExclusao] = useState<string | null>(null);

  useEffect(() => {
    setSelecionadas(new Set());
  }, [filtro]);

  // Numera pela ordem cronológica das corridas do filtro atual.
  const visiveis = [...corridas]
    .sort((a, b) => a.iniciadaEm.localeCompare(b.iniciadaEm))
    .map((c, i) => ({ ...c, numero: `#${i + 1}` }));
  const encerradas = visiveis.filter((c) => c.status !== 'EM_ANDAMENTO');
  const finalizadas = visiveis.filter(ehSucesso);
  const comBateria = encerradas.filter(
    (c) => c.bateriaInicial !== null && c.bateriaFinal !== null,
  );

  // Corridas em andamento são exibidas no histórico, mas nunca podem ser
  // selecionadas para exclusão.
  const corridasExcluiveis = visiveis.filter(
    (c) => c.status !== 'EM_ANDAMENTO',
  );

  const todasSelecionadas =
    corridasExcluiveis.length > 0 &&
    corridasExcluiveis.every((c) => selecionadas.has(c.id));

  const alternarSelecao = (id: string) => {
    const corrida = visiveis.find((c) => c.id === id);
    if (!corrida || corrida.status === 'EM_ANDAMENTO') return;

    setSelecionadas((prev) => {
      const proximo = new Set(prev);
      if (proximo.has(id)) {
        proximo.delete(id);
      } else {
        proximo.add(id);
      }
      return proximo;
    });
  };

  const alternarTodas = () => {
    if (todasSelecionadas) {
      setSelecionadas(new Set());
    } else {
      setSelecionadas(new Set(corridasExcluiveis.map((c) => c.id)));
    }
  };

  const abrirModalExclusao = () => {
    if (selecionadas.size === 0) return;
    setErroExclusao(null);
    setModalAberto(true);
  };

  const cancelarExclusao = () => {
    if (excluindo) return;
    setModalAberto(false);
    setErroExclusao(null);
    setSelecionadas(new Set());
  };

  const confirmarExclusao = async () => {
    setExcluindo(true);
    setErroExclusao(null);

    try {
      await excluirCorridas(Array.from(selecionadas));
      setSelecionadas(new Set());
      setModalAberto(false);
      router.refresh();
    } catch (err: unknown) {
      const msg =
        err instanceof Error
          ? err.message
          : 'Erro inesperado ao excluir corridas.';
      setErroExclusao(msg);
    } finally {
      setExcluindo(false);
    }
  };

  const pontos = (
    lista: typeof visiveis,
    valor: (c: CorridaResumo) => number,
  ): PontoGrafico[] =>
    lista.map((c) => ({
      corrida: c.numero,
      labirinto: c.labirinto,
      valor: valor(c),
    }));

  const consumo = (c: CorridaResumo) =>
    (c.bateriaInicial ?? 0) - (c.bateriaFinal ?? 0);
  const media = (valores: number[]) =>
    valores.length === 0
      ? null
      : valores.reduce((soma, v) => soma + v, 0) / valores.length;

  // Em "Todos", o recorde é o menor tempo entre os recordes de cada labirinto.
  const recordesDoFiltro = recordes.filter(
    (r) => filtro === 'TODOS' || r.labirinto === filtro,
  );
  const recorde = recordesDoFiltro.length
    ? recordesDoFiltro.reduce((melhor, r) =>
        r.tempoSegundos < melhor.tempoSegundos ? r : melhor,
      )
    : null;
  const velocidadeMedia = media(encerradas.map((c) => c.velocidadeMedia));
  const consumoMedio = media(comBateria.map(consumo));
  const taxaSucesso = encerradas.length
    ? (finalizadas.length / encerradas.length) * 100
    : null;

  return (
    <div className="flex flex-col gap-6 font-mono">
      <div className="flex flex-wrap items-center gap-2">
        <span className="opacity-70">Labirinto:</span>
        {FILTROS.map((f) => (
          <Link
            key={f}
            href={f === 'TODOS' ? '/historico' : `/historico?labirinto=${f}`}
            aria-current={filtro === f ? 'page' : undefined}
            className={`rounded px-3 py-1 border border-white/30 ${
              filtro === f ? 'bg-[#393A99] font-bold' : 'opacity-60'
            }`}
          >
            {f === 'TODOS' ? 'Todos' : f}
          </Link>
        ))}
      </div>

      <div className="grid grid-cols-2 lg:grid-cols-5 gap-4">
        <Indicador rotulo="Corridas" valor={String(visiveis.length)} />
        <Indicador
          rotulo="Taxa de sucesso"
          valor={taxaSucesso === null ? '—' : `${taxaSucesso.toFixed(0)}%`}
        />
        <Indicador
          rotulo={
            recorde && filtro === 'TODOS'
              ? `Recorde (${recorde.labirinto})`
              : 'Recorde'
          }
          valor={recorde === null ? '—' : formatarTempo(recorde.tempoSegundos)}
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
          dados={pontos(comBateria, consumo)}
          formatarValor={(v) => `${v}%`}
        />
      </div>

      <section className="rounded-xl overflow-hidden bg-[#393A99]">
        <div className="flex items-center justify-between bg-[#172860] px-4 py-2">
          <h2 className="text-lg">Corridas</h2>
          <button
            type="button"
            onClick={abrirModalExclusao}
            disabled={selecionadas.size === 0}
            className="cursor-pointer rounded-lg bg-red-600 px-3 py-1.5 text-xs font-bold text-white transition hover:bg-red-500 disabled:cursor-not-allowed disabled:opacity-40"
          >
            {selecionadas.size > 0
              ? `Excluir selecionadas (${selecionadas.size})`
              : 'Excluir selecionadas'}
          </button>
        </div>

        <div className="overflow-x-auto">
          <table className="w-full text-left text-sm">
            <thead className="opacity-70">
              <tr>
                <th className="w-10 px-4 py-2">
                  <input
                    type="checkbox"
                    checked={todasSelecionadas}
                    onChange={alternarTodas}
                    disabled={corridasExcluiveis.length === 0}
                    aria-label="Selecionar todas as corridas"
                    className="h-4 w-4 cursor-pointer accent-red-500 disabled:cursor-not-allowed"
                  />
                </th>
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
              {visiveis.length === 0 && (
                <tr className="border-t border-white/10">
                  <td colSpan={8} className="px-4 py-6 text-center opacity-70">
                    Nenhuma corrida registrada
                    {filtro === 'TODOS' ? '' : ` para o labirinto ${filtro}`}.
                  </td>
                </tr>
              )}
              {[...visiveis].reverse().map((c) => {
                const marcada = selecionadas.has(c.id);
                return (
                  <tr
                    key={c.id}
                    className={`border-t border-white/10 transition-colors ${
                      marcada ? 'bg-red-950/30' : ''
                    }`}
                  >
                    <td className="px-4 py-2">
                      <input
                        type="checkbox"
                        checked={marcada}
                        onChange={() => alternarSelecao(c.id)}
                        disabled={c.status === 'EM_ANDAMENTO'}
                        aria-label={`Selecionar corrida ${c.numero}`}
                        title={
                          c.status === 'EM_ANDAMENTO'
                            ? 'Corridas em andamento não podem ser excluídas'
                            : undefined
                        }
                        className="h-4 w-4 cursor-pointer accent-red-500 disabled:cursor-not-allowed disabled:opacity-40"
                      />
                    </td>
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
                      {c.bateriaInicial === null || c.bateriaFinal === null
                        ? '—'
                        : `${c.bateriaInicial}% → ${c.bateriaFinal}%`}
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        </div>
      </section>

      <ModalExcluirCorridas
        aberto={modalAberto}
        quantidade={selecionadas.size}
        carregando={excluindo}
        erro={erroExclusao}
        onFechar={cancelarExclusao}
        onConfirmar={confirmarExclusao}
      />
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