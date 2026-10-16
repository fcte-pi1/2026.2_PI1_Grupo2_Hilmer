import type { ReactNode } from 'react';
interface PainelResultadosProps {
  children: ReactNode;
}

export function PainelResultados({ children }: PainelResultadosProps) {
  return (
    <section className="rounded-xl overflow-hidden bg-[#393A99] font-mono text-white w-full lg:w-154">
      <h1 className="bg-[#172860] px-4 py-2 text-xl">Resultados</h1>
      <div className="p-4 flex flex-col gap-4">{children}</div>
    </section>
  );
}
