const { PrismaClient } = require("@prisma/client");

describe("Disponibilidade dos Bancos de Dados", () => {
  let prismaDev;
  let prismaTest;

  beforeAll(() => {
    // Instancia o cliente para o banco de desenvolvimento
    prismaDev = new PrismaClient({
      datasources: {
        db: {
          url: "postgresql://localUser:localPassword@localhost:5432/microMouse?schema=public",
        },
      },
    });

    // Instancia o cliente para o banco de testes (via env ou fallback)
    prismaTest = new PrismaClient({
      datasources: {
        db: {
          url:
            process.env.DATABASE_URL ||
            "postgresql://localUser:localPassword@localhost:5432/microMouse_test?schema=public",
        },
      },
    });
  });

  afterAll(async () => {
    await prismaDev.$disconnect();
    await prismaTest.$disconnect();
  });

  it("deve conectar com sucesso e responder query no banco de desenvolvimento (microMouse)", async () => {
    const result = await prismaDev.$queryRaw`SELECT 1 as alive`;
    expect(result).toBeDefined();
    expect(result[0].alive).toBe(1);
  });

  it("deve conectar com sucesso e responder query no banco de testes (microMouse_test)", async () => {
    const result = await prismaTest.$queryRaw`SELECT 1 as alive`;
    expect(result).toBeDefined();
    expect(result[0].alive).toBe(1);
  });

  it("deve confirmar que os dois bancos são instâncias lógicas distintas", async () => {
    const [dbDevInfo] =
      await prismaDev.$queryRaw`SELECT current_database() as name`;
    const [dbTestInfo] =
      await prismaTest.$queryRaw`SELECT current_database() as name`;

    expect(dbDevInfo.name).toBe("microMouse");
    expect(dbTestInfo.name).toBe("microMouse_test");
    expect(dbDevInfo.name).not.toBe(dbTestInfo.name);
  });
});
