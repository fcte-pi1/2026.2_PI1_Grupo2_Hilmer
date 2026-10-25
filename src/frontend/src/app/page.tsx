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
import { useTelemetry } from '@/context/TelemetryContext';

export default function Monitoramento() {
  const [selectedMaze, setSelectedMaze] = useState<MazeDimension>('4x4');
  const [isSimulating, setIsSimulating] = useState<boolean>(false);
  const [currentStep, setCurrentStep] = useState<number>(0);

  const {
    isLiveActive,
    livePos,
    liveHeading,
    livePath,
    liveSpeed,
    liveTime,
    liveBatteryConsumo,
    liveGoalReached,
    resetLiveState,
  } = useTelemetry();

  const baseData = mazeMocks[selectedMaze];
  const maxSteps = baseData.path.length;

  // Se a telemetria ao vivo começar, desativa a simulação manual
  useEffect(() => {
    if (isLiveActive) {
      setIsSimulating(false);
    }
  }, [isLiveActive]);

  // Simulação manual local (fallback)
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
    ? liveBatteryConsumo
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
    resetLiveState();
    setCurrentStep(0);
  };

  const handleToggleSimulation = () => {
    resetLiveState();
    if (isSimulating) {
      setIsSimulating(false);
    } else {
      setCurrentStep(0);
      setIsSimulating(true);
    }
  };

  return (
    <div className="flex flex-col xl:flex-row xl:items-start justify-between gap-5">
      <MazeLegend
        currentDimension={selectedMaze}
        onSelectDimension={handleSelectDimension}
        isSimulating={isSimulating}
        onToggleSimulation={handleToggleSimulation}
      />

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