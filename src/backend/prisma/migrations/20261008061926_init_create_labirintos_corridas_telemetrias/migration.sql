-- CreateTable
CREATE TABLE "labirintos" (
    "id" TEXT NOT NULL,
    "dimensao" TEXT NOT NULL,
    "created_at" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT "labirintos_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "corridas" (
    "id" TEXT NOT NULL,
    "labirinto_id" TEXT NOT NULL,
    "status" TEXT NOT NULL,
    "iniciada_em" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "finalizada_em" TIMESTAMP(3),

    CONSTRAINT "corridas_pkey" PRIMARY KEY ("id")
);

-- CreateTable
CREATE TABLE "telemetrias" (
    "id" TEXT NOT NULL,
    "corrida_id" TEXT NOT NULL,
    "timestamp" TIMESTAMP(3) NOT NULL DEFAULT CURRENT_TIMESTAMP,
    "pos_x" DOUBLE PRECISION NOT NULL,
    "pos_y" DOUBLE PRECISION NOT NULL,
    "direcao" TEXT NOT NULL,
    "sensores" JSONB NOT NULL,

    CONSTRAINT "telemetrias_pkey" PRIMARY KEY ("id")
);

-- CreateIndex
CREATE UNIQUE INDEX "labirintos_dimensao_key" ON "labirintos"("dimensao");

-- AddForeignKey
ALTER TABLE "corridas" ADD CONSTRAINT "corridas_labirinto_id_fkey" FOREIGN KEY ("labirinto_id") REFERENCES "labirintos"("id") ON DELETE RESTRICT ON UPDATE CASCADE;

-- AddForeignKey
ALTER TABLE "telemetrias" ADD CONSTRAINT "telemetrias_corrida_id_fkey" FOREIGN KEY ("corrida_id") REFERENCES "corridas"("id") ON DELETE CASCADE ON UPDATE CASCADE;
