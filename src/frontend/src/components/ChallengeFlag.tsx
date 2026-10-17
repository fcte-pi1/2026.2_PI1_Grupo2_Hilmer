interface ChallengeFlagProps {
  completed: boolean;
}

export function ChallengeFlag({ completed }: ChallengeFlagProps) {
  return (
    <div>
      <h2 className="text-xl text-white">
        Desafio cumprido: {completed ? 'S' : 'N'}{' '}
      </h2>
    </div>
  );
}
