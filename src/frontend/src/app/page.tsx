'use client';

import { useEffect, useState } from 'react';
import { Battery } from '@/components/Battery';
import { ChallengeFlag } from '@/components/ChallengeFlag';
import { MazeLegend } from '@/components/MazeLegend';
import { MazeMap } from '@/components/MazeMap';
import { MazeType } from '@/components/MazeType';
import { PainelResultados } from '@/components/Results';
import { Speed } from '@/components/Speed';
import { Time } from '@/components/Time';
import { HeadingDirection, MazeDimension, mazeMocks } from '@/lib/mock-data';

interface LiveTelemetryPayload {
  v: number;
  type: 'telemetry';
  robot_id: string;
  seq: number;
  t_ms: number;
  pos: { x: number; y: number };
  heading: HeadingDirection;
  speed_mm_s: number;
  odo_mm?: number;
  battery_pct: number;
  state: string;
}

export default function Monitoramento() {
  const [selectedMaze, setSelectedMaze] = useState<MazeDimension>('4x4');
  const [isSimulating, setIsSimulating] = useState<boolean>(false);
  const [currentStep, setCurrentStep] = useState<number>(0);

  // Estados de telemetria ao vivo
  const [isLiveActive, setIsLiveActive] = useState<boolean>(false);
  const [livePos, setLivePos] = useState<{ x: number; y: number }>({ x: 0, y: 0 });
  const [liveHeading, setLiveHeading] = useState<HeadingDirection>('N');
  const [livePath, setLivePath] = useState<Array<{ x: number; y: number }>>([{ x: 0, y: 0 }]);
  const [liveSpeed, setLiveSpeed] = useState<number>(0);
  const [liveTime, setLiveTime] = useState<number>(0);
  const [liveBattery, setLiveBattery] = useState<number>(0);
  const [liveGoalReached, setLiveGoalReached] = useState<boolean>(false);

  const baseData = mazeMocks[selectedMaze];
  const maxSteps = baseData.path.length;

  // Conexão WebSocket para telemetria em tempo real
  useEffect(() => {
    if (typeof window === 'undefined') return;

    // 1. Determina a URL dinâmica do WebSocket
    let wsUrl = 'ws://localhost:3002/live';

    if (window.location.hostname.includes('.app.github.dev')) {
      // Ajusta a rota para o domínio seguro e porta pública do Codespaces
      const codespaceHost = window.location.hostname.replace('-3000.', '-3002.');
      wsUrl = `wss://${codespaceHost}/live`;
    }

    console.log('[Live WS] Tentando conectar em:', wsUrl);
    const ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      console.log('[Live WS] Conexão WebSocket estabelecida com sucesso!');
    };

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data);
        console.log('[Live WS] Mensagem recebida:', msg);

        if (msg.channel === 'event') {
          if (msg.event === 'RUN_START') {
            setIsLiveActive(true);
            setIsSimulating(false);
            setLiveGoalReached(false);
            setLivePath([{ x: 0, y: 0 }]);
            setLivePos({ x: 0, y: 0 });
            setLiveHeading('N');
          } else if (msg.event === 'GOAL_REACHED') {
            setLiveGoalReached(true);
          } else if (msg.event === 'RUN_END') {
            // Fim de corrida
          }
        }

        if (msg.channel === 'telemetry') {
          const telemetry: LiveTelemetryPayload = msg.data;
          setIsLiveActive(true);
          setIsSimulating(false);

          setLivePos(telemetry.pos);
          setLiveHeading(telemetry.heading);
          // Converte mm/s para m/s para compatibilidade visual do componente Speed
          setLiveSpeed(Number((telemetry.speed_mm_s / 1000).toFixed(2)));
          setLiveTime(Math.round(telemetry.t_ms / 1000));
          // Consumo estimado da bateria: 100 - restante
          setLiveBattery(Math.max(0, Math.round(100 - telemetry.battery_pct)));

          setLivePath((prev) => {
            const last = prev[prev.length - 1];
            if (!last || last.x !== telemetry.pos.x || last.y !== telemetry.pos.y) {
              return [...prev, telemetry.pos];
            }
            return prev;
          });
        }
      } catch (err) {
        console.error('[Live WS] Erro ao processar pacote de telemetria:', err);
      }
    };

    ws.onerror = (err) => {
      console.error('[Live WS] Erro na conexão WebSocket:', err);
    };

    ws.onclose = (event) => {
      console.warn(`[Live WS] Conexão encerrada (código ${event.code}).`);
    };

    return () => {
      ws.close();
    };
  }, []);

  // Simulação manual local (fallback caso o robô não esteja transmitindo)
  useEffect(() => {
    if (!isSimulating) return;

    const interval = setInterval(() => {
      setCurrentStep((prev) => {
        if (prev + 1 >= maxSteps) {
          setIsSimulating(false);
          return maxSteps - 1;
        }
        return prev + 1;
      });
    }, 450);

    return () => clearInterval(interval);
  }, [isSimulating, maxSteps]);

  // Escolha entre valores ao vivo ou valores do mock/simulação
  const effectiveStep = isSimulating ? currentStep : maxSteps - 1;
  const currentPath = isLiveActive
    ? livePath
    : isSimulating
      ? baseData.path.slice(0, effectiveStep + 1)
      : baseData.path;

  const currentPos = isLiveActive
    ? livePos
    : isSimulating
      ? baseData.path[effectiveStep] ?? baseData.path[0]
      : baseData.path[maxSteps - 1] ?? baseData.path[0];

  const getSimulatedHeading = (stepIdx: number): HeadingDirection => {
    if (stepIdx === 0) return 'N';
    const prev = baseData.path[stepIdx - 1];
    const curr = baseData.path[stepIdx];
    if (!prev || !curr) return 'N';
    if (curr.x > prev.x) return 'E';
    if (curr.x < prev.x) return 'W';
    if (curr.y > prev.y) return 'N';
    if (curr.y < prev.y) return 'S';
    return baseData.robotPosition.heading;
  };

  const currentHeading = isLiveActive
    ? liveHeading
    : isSimulating
      ? getSimulatedHeading(effectiveStep)
      : baseData.robotPosition.heading;

  const currentSpeed = isLiveActive
    ? liveSpeed
    : isSimulating
      ? Math.round((2.0 + (effectiveStep / maxSteps) * 1.5) * 10) / 10
      : baseData.speed;

  const currentTime = isLiveActive
    ? liveTime
    : isSimulating
      ? Math.round((effectiveStep / maxSteps) * baseData.completionTime)
      : baseData.completionTime;

  const currentBatteryConsumo = isLiveActive
    ? liveBattery
    : isSimulating
      ? Math.round((effectiveStep / maxSteps) * baseData.batteryConsumption)
      : baseData.batteryConsumption;

  const currentChallengeCompleted = isLiveActive
    ? liveGoalReached
    : isSimulating
      ? effectiveStep === maxSteps - 1
      : baseData.challengeCompleted;

  const handleSelectDimension = (dim: MazeDimension) => {
    setSelectedMaze(dim);
    setIsSimulating(false);
    setIsLiveActive(false);
    setCurrentStep(0);
  };

  const handleToggleSimulation = () => {
    setIsLiveActive(false);
    if (isSimulating) {
      setIsSimulating(false);
    } else {
      setCurrentStep(0);
      setIsSimulating(true);
    }
  };

  return (
    <div className="flex flex-col xl:flex-row xl:items-start justify-between gap-5">
      {/* 1. Painel de Legenda & Seleção de Labirinto */}
      <MazeLegend
        currentDimension={selectedMaze}
        onSelectDimension={handleSelectDimension}
        isSimulating={isSimulating}
        onToggleSimulation={handleToggleSimulation}
      />

      {/* 2. Visualização Gráfica do Labirinto em SVG */}
      <MazeMap
        dimension={selectedMaze}
        robotPosition={{
          x: currentPos.x,
          y: currentPos.y,
          heading: currentHeading,
        }}
        path={currentPath}
        discoveredWalls={baseData.discoveredWalls}
      />

      {/* 3. Painel Lateral de Resultados da Telemetria */}
      <PainelResultados>
        <ChallengeFlag completed={currentChallengeCompleted} />
        <Battery consumo={currentBatteryConsumo} />
        <MazeType type={selectedMaze} />
        <Speed speed={currentSpeed} />
        <Time time={currentTime} />
      </PainelResultados>
    </div>
  );
}