#!/usr/bin/env python3
"""
Mock do robô Micromouse.

Simula o robô percorrendo o labirinto até o objetivo e transmite telemetria
em tempo real via WebSocket ou stdout conforme o contrato do backend.

Uso:
    python mocks/robot_mock.py --maze 4x4
    python mocks/robot_mock.py --maze 8x4 --rate 10 --stdout
    python mocks/robot_mock.py --maze 12x4 --url ws://localhost:3001/ingest
"""
import argparse
import json
import random
import sys
import time

# Caminhos pré-definidos (células da largada até o objetivo) para cada dimensão
PATHS = {
    "4x4": [(0, 0), (0, 1), (0, 2), (1, 2), (1, 1), (2, 1), (3, 1), (3, 2), (3, 3)],
    "8x4": [(0, 0), (1, 0), (2, 0), (2, 1), (1, 1), (1, 2), (0, 2), (0, 3), (1, 3), (2, 3),
            (3, 3), (4, 3), (5, 3), (5, 2), (6, 2), (6, 3), (7, 3)],
    "12x4": [(0, 0), (0, 1), (0, 2), (0, 3), (1, 3), (2, 3), (2, 2), (2, 1), (1, 1), (1, 0),
             (2, 0), (3, 0), (4, 0), (5, 0), (6, 0), (7, 0), (7, 1), (7, 2), (8, 2), (9, 2),
             (9, 3), (10, 3), (11, 3)],
}

HEADINGS = {(0, 1): "N", (1, 0): "E", (0, -1): "S", (-1, 0): "W"}
CELL_MM = 180.0  # Tamanho padrão da célula do labirinto em milímetros


def build_run(path, rate_hz=10.0, robot_id="mm-01"):
    """Gera a sequência cronológica completa de mensagens da corrida."""
    tick_s = 1.0 / rate_hz
    tick_ms = int(tick_s * 1000)

    msgs = []
    seq = 0
    t_ms = 0
    odo = 0.0
    battery = round(random.uniform(96.0, 100.0), 1)

    def add(**fields):
        nonlocal seq
        msgs.append({
            "v": 1,
            "robot_id": robot_id,
            "seq": seq,
            "t_ms": t_ms,
            **fields
        })
        seq += 1

    def telemetry(x, y, heading, speed, state):
        add(
            type="telemetry",
            pos={"x": x, "y": y},
            heading=heading,
            speed_mm_s=round(speed, 1),
            odo_mm=int(odo),
            battery_pct=round(max(0.0, battery), 1),
            state=state,
        )

    # 1. Evento de Calibração
    add(type="event", event="CALIBRATION_DONE", ok=True)
    t_ms += 200

    # 2. Início oficial da corrida
    add(type="event", event="RUN_START", battery_pct=battery)
    t_ms += 100

    # 3. Navegação passo a passo pelas células do trajeto
    heading = "N"
    for (x, y), (nx, ny) in zip(path, path[1:]):
        heading = HEADINGS[(nx - x, ny - y)]
        walked = 0.0
        while walked < CELL_MM:
            speed = random.uniform(280.0, 320.0)  # Velocidade média em mm/s com variação
            step = min(speed * tick_s, CELL_MM - walked)
            walked += step
            odo += step
            t_ms += tick_ms
            battery -= 0.02

            # Atualiza a coordenada ao ultrapassar a metade da célula
            cx, cy = (x, y) if walked < CELL_MM / 2.0 else (nx, ny)
            telemetry(cx, cy, heading, speed, "EXPLORING")

    # 4. Chegada ao objetivo final
    gx, gy = path[-1]
    add(type="event", event="GOAL_REACHED", pos={"x": gx, "y": gy})
    t_ms += 100
    telemetry(gx, gy, heading, 0.0, "GOAL_REACHED")

    # 5. Conclusão da corrida
    t_ms += 100
    add(type="event", event="RUN_END")

    return msgs


def play(msgs, send, rate_hz=10.0):
    """Transmite os pacotes no ritmo real definido pela frequência em Hz."""
    tick_s = 1.0 / rate_hz
    next_tick = time.monotonic()
    for msg in msgs:
        send(msg)
        if msg.get("type") == "telemetry":
            next_tick += tick_s
            time.sleep(max(0.0, next_tick - time.monotonic()))


def main():
    parser = argparse.ArgumentParser(description="Mock do robô Micromouse")
    parser.add_argument(
        "--maze",
        choices=list(PATHS.keys()),
        default="4x4",
        help="Labirinto a percorrer (4x4, 8x4, 12x4)"
    )
    parser.add_argument(
        "--rate",
        type=float,
        default=10.0,
        help="Taxa de envio em Hz (padrão: 10 Hz)"
    )
    parser.add_argument(
        "--url",
        default="ws://localhost:3001/ingest",
        help="URL do WebSocket de ingestão"
    )
    parser.add_argument(
        "--stdout",
        action="store_true",
        help="Exibe os JSONs no terminal em vez de enviar via WebSocket"
    )
    args = parser.parse_args()

    msgs = build_run(PATHS[args.maze], rate_hz=args.rate)

    if args.stdout:
        play(msgs, lambda msg: print(json.dumps(msg), flush=True), rate_hz=args.rate)
    else:
        try:
            from websockets.sync.client import connect
        except ImportError:
            sys.exit("Erro: pacote 'websockets' não instalado. Instale com: pip install websockets")

        try:
            with connect(args.url) as ws:
                play(msgs, lambda msg: ws.send(json.dumps(msg)), rate_hz=args.rate)
        except OSError as exc:
            sys.exit(f"Não foi possível conectar em {args.url}: {exc}")

    print(
        f"Corrida no labirinto {args.maze} finalizada com sucesso ({len(msgs)} pacotes transmitidos a {args.rate} Hz).",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()