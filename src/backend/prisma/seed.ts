import { PrismaClient } from '@prisma/client';

const prisma = new PrismaClient();

async function main() {
  console.log('Iniciando execução do Seed...');

  // 1. Limpeza prévia para garantir idempotência em ambiente de desenvolvimento
  await prisma.telemetria.deleteMany({});
  await prisma.corrida.deleteMany({});

  // 2. Criação/Upsert dos Labirintos
  const dimensoes = ['4x4', '8x4', '12x4'];
  const labirintosMap: Record<string, string> = {};

  for (const dimensao of dimensoes) {
    const lab = await prisma.labirinto.upsert({
      where: { dimensao },
      update: {},
      create: { dimensao },
    });
    labirintosMap[dimensao] = lab.id;
  }
  console.log('Labirintos prontos: 4x4, 8x4 e 12x4.');

  const now = new Date();

  // Helper para criar corridas e amostras de telemetria
  const criarCorridaComTelemetria = async (params: {
    dimensao: '4x4' | '8x4' | '12x4';
    status: 'CONCLUIDA' | 'EM_ANDAMENTO' | 'FALHA';
    duracaoSegundos?: number;
    offsetMinutosAtras: number;
    amostrasQtd: number;
  }) => {
    const iniciadaEm = new Date(
      now.getTime() - params.offsetMinutosAtras * 60 * 1000,
    );
    const finalizadaEm =
      params.duracaoSegundos && params.status !== 'EM_ANDAMENTO'
        ? new Date(iniciadaEm.getTime() + params.duracaoSegundos * 1000)
        : null;

    const corrida = await prisma.corrida.create({
      data: {
        labirintoId: labirintosMap[params.dimensao],
        status: params.status,
        iniciadaEm,
        finalizadaEm,
      },
    });

    // Gera amostras sequenciais de telemetria
    const telemetriasData = [];
    let posX = 0.0;
    let posY = 0.0;
    const direcoes = ['N', 'E', 'S', 'W'];

    for (let seq = 0; seq < params.amostrasQtd; seq++) {
      const stepTimestamp = new Date(iniciadaEm.getTime() + seq * 100); // amostragem a cada 100ms
      const direcao = direcoes[seq % direcoes.length];

      if (direcao === 'N') posY += 0.5;
      else if (direcao === 'E') posX += 0.5;
      else if (direcao === 'S') posY = Math.max(0, posY - 0.5);
      else if (direcao === 'W') posX = Math.max(0, posX - 0.5);

      telemetriasData.push({
        corridaId: corrida.id,
        timestamp: stepTimestamp,
        posX: Number(posX.toFixed(2)),
        posY: Number(posY.toFixed(2)),
        direcao,
        sensores: {
          seq,
          v: 1,
          speed_mm_s: Math.round(250 + Math.random() * 50),
          battery_pct: Math.max(10, Number((100 - seq * 0.05).toFixed(1))),
          distancia_frontal_mm: Math.round(80 + Math.random() * 40),
          distancia_esquerda_mm: Math.round(50 + Math.random() * 20),
          distancia_direita_mm: Math.round(50 + Math.random() * 20),
        },
      });
    }

    if (telemetriasData.length > 0) {
      await prisma.telemetria.createMany({
        data: telemetriasData,
      });
    }

    return corrida;
  };

  // 3. Cenários de Corridas

  // Cenário A: Labirinto 4x4 (Recorde rápido de 18.4s)
  await criarCorridaComTelemetria({
    dimensao: '4x4',
    status: 'CONCLUIDA',
    duracaoSegundos: 18.4,
    offsetMinutosAtras: 120,
    amostrasQtd: 25,
  });

  // Cenário B: Labirinto 4x4 (Corrida mais lenta de 24.1s)
  await criarCorridaComTelemetria({
    dimensao: '4x4',
    status: 'CONCLUIDA',
    duracaoSegundos: 24.1,
    offsetMinutosAtras: 90,
    amostrasQtd: 20,
  });

  // Cenário C: Labirinto 8x4 (Recorde concluído em 42.0s)
  await criarCorridaComTelemetria({
    dimensao: '8x4',
    status: 'CONCLUIDA',
    duracaoSegundos: 42.0,
    offsetMinutosAtras: 60,
    amostrasQtd: 30,
  });

  // Cenário D: Labirinto 8x4 (Corrida que falhou/bateu)
  await criarCorridaComTelemetria({
    dimensao: '8x4',
    status: 'FALHA',
    duracaoSegundos: 12.3,
    offsetMinutosAtras: 30,
    amostrasQtd: 15,
  });

  // Cenário E: Labirinto 12x4 (Corrida em andamento)
  await criarCorridaComTelemetria({
    dimensao: '12x4',
    status: 'EM_ANDAMENTO',
    offsetMinutosAtras: 2,
    amostrasQtd: 10,
  });

  console.log('Seed concluído com sucesso! Corridas e telemetrias populadas.');
}

main()
  .catch((e) => {
    console.error('Erro executando o seed:', e);
    process.exit(1);
  })
  .finally(async () => {
    await prisma.$disconnect();
  });
