interface BatteryProps {
  consumo: number;
}

export function Battery({ consumo }: BatteryProps) {
  return (
    <div>
      <h2 className="text-xl text-white">Consumo de bateria: {consumo}%</h2>
    </div>
  );
}
