-- Multiplica as corridas FINALIZADAS gravadas pelo emulador para simular o
-- volume de várias semanas de uso (10 Hz, RNF-15). Usado para medir as
-- consultas do histórico com EXPLAIN ANALYZE (bench-historico.sql).
--
-- Uso (rode antes uma ou mais corridas com `npm run robot:emulator`):
--   docker exec -i microMouse-db-dev psql -U localUser -d <banco> \
--     -v copias=1000 < infra/scripts/bench-popular.sql
--
-- Não rode no banco de desenvolvimento se quiser manter os dados limpos:
-- use um banco à parte (ex.: CREATE DATABASE "microMouse_bench").

\set ON_ERROR_STOP on

BEGIN;

CREATE TEMP TABLE copias ON COMMIT DROP AS
SELECT
  gen_random_uuid()::text AS novo_id,
  b.id AS base_id,
  -- Distribui as cópias entre todos os labirintos cadastrados
  labs.ids[1 + g.n % cardinality(labs.ids)] AS labirinto_id,
  g.n,
  interval '1 minute' * g.n AS deslocamento,
  -- Varia a duração para que exista uma melhor corrida de fato
  random() * interval '10 seconds' AS ajuste
FROM corridas b
CROSS JOIN generate_series(1, :copias) AS g(n)
CROSS JOIN (SELECT array_agg(id ORDER BY created_at) AS ids FROM labirintos) labs
WHERE b.status = 'FINALIZADA';

INSERT INTO corridas (id, labirinto_id, status, iniciada_em, finalizada_em)
SELECT
  c.novo_id,
  c.labirinto_id,
  CASE WHEN c.n % 10 = 0 THEN 'ABORTADA' ELSE 'FINALIZADA' END,
  b.iniciada_em - c.deslocamento,
  b.finalizada_em - c.deslocamento + c.ajuste
FROM copias c
JOIN corridas b ON b.id = c.base_id;

INSERT INTO telemetrias (id, corrida_id, "timestamp", pos_x, pos_y, direcao, sensores)
SELECT
  gen_random_uuid()::text,
  c.novo_id,
  t."timestamp" - c.deslocamento,
  t.pos_x,
  t.pos_y,
  t.direcao,
  t.sensores
FROM copias c
JOIN telemetrias t ON t.corrida_id = c.base_id
-- Grava as amostras de cada corrida juntas, como na ingestão real
ORDER BY c.n, c.base_id, t."timestamp";

COMMIT;

ANALYZE corridas;
ANALYZE telemetrias;

SELECT
  (SELECT count(*) FROM corridas) AS corridas,
  (SELECT count(*) FROM telemetrias) AS telemetrias;
