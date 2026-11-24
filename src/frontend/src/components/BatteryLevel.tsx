interface BatteryLevelProps {
  level: number;
}

export function getBatteryColor(level: number): string {
  if (level <= 20) return 'bg-red-500';
  if (level <= 50) return 'bg-yellow-400';
  return 'bg-green-400';
}

export function BatteryLevel({ level }: BatteryLevelProps) {
  const clampedLevel = Math.max(0, Math.min(100, level));
  const colorClass = getBatteryColor(clampedLevel);

  return (
    <div className="flex items-center gap-3 font-mono">
      <span className="text-2xl">{level}%</span>
      <div className="h-10 w-24 rounded border-2 border-white overflow-hidden">
        <div
          data-testid="battery-level-bar"
          className={`h-full transition-all duration-300 ${colorClass}`}
          style={{ width: `${clampedLevel}%` }}
        />
      </div>
    </div>
  );
}
