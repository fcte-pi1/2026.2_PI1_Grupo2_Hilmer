import { ChallengeFlag } from '@/components/ChallengeFlag';
import { MazeType } from '@/components/MazeType';
import { Speed } from '@/components/Speed';
import { Time } from '@/components/Time';
import { Battery } from '@/components/Battery';
import { mockData } from '@/lib/mock-data';
import { PainelResultados } from '@/components/Results';
import { LiveMap } from '@/components/LiveMap';
import { MapLegend } from '@/components/MapLegend';

export default function Monitoramento() {
  return (
    <div className="flex flex-col lg:flex-row lg:justify-between gap-4">
      <div className="w-full lg:w-64">
        <MapLegend />
      </div>
      <LiveMap maze={mockData.mazeType} />
      <PainelResultados>
        <ChallengeFlag completed={mockData.challengeCompleted} />
        <Battery consumo={mockData.batteryConsumption} />
        <MazeType type={mockData.mazeType} />
        <Speed speed={mockData.speed} />
        <Time time={mockData.completionTime} />
      </PainelResultados>
    </div>
  );
}
