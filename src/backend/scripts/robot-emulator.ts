
/**
 * Mock Emulador do Micromouse Físico
 *
 * Simula o hardware do robô conectando ao Gateway WebSocket em
 * ws://localhost:3001/ingest e transmitindo eventos e telemetria.
 *
 * Execução:
 *   npm run robot:emulator
 *   npm run robot:emulator -- --hz 10 --steps 120 --loop
 *   npm run robot:emulator -- --maze 8x4 --battery 75
 *   npm run robot:emulator -- --battery 50 --battery-drain 0.2
 */

import WebSocket from 'ws';

interface EmulatorConfig {
  url: string;
  hz: number;
  robotId: string;
  totalSteps: number;
  loop: boolean;
  maze: '4x4' | '8x4' | '12x4';
  battery: number;
  batteryDrain: number;
}

type TrajectoryPoint = {
  x: number;
  y: number;
  heading: 'N' | 'E' | 'S' | 'W';
  walls: { n?: boolean; e?: boolean; s?: boolean; w?: boolean };
};

// Trajetórias simuladas para cada dimensão de labirinto
const trajectories: Record<'4x4' | '8x4' | '12x4', TrajectoryPoint[]> = {
  '4x4': [
    { x: 0, y: 0, heading: 'N', walls: { w: true, s: true, e: false, n: false } },
    { x: 0, y: 1, heading: 'N', walls: { w: true, e: true, s: false, n: false } },
    { x: 0, y: 2, heading: 'N', walls: { w: true, n: true, e: false, s: false } },
    { x: 1, y: 2, heading: 'E', walls: { n: true, s: false, w: false, e: false } },
    { x: 2, y: 2, heading: 'E', walls: { s: true, n: false, w: false, e: false } },
    { x: 2, y: 3, heading: 'N', walls: { w: true, n: true, e: false, s: false } },
    { x: 3, y: 3, heading: 'E', walls: { n: true, e: true, w: false, s: false } },
  ],
  '8x4': [
    { x: 0, y: 0, heading: 'N', walls: { w: true, s: true, e: false, n: false } },
    { x: 0, y: 1, heading: 'N', walls: { w: true, e: true, s: false, n: false } },
    { x: 1, y: 1, heading: 'E', walls: { n: false, s: true, w: false, e: false } },
    { x: 2, y: 1, heading: 'E', walls: { n: true, s: false, w: false, e: false } },
    { x: 3, y: 1, heading: 'E', walls: { n: false, s: true, w: false, e: false } },
    { x: 4, y: 2, heading: 'N', walls: { w: true, e: false, s: false, n: false } },
    { x: 5, y: 2, heading: 'E', walls: { n: true, s: false, w: false, e: false } },
    { x: 6, y: 3, heading: 'N', walls: { w: true, e: false, s: false, n: false } },
    { x: 7, y: 3, heading: 'E', walls: { n: true, e: true, w: false, s: false } },
  ],
  '12x4': [
    { x: 0, y: 0, heading: 'N', walls: { w: true, s: true, e: false, n: false } },
    { x: 0, y: 1, heading: 'N', walls: { w: true, e: true, s: false, n: false } },
    { x: 2, y: 1, heading: 'E', walls: { n: false, s: true, w: false, e: false } },
    { x: 4, y: 2, heading: 'E', walls: { n: true, s: false, w: false, e: false } },
    { x: 6, y: 2, heading: 'E', walls: { n: false, s: true, w: false, e: false } },
    { x: 8, y: 3, heading: 'N', walls: { w: true, e: false, s: false, n: false } },
    { x: 10, y: 3, heading: 'E', walls: { n: false, s: true, w: false, e: false } },
    { x: 11, y: 3, heading: 'E', walls: { n: true, e: true, w: false, s: false } },
  ],
};

// Configurações padrão e parse de argumentos CLI
function parseArgs(): EmulatorConfig {
  const args = process.argv.slice(2);

  const getArg = (flag: string, fallback: string): string => {
    const idx = args.indexOf(flag);
    return idx !== -1 && args[idx + 1] ? args[idx + 1] : fallback;
  };

  const getNumberArg = (flag: string, fallback: number): number => {
    const rawValue = getArg(flag, String(fallback));
    const value = Number(rawValue);

    if (!Number.isFinite(value)) {
      throw new Error(
        `Valor inválido para ${flag}: "${rawValue}". Informe um número válido.`,
      );
    }

    return value;
  };

  const rawMaze = getArg('--maze', '4x4');
  const maze = (['4x4', '8x4', '12x4'].includes(rawMaze) ? rawMaze : '4x4') as
    | '4x4'
    | '8x4'
    | '12x4';

  const battery = getNumberArg('--battery', 100);
  const batteryDrain = getNumberArg('--battery-drain', 0.05);
  const hz = getNumberArg('--hz', 10);
  const totalSteps = getNumberArg('--steps', 100);

  if (battery < 0 || battery > 100) {
    throw new Error('--battery deve estar entre 0 e 100.');
  }

  if (batteryDrain < 0) {
    throw new Error('--battery-drain não pode ser negativo.');
  }

  if (hz < 1 || hz > 50) {
    throw new Error('--hz deve estar entre 1 e 50.');
  }

  if (!Number.isInteger(totalSteps) || totalSteps < 1) {
    throw new Error('--steps deve ser um número inteiro maior que zero.');
  }

  return {
    url: getArg(
      '--url',
      process.env.TELEMETRY_WS_URL ?? 'ws://localhost:3001/ingest',
    ),
    hz,
    robotId: getArg('--robot-id', 'PIH_02_MOUSE'),
    totalSteps,
    loop: args.includes('--loop'),
    maze,
    battery,
    batteryDrain,
  };
}

const config = parseArgs();
const intervalMs = Math.round(1000 / config.hz);
const trajectory = trajectories[config.maze];

async function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function runSimulation(): Promise<void> {
  console.log('='.repeat(65));
  console.log('🤖 MOCK EMULADOR DO MICROMOUSE (HARDWARE EMULATOR)');
  console.log('='.repeat(65));
  console.log(`📡 Alvo WebSocket: ${config.url}`);
  console.log(`⏱️ Frequência:     ${config.hz} Hz (intervalo: ${intervalMs} ms)`);
  console.log(`🆔 Robot ID:       ${config.robotId}`);
  console.log(`🔄 Modo Loop:      ${config.loop ? 'Ativado' : 'Desativado'}`);
  console.log(`🗺️ Labirinto:       ${config.maze}`);
  console.log(`🏁 Passos/Ciclo:   ${config.totalSteps}`);
  console.log(`🔋 Bateria inicial: ${config.battery}%`);
  console.log(`📉 Consumo/passo:  ${config.batteryDrain} pontos percentuais`);
  console.log('='.repeat(65));

  let keepRunning = true;

  process.on('SIGINT', () => {
    console.log('\n🛑 Interrupção manual recebida. Finalizando emulador...');
    keepRunning = false;
  });

  do {
    await simulateSingleRun();

    if (config.loop && keepRunning) {
      console.log('\n⏳ Aguardando 3 segundos antes de iniciar novo ciclo de corrida...');
      await sleep(3000);
    }
  } while (config.loop && keepRunning);

  console.log('\n✅ Emulador finalizado com sucesso.');
  process.exit(0);
}

function simulateSingleRun(): Promise<void> {
  return new Promise<void>((resolve) => {
    let seq = 1;
    let startTime = Date.now();
    let totalDistanceMm = 0;
    let battery = config.battery;
    let timer: NodeJS.Timeout | null = null;
    let settled = false;

    const finish = () => {
      if (settled) return;
      settled = true;

      if (timer) clearInterval(timer);

      resolve();
    };

    console.log(`\n[WS] Tentando conectar a ${config.url}...`);
    const ws = new WebSocket(config.url);

    ws.on('open', () => {
      console.log('🔌 Conectado ao Gateway WebSocket com sucesso!\n');
      startTime = Date.now();

      // 1. Evento de calibração concluída
      const calibPayload = {
        v: 1,
        type: 'event',
        robot_id: config.robotId,
        seq: seq++,
        t_ms: 0,
        event: 'CALIBRATION_DONE',
        ok: true,
        battery_pct: battery,
      };

      ws.send(JSON.stringify(calibPayload));

      console.log(
        `🔧 [EVENT] CALIBRATION_DONE enviado (seq=${calibPayload.seq})`,
      );

      // 2. Evento de início de corrida
      const startPayload = {
        v: 1,
        type: 'event',
        robot_id: config.robotId,
        seq: seq++,
        t_ms: 50,
        event: 'RUN_START',
        maze: config.maze,
        battery_pct: battery,
        pos: { x: trajectory[0].x, y: trajectory[0].y },
      };

      ws.send(JSON.stringify(startPayload));

      console.log(`🟢 [EVENT] RUN_START enviado (seq=${startPayload.seq})`);

      let currentStep = 0;

      timer = setInterval(() => {
        currentStep++;

        const elapsedMs = Date.now() - startTime;

        // Avança na trajetória com base no progresso
        const trajIndex = Math.min(
          trajectory.length - 1,
          Math.floor((currentStep / config.totalSteps) * trajectory.length),
        );

        const cell = trajectory[trajIndex];

        // Variações realistas de velocidade (250 a 450 mm/s) e ruído leve
        const speed = Math.round(
          300 + Math.sin(currentStep * 0.2) * 80 + (Math.random() * 20 - 10),
        );

        totalDistanceMm += Math.round((speed * intervalMs) / 1000);

        // Consome a bateria configurada, sem ficar abaixo de zero.
        // Duas casas decimais permitem representar consumos como 0.05.
        battery = Math.max(
          0,
          Math.round((battery - config.batteryDrain) * 100) / 100,
        );

        const isGoalStep = currentStep >= config.totalSteps;
        const state = isGoalStep ? 'GOAL_REACHED' : 'EXPLORING';

        // 3. Pacote de telemetria
        const telemetryPayload = {
          v: 1,
          type: 'telemetry',
          robot_id: config.robotId,
          seq: seq++,
          t_ms: elapsedMs,
          pos: { x: cell.x, y: cell.y },
          heading: cell.heading,
          speed_mm_s: speed,
          odo_mm: totalDistanceMm,
          battery_pct: battery,
          state,
          walls: cell.walls,
        };

        ws.send(JSON.stringify(telemetryPayload));

        if (currentStep % 10 === 0 || isGoalStep) {
          console.log(
            `📡 [TELEMETRIA] #${String(currentStep).padStart(3, '0')} | ` +
              `t=${(elapsedMs / 1000).toFixed(1)}s | ` +
              `seq=${telemetryPayload.seq} | ` +
              `pos=(${cell.x},${cell.y}) ${cell.heading} | ` +
              `vel=${speed} mm/s | ` +
              `odo=${totalDistanceMm} mm | ` +
              `bat=${battery}%`,
          );
        }

        // 4. Ao atingir o total de passos, finaliza a corrida
        if (isGoalStep) {
          if (timer) {
            clearInterval(timer);
            timer = null;
          }

          // Evento de chegada
          const goalPayload = {
            v: 1,
            type: 'event',
            robot_id: config.robotId,
            seq: seq++,
            t_ms: Date.now() - startTime,
            event: 'GOAL_REACHED',
            battery_pct: battery,
            pos: { x: cell.x, y: cell.y },
          };

          ws.send(JSON.stringify(goalPayload));

          console.log(
            `🏆 [EVENT] GOAL_REACHED enviado! (seq=${goalPayload.seq})`,
          );

          // Evento de fim de corrida
          const endPayload = {
            v: 1,
            type: 'event',
            robot_id: config.robotId,
            seq: seq++,
            t_ms: Date.now() - startTime,
            event: 'RUN_END',
            battery_pct: battery,
          };

          ws.send(JSON.stringify(endPayload));

          console.log(`🏁 [EVENT] RUN_END enviado (seq=${endPayload.seq})`);

          setTimeout(() => {
            ws.close();
            finish();
          }, 500);
        }
      }, intervalMs);
    });

    ws.on('error', (err) => {
      console.error(`❌ Erro no socket WebSocket: ${err.message}`);
      finish();
    });

    ws.on('close', (code, reason) => {
      console.log(
        `🔌 Conexão encerrada pelo servidor (código: ${code}, motivo: ${reason.toString() || 'normal'}).`,
      );

      finish();
    });
  });
}

runSimulation().catch((err) => {
  console.error('Falha crítica na execução do emulador:', err);
  process.exit(1);
});
