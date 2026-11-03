-- Mesmas consultas que o RunsService faz (findAll e findBestRuns).
-- Incluído por bench-historico.sql; espera a variável :maze.

SELECT id AS labirinto_id FROM labirintos WHERE dimensao = :'maze' \gset

\echo '--- findAll: corridas do labirinto, da mais recente para a mais antiga'
EXPLAIN (ANALYZE, BUFFERS)
SELECT c.*, l.dimensao
FROM corridas c
JOIN labirintos l ON l.id = c.labirinto_id
WHERE c.labirinto_id = :'labirinto_id'
ORDER BY c.iniciada_em DESC;

\echo '--- findAll: primeira e última amostra de cada corrida'
EXPLAIN (ANALYZE, BUFFERS)
SELECT
  c.id,
  (ini.sensores->>'battery_pct')::float8,
  (fim.sensores->>'battery_pct')::float8,
  (ini.sensores->>'odo_mm')::float8,
  (fim.sensores->>'odo_mm')::float8,
  (ini.sensores->>'t_ms')::float8,
  (fim.sensores->>'t_ms')::float8
FROM unnest(ARRAY(
  SELECT id FROM corridas WHERE labirinto_id = :'labirinto_id'
)) AS c(id)
JOIN LATERAL (
  SELECT t.sensores FROM telemetrias t
  WHERE t.corrida_id = c.id
  ORDER BY t."timestamp" ASC
  LIMIT 1
) ini ON true
JOIN LATERAL (
  SELECT t.sensores FROM telemetrias t
  WHERE t.corrida_id = c.id
  ORDER BY t."timestamp" DESC
  LIMIT 1
) fim ON true;

\echo '--- findBestRuns: melhor corrida de cada labirinto'
EXPLAIN (ANALYZE, BUFFERS)
SELECT *
FROM (
  SELECT DISTINCT ON (c.labirinto_id)
    l.dimensao, l.created_at, c.labirinto_id, c.id, c.status,
    c.iniciada_em, c.finalizada_em
  FROM corridas c
  JOIN labirintos l ON l.id = c.labirinto_id
  WHERE c.status IN ('CONCLUIDA', 'FINALIZADA')
    AND c.finalizada_em IS NOT NULL
  ORDER BY c.labirinto_id, c.finalizada_em - c.iniciada_em, c.iniciada_em
) melhores
ORDER BY created_at;
