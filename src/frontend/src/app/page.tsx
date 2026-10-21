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

export default function Monitoramento() {
  const [selectedMaze, setSelectedMaze] = useState<MazeDimension>('4x4');
  const [isSimulating, setIsSimulating] = useState<boolean>(false);
  const [currentStep, setCurrentStep] = useState<number>(0);

  const baseData = mazeMocks[selectedMaze];
  const maxSteps = baseData.path.length;

  // Lógica de simulação passo a passo
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

  // Se estiver simulando, calcula o trajeto parcial e posição intermediária
  const effectiveStep = isSimulating ? currentStep : maxSteps - 1;
  const currentPath = baseData.path.slice(0, effectiveStep + 1);
  const currentPos = baseData.path[effectiveStep] ?? baseData.path[0];

  // Determina orientação com base no vetor de movimento entre o passo anterior e atual
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

  const currentHeading = isSimulating
    ? getSimulatedHeading(effectiveStep)
    : baseData.robotPosition.heading;

  const currentSpeed = isSimulating
    ? Math.round((2.0 + (effectiveStep / maxSteps) * 1.5) * 10) / 10
    : baseData.speed;

  const currentTime = isSimulating
    ? Math.round((effectiveStep / maxSteps) * baseData.completionTime)
    : baseData.completionTime;

  const currentBatteryConsumo = isSimulating
    ? Math.round((effectiveStep / maxSteps) * baseData.batteryConsumption)
    : baseData.batteryConsumption;

  const currentChallengeCompleted = isSimulating
    ? effectiveStep === maxSteps - 1
    : baseData.challengeCompleted;

  const handleSelectDimension = (dim: MazeDimension) => {
    setSelectedMaze(dim);
    setIsSimulating(false);
    setCurrentStep(0);
  };

  const handleToggleSimulation = () => {
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
