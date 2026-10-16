import { ChallengeFlag } from '@/components/ChallengeFlag';
import { MazeType } from '@/components/MazeType';
import { Speed } from '@/components/Speed';
import { Time } from '@/components/Time';
import { Battery } from '@/components/Battery';
import { mockData } from '@/lib/mock-data';
import { PainelResultados } from '@/components/Results';

export default function Monitoramento() {
  return (
    <div className="flex flex-col lg:flex-row lg:justify-between gap-4">
      <div className="w-full lg:w-64 bg-white/10">Legenda</div>
      <div className="lg:flex-1 w-full aspect-square max-w-200 bg-white/10">
        Mapa
      </div>
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
