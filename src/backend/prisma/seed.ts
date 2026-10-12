import { PrismaClient } from "@prisma/client";

const prisma = new PrismaClient();

async function main() {
  const dimensoes = ["4x4", "8x4", "12x4"];

  for (const dimensao of dimensoes) {
    await prisma.labirinto.upsert({
      where: { dimensao },
      update: {},
      create: { dimensao },
    });
  }
  console.log("Seed executado: Labirintos 4x4, 8x4 e 12x4 prontos.");
}

main()
  .catch((e) => {
    console.error(e);
    process.exit(1);
  })
  .finally(async () => {
    await prisma.$disconnect();
  });
