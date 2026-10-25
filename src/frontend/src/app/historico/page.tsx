import { Suspense } from 'react';
import { HistoricoDashboard } from '@/components/historico/HistoricoDashboard';
import { ehDimensaoLabirinto, getCorridas, getRecordes } from '@/lib/corridas';

type SearchParams = Promise<{ [chave: string]: string | string[] | undefined }>;

export default function HistoricoPage({
  searchParams,
}: {
  searchParams: SearchParams;
}) {
  return (
    <div className="flex flex-col gap-6">
      <h1 className="text-3xl font-mono">Histórico de corridas</h1>
      <Suspense
        fallback={<p className="font-mono opacity-70">Carregando corridas…</p>}
      >
        <Historico searchParams={searchParams} />
      </Suspense>
    </div>
  );
}

async function Historico({ searchParams }: { searchParams: SearchParams }) {
  const { labirinto } = await searchParams;
  // Valor inválido ou ausente na URL equivale ao filtro "Todos".
  const filtro = ehDimensaoLabirinto(labirinto) ? labirinto : undefined;

  const [corridas, recordes] = await Promise.all([
    getCorridas(filtro),
    getRecordes(),
  ]);
  return (
    <HistoricoDashboard
      corridas={corridas}
      recordes={recordes}
      filtro={filtro ?? 'TODOS'}
    />
  );
}
