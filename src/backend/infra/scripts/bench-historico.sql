-- EXPLAIN ANALYZE das consultas do histórico (RF-14) e do melhor resultado
-- (RF-15), com e sem os índices da migração add_indices_historico.
--
-- Uso (depois de popular o banco com bench-popular.sql). O \ir inclui o
-- arquivo vizinho, então os dois precisam estar dentro do container:
--   docker cp infra/scripts/. microMouse-db-dev:/tmp/bench/
--   docker exec microMouse-db-dev psql -U localUser -d <banco> \
--     -v maze=4x4 -f /tmp/bench/bench-historico.sql
--
-- O bloco "sem índices" remove os índices dentro de uma transação e desfaz
-- tudo no ROLLBACK, então o banco não é alterado.

\set ON_ERROR_STOP on
\pset pager off

\echo '==================== COM ÍNDICES ===================='
\ir bench-historico-consultas.sql

\echo '==================== SEM ÍNDICES ===================='
BEGIN;
DROP INDEX "corridas_labirinto_id_iniciada_em_idx";
DROP INDEX "corridas_status_idx";
DROP INDEX "telemetrias_corrida_id_timestamp_idx";
\ir bench-historico-consultas.sql
ROLLBACK;
