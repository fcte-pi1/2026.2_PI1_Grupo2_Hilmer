interface BatteryLevelProps {
  level: number;
}

export function BatteryLevel({ level }: BatteryLevelProps) {
  return (
    <div className="flex items-center gap-3 font-mono">
      <span className="text-2xl">{level}%</span>
      <div className="h-10 w-24 rounded border-2 border-white">
        <div className="h-full bg-green-400" style={{ width: `${level}%` }} />
      </div>
    </div>
  );
}
