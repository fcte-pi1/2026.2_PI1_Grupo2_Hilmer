const { execSync } = require("node:child_process");

function createTestDb() {
  const container = "microMouse-db-dev";
  const user = process.env.POSTGRES_USER || "localUser";
  const testDb = process.env.POSTGRES_DB || "microMouse_test";

  try {
    // Verifica se o banco de testes já existe dentro do postgres
    const checkCmd = `docker exec ${container} psql -U ${user} -d postgres -tAc "SELECT 1 FROM pg_database WHERE datname='${testDb}'"`;
    const exists = execSync(checkCmd, { encoding: "utf8" }).trim();

    if (exists !== "1") {
      const createCmd = `docker exec ${container} psql -U ${user} -d postgres -c "CREATE DATABASE \\"${testDb}\\";"`;
      execSync(createCmd, { stdio: "inherit" });
      console.log(`🟢 Banco de testes "${testDb}" criado com sucesso.`);
    } else {
      console.log(`🟢 Banco de testes "${testDb}" já existe e está pronto.`);
    }
  } catch (err) {
    console.error("Erro ao verificar/criar banco de teste:", err.message);
    process.exit(1);
  }
}

createTestDb();
