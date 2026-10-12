const { PrismaClient } = require("@prisma/client");

const prismaTest = new PrismaClient({
  datasources: {
    db: {
      url: process.env.DATABASE_URL,
    },
  },
});

beforeEach(async () => {
  // Limpa as tabelas antes de cada teste mantendo o isolamento
  await prismaTest.$executeRawUnsafe(`
    TRUNCATE TABLE "telemetrias", "corridas", "labirintos" CASCADE;
  `);
});

afterAll(async () => {
  await prismaTest.$disconnect();
});

module.exports = { prismaTest };
