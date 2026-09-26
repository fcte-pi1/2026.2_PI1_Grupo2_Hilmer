#!/usr/bin/env python3
"""
Mock simples do robô micromouse.

Simula o robô andando por um caminho fixo até o objetivo e envia a telemetria
falsa (JSON) a cada 100 ms, no formato do contrato (docs/arquitetura.md, seção 2).

Uso:
    python robot_mock.py --maze 4x4             # envia para ws://localhost:3001/ingest
    python robot_mock.py --maze 8x4 --stdout    # só imprime o JSON no terminal
"""
import argparse
import json
import random
import sys
import time

# Caminho fixo (células da largada até o objetivo) de cada labirinto.
# (0,0) é o canto inferior esquerdo; y cresce para o norte.
PATHS = {
    "4x4": [(0, 0), (0, 1), (0, 2), (1, 2), (1, 1), (2, 1), (3, 1), (3, 2), (3, 3)],
    "8x4": [(0, 0), (1, 0), (2, 0), (2, 1), (1, 1), (1, 2), (0, 2), (0, 3), (1, 3), (2, 3),
            (3, 3), (4, 3), (5, 3), (5, 2), (6, 2), (6, 3), (7, 3)],
    "12x4": [(0, 0), (0, 1), (0, 2), (0, 3), (1, 3), (2, 3), (2, 2), (2, 1), (1, 1), (1, 0),
             (2, 0), (3, 0), (4, 0), (5, 0), (6, 0), (7, 0), (7, 1), (7, 2), (8, 2), (9, 2),
             (9, 3), (10, 3), (11, 3)],
}

HEADINGS = {(0, 1): "N", (1, 0): "E", (0, -1): "S", (-1, 0): "W"}
CELL_MM = 180     # tamanho de uma célula
TICK_S = 0.1      # 100 ms entre pacotes -> 10 Hz (RNF15)


def build_run(path, robot_id="mm-01"):
    """Gera a lista com todas as mensagens da corrida, em ordem."""
    msgs = []
    seq = 0
    t_ms = 0
    odo = 0.0
    battery = random.uniform(95, 100)

    def add(**fields):
        nonlocal seq
        msgs.append({"v": 1, "robot_id": robot_id, "seq": seq, "t_ms": t_ms, **fields})
        seq += 1

    def telemetry(x, y, heading, speed, state):
        add(type="telemetry", pos={"x": x, "y": y}, heading=heading,
            speed_mm_s=round(speed, 1), odo_mm=int(odo),
            battery_pct=round(battery, 1), state=state)

    add(type="event", event="CALIBRATION_DONE", ok=True)
    add(type="event", event="RUN_START", battery_pct=round(battery, 1))

    # Anda de uma célula para a próxima, gerando um pacote a cada 100 ms.
    for (x, y), (nx, ny) in zip(path, path[1:]):
        heading = HEADINGS[(nx - x, ny - y)]
        walked = 0.0
        while walked < CELL_MM:
            speed = random.uniform(280, 320)             # mm/s, com um pouco de ruído
            step = min(speed * TICK_S, CELL_MM - walked)
            walked += step
            odo += step
            t_ms += 100
            battery -= 0.05
            # Passou da metade da célula: já conta como estando na próxima.
            cx, cy = (x, y) if walked < CELL_MM / 2 else (nx, ny)
            telemetry(cx, cy, heading, speed, "EXPLORING")

    gx, gy = path[-1]
    add(type="event", event="GOAL_REACHED", pos={"x": gx, "y": gy})
    telemetry(gx, gy, heading, 0.0, "GOAL_REACHED")
    add(type="event", event="RUN_END")
    return msgs


def play(msgs, send):
    """Envia as mensagens no ritmo real: um pacote de telemetria a cada 100 ms."""
    next_tick = time.monotonic()
    for msg in msgs:
        send(msg)
        if msg["type"] == "telemetry":
            # Espera até o próximo "tique" de 100 ms, descontando o tempo gasto no envio.
            next_tick += TICK_S
            time.sleep(max(0.0, next_tick - time.monotonic()))


def main():
    parser = argparse.ArgumentParser(description="Mock simples do robô micromouse")
    parser.add_argument("--maze", choices=PATHS.keys(), default="4x4")
    parser.add_argument("--url", default="ws://localhost:3001/ingest")
    parser.add_argument("--stdout", action="store_true", help="imprime o JSON em vez de enviar")
    args = parser.parse_args()

    msgs = build_run(PATHS[args.maze])

    if args.stdout:
        play(msgs, lambda msg: print(json.dumps(msg), flush=True))
    else:
        from websockets.sync.client import connect
        try:
            with connect(args.url) as ws:
                play(msgs, lambda msg: ws.send(json.dumps(msg)))
        except OSError as exc:
            sys.exit(f"Não foi possível conectar em {args.url}: {exc}")

    print(f"Corrida {args.maze} finalizada: {len(msgs)} mensagens.", file=sys.stderr)


if __name__ == "__main__":
    main()