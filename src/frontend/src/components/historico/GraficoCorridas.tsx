'use client';

import {
  Bar,
  BarChart,
  CartesianGrid,
  Line,
  LineChart,
  ResponsiveContainer,
  Tooltip,
  XAxis,
  YAxis,
  type TooltipContentProps,
} from 'recharts';

export interface PontoGrafico {
  /** Rótulo do eixo X, ex.: "#3". */
  corrida: string;
  labirinto: string;
  valor: number;
}

interface GraficoCorridasProps {
  titulo: string;
  dados: PontoGrafico[];
  tipo: 'barra' | 'linha';
  cor: string;
  formatarValor: (valor: number) => string;
}

const COR_EIXO = 'rgba(255, 255, 255, 0.6)';
const COR_GRADE = 'rgba(255, 255, 255, 0.1)';

export function GraficoCorridas({
  titulo,
  dados,
  tipo,
  cor,
  formatarValor,
}: GraficoCorridasProps) {
  const eixos = (
    <>
      <CartesianGrid vertical={false} stroke={COR_GRADE} />
      <XAxis
        dataKey="corrida"
        tick={{ fill: COR_EIXO, fontSize: 12 }}
        axisLine={{ stroke: COR_GRADE }}
        tickLine={false}
      />
      <YAxis
        tick={{ fill: COR_EIXO, fontSize: 12 }}
        tickFormatter={formatarValor}
        axisLine={false}
        tickLine={false}
        width={72}
      />
      <Tooltip
        content={(props) => (
          <DicaCorrida {...props} formatarValor={formatarValor} />
        )}
        cursor={{ fill: COR_GRADE, stroke: COR_GRADE }}
      />
    </>
  );

  return (
    // Fundo escuro no gráfico: as cores das séries não têm contraste 3:1 sobre #393A99.
    <section className="rounded-xl overflow-hidden bg-[#172860] font-mono">
      <h2 className="bg-[#393A99] px-4 py-2 text-lg">{titulo}</h2>
      <div className="h-64 p-4">
        {dados.length === 0 ? (
          <p className="h-full grid place-items-center opacity-60">
            Sem corridas para exibir
          </p>
        ) : (
          <ResponsiveContainer width="100%" height="100%">
            {tipo === 'barra' ? (
              <BarChart data={dados}>
                {eixos}
                <Bar
                  dataKey="valor"
                  fill={cor}
                  radius={[4, 4, 0, 0]}
                  maxBarSize={32}
                />
              </BarChart>
            ) : (
              <LineChart data={dados}>
                {eixos}
                <Line
                  dataKey="valor"
                  stroke={cor}
                  strokeWidth={2}
                  dot={{ r: 4, fill: cor, strokeWidth: 0 }}
                  activeDot={{ r: 6, stroke: '#172860', strokeWidth: 2 }}
                />
              </LineChart>
            )}
          </ResponsiveContainer>
        )}
      </div>
    </section>
  );
}

function DicaCorrida({
  active,
  payload,
  formatarValor,
}: Pick<TooltipContentProps, 'active' | 'payload'> & {
  formatarValor: (valor: number) => string;
}) {
  if (!active || !payload?.length) return null;
  const ponto = payload[0].payload as PontoGrafico;
  return (
    <div className="rounded bg-[#13143C] px-3 py-2 text-sm font-mono shadow-lg">
      <p className="opacity-70">
        Corrida {ponto.corrida} · {ponto.labirinto}
      </p>
      <p className="text-base">{formatarValor(ponto.valor)}</p>
    </div>
  );
}
