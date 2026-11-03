-- CreateIndex
CREATE INDEX "corridas_labirinto_id_iniciada_em_idx" ON "corridas"("labirinto_id", "iniciada_em" DESC);

-- CreateIndex
CREATE INDEX "corridas_status_idx" ON "corridas"("status");

-- CreateIndex
CREATE INDEX "telemetrias_corrida_id_timestamp_idx" ON "telemetrias"("corrida_id", "timestamp");
