interface ConnectionProps {
  connected: boolean;
}

export function Connection({ connected }: ConnectionProps) {
  return (
    <div className="flex items-center gap-3">
      <span
        className={
          connected
            ? 'w-4 h-4 rounded-full bg-green-400'
            : 'w-4 h-4 rounded-full bg-red-500'
        }
      />
      <span className="font-mono text-2xl ">
        {connected ? 'Conectado' : 'Desconectado'}{' '}
      </span>
    </div>
  );
}
