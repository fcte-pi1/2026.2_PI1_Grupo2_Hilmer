// Healthcheck do container (Dockerfile e compose.yaml): chama o GET /health
// e sai com 1 se a resposta não for 2xx (ex.: 503 com o banco fora do ar).
const port = process.env.PORT || 3002;

fetch(`http://127.0.0.1:${port}/health`, { signal: AbortSignal.timeout(4000) })
  .then((res) => process.exit(res.ok ? 0 : 1))
  .catch(() => process.exit(1));
