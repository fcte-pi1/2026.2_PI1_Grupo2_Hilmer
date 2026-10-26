import { act, render, screen } from '@testing-library/react';
import {
  RECONNECT_DELAY_MS,
  ROBOT_TIMEOUT_MS,
  TelemetryProvider,
  resolveLiveUrl,
  useTelemetry,
} from '@/context/TelemetryContext';

// WebSocket falso: guarda as instâncias para o teste disparar open/message/close.
class FakeWebSocket {
  static instances: FakeWebSocket[] = [];
  url: string;
  onopen: (() => void) | null = null;
  onmessage: ((event: { data: string }) => void) | null = null;
  onerror: ((err: unknown) => void) | null = null;
  onclose: (() => void) | null = null;
  close = jest.fn();

  constructor(url: string) {
    this.url = url;
    FakeWebSocket.instances.push(this);
  }

  receive(msg: object) {
    this.onmessage?.({ data: JSON.stringify(msg) });
  }
}

const latest = () =>
  FakeWebSocket.instances[FakeWebSocket.instances.length - 1];

const telemetry = (x: number, y: number, walls?: object) => ({
  channel: 'telemetry',
  data: {
    v: 1,
    type: 'telemetry',
    robot_id: 'PIH_02_MOUSE',
    seq: 3,
    t_ms: 2500,
    pos: { x, y },
    heading: 'E',
    speed_mm_s: 320,
    battery_pct: 79.6,
    walls,
    state: 'EXPLORING',
  },
});

// Mostra o estado do contexto em texto para as asserções.
function Sonda() {
  const t = useTelemetry();
  return (
    <ul>
      <li>conectado:{String(t.isConnected)}</li>
      <li>ativo:{String(t.isLiveActive)}</li>
      <li>labirinto:{t.liveMaze ?? 'nenhum'}</li>
      <li>bateria:{t.batteryLevel}</li>
      <li>velocidade:{t.liveSpeed}</li>
      <li>tempo:{t.liveTime}</li>
      <li>trajeto:{t.livePath.length}</li>
      <li>paredes:{JSON.stringify(t.liveWalls)}</li>
    </ul>
  );
}

const renderProvider = () =>
  render(
    <TelemetryProvider>
      <Sonda />
    </TelemetryProvider>,
  );

describe('TelemetryContext', () => {
  const originalWebSocket = global.WebSocket;

  beforeEach(() => {
    jest.useFakeTimers();
    FakeWebSocket.instances = [];
    global.WebSocket = FakeWebSocket as unknown as typeof WebSocket;
    jest.spyOn(console, 'log').mockImplementation(() => {});
    jest.spyOn(console, 'error').mockImplementation(() => {});
  });

  afterEach(() => {
    jest.useRealTimers();
    global.WebSocket = originalWebSocket;
    delete process.env.NEXT_PUBLIC_LIVE_URL;
    jest.restoreAllMocks();
  });

  describe('resolveLiveUrl', () => {
    it('usa NEXT_PUBLIC_LIVE_URL quando definida', () => {
      process.env.NEXT_PUBLIC_LIVE_URL = 'ws://192.168.0.10:3002/live';
      expect(resolveLiveUrl()).toBe('ws://192.168.0.10:3002/live');
    });

    it('sem a variável, usa o host da página na porta 3002', () => {
      expect(resolveLiveUrl()).toBe('ws://localhost:3002/live');
    });
  });

  it('começa desconectado e abre o /live', () => {
    renderProvider();
    expect(screen.getByText('conectado:false')).toBeInTheDocument();
    expect(latest().url).toBe('ws://localhost:3002/live');
  });

  it('só mostra conectado quando o robô manda dados', () => {
    renderProvider();
    act(() => latest().onopen?.());
    expect(screen.getByText('conectado:false')).toBeInTheDocument();

    act(() => latest().receive(telemetry(0, 0)));
    expect(screen.getByText('conectado:true')).toBeInTheDocument();
  });

  it('volta para desconectado quando o robô para de mandar dados', () => {
    renderProvider();
    act(() => latest().receive(telemetry(0, 0)));

    act(() => jest.advanceTimersByTime(ROBOT_TIMEOUT_MS - 100));
    act(() => latest().receive(telemetry(0, 1)));
    act(() => jest.advanceTimersByTime(ROBOT_TIMEOUT_MS - 100));
    expect(screen.getByText('conectado:true')).toBeInTheDocument();

    act(() => jest.advanceTimersByTime(200));
    expect(screen.getByText('conectado:false')).toBeInTheDocument();
  });

  it('reconecta ao /live depois que a conexão cai', () => {
    renderProvider();
    act(() => latest().receive(telemetry(0, 0)));

    act(() => latest().onclose?.());
    expect(screen.getByText('conectado:false')).toBeInTheDocument();
    expect(FakeWebSocket.instances).toHaveLength(1);

    act(() => jest.advanceTimersByTime(RECONNECT_DELAY_MS));
    expect(FakeWebSocket.instances).toHaveLength(2);
  });

  it('não reconecta depois de desmontado', () => {
    const { unmount } = renderProvider();
    const ws = latest();
    unmount();
    expect(ws.close).toHaveBeenCalled();

    act(() => ws.onclose?.());
    act(() => jest.advanceTimersByTime(RECONNECT_DELAY_MS * 2));
    expect(FakeWebSocket.instances).toHaveLength(1);
  });

  it('usa o labirinto informado no RUN_START', () => {
    renderProvider();
    act(() =>
      latest().receive({
        channel: 'event',
        event: 'RUN_START',
        data: { maze: '8x4', pos: { x: 0, y: 0 } },
      }),
    );
    expect(screen.getByText('ativo:true')).toBeInTheDocument();
    expect(screen.getByText('labirinto:8x4')).toBeInTheDocument();
  });

  it('ignora um labirinto desconhecido no RUN_START', () => {
    renderProvider();
    act(() =>
      latest().receive({
        channel: 'event',
        event: 'RUN_START',
        data: { maze: '16x16' },
      }),
    );
    expect(screen.getByText('labirinto:nenhum')).toBeInTheDocument();
  });

  it('converte bateria, velocidade e tempo da telemetria', () => {
    renderProvider();
    act(() => latest().receive(telemetry(1, 0)));
    expect(screen.getByText('bateria:80')).toBeInTheDocument();
    expect(screen.getByText('velocidade:0.32')).toBeInTheDocument();
    expect(screen.getByText('tempo:3')).toBeInTheDocument();
  });

  it('acumula as paredes por célula e o trajeto sem repetir posição', () => {
    renderProvider();
    act(() => latest().receive(telemetry(0, 0, { s: true, w: true })));
    act(() => latest().receive(telemetry(0, 0, { s: true, w: true })));
    act(() => latest().receive(telemetry(0, 1, { e: true })));

    expect(
      screen.getByText('paredes:{"0,0":{"s":true,"w":true},"0,1":{"e":true}}'),
    ).toBeInTheDocument();
    // [0,0] inicial + (0,1); a repetição de (0,0) não entra
    expect(screen.getByText('trajeto:2')).toBeInTheDocument();
  });

  it('não quebra com mensagem inválida', () => {
    renderProvider();
    act(() => latest().onmessage?.({ data: 'não é json' }));
    expect(screen.getByText('conectado:false')).toBeInTheDocument();
  });
});
