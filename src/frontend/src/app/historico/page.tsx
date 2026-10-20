import { Suspense } from 'react';
import { HistoricoDashboard } from '@/components/historico/HistoricoDashboard';
import { getCorridas } from '@/lib/corridas';

export default function HistoricoPage() {
  return (
    <div className="flex flex-col gap-6">
      <h1 className="text-3xl font-mono">Histórico de corridas</h1>
      <Suspense
        fallback={<p className="font-mono opacity-70">Carregando corridas…</p>}
      >
        <Historico />
      </Suspense>
    </div>
  );
}

async function Historico() {
  const corridas = await getCorridas();
  return <HistoricoDashboard corridas={corridas} />;
}
