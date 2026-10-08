const { exec } = require("node:child_process");
let count = 0;
const spinner = ["⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"];

function checkPostgres() {
  exec("docker exec postgres-dev pg_isready --host localhost", handleReturn);

  function handleReturn(err, stdout) {
    const isReady = stdout && stdout.includes("accepting connections");

    if (!isReady) {
      const frameIndex = count % spinner.length;

      process.stdout.write(
        `\r🟡 Aguardando Postgres aceitar conexões ${spinner[frameIndex]} `,
      );

      count++;
      setTimeout(checkPostgres, 250);
      return;
    }

    process.stdout.write("\r🟢 Postgres está pronto e aceitando conexões!\n\n");
  }
}

checkPostgres();
