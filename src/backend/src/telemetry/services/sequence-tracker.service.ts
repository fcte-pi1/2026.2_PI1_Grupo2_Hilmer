import { Injectable, Logger } from '@nestjs/common';

export interface SequenceCheckResult {
  isValid: boolean;
  isGap: boolean;
  expectedSeq?: number;
  lastSeq?: number;
}

@Injectable()
export class SequenceTrackerService {
  private readonly logger = new Logger(SequenceTrackerService.name);
  private readonly lastSeenSeq = new Map<string, number>();

  /**
   * Processa o número de sequência de um robô.
   * - Descarta pacotes duplicados ou com seq <= último processado.
   * - Alerta e registra em log lacunas numéricas (seq > último + 1).
   * - Aceita o pacote normal (seq === último + 1 ou primeiro pacote).
   */
  processSequence(robotId: string, seq: number): SequenceCheckResult {
    if (!this.lastSeenSeq.has(robotId)) {
      this.lastSeenSeq.set(robotId, seq);
      this.logger.debug(
        `[SequenceTracker] Primeiro pacote registrado para o robô "${robotId}". seq inicial=${seq}`,
      );
      return {
        isValid: true,
        isGap: false,
        lastSeq: seq,
      };
    }

    const lastSeq = this.lastSeenSeq.get(robotId)!;

    // Pacote duplicado ou retroativo (fora de ordem)
    if (seq <= lastSeq) {
      this.logger.warn(
        `[SequenceTracker] Pacote duplicado ou fora de ordem descartado para o robô "${robotId}". Recebido seq=${seq}, último esperado > ${lastSeq}`,
      );
      return {
        isValid: false,
        isGap: false,
        expectedSeq: lastSeq + 1,
        lastSeq,
      };
    }

    // Salto numérico (lacuna na transmissão)
    if (seq > lastSeq + 1) {
      const lostPackets = seq - (lastSeq + 1);
      this.logger.warn(
        `[SequenceTracker] Lacuna na transmissão detectada para o robô "${robotId}". Esperado seq=${lastSeq + 1}, recebido seq=${seq} (estimativa de ${lostPackets} pacote(s) perdido(s))`,
      );
      this.lastSeenSeq.set(robotId, seq);
      return {
        isValid: true,
        isGap: true,
        expectedSeq: lastSeq + 1,
        lastSeq: seq,
      };
    }

    // Pacote em ordem sequencial exata
    this.lastSeenSeq.set(robotId, seq);
    return {
      isValid: true,
      isGap: false,
      lastSeq: seq,
    };
  }

  /**
   * Retorna a última sequência registrada para um robô.
   */
  getLastSequence(robotId: string): number | undefined {
    return this.lastSeenSeq.get(robotId);
  }

  /**
   * Limpa o estado da sequência (útil ao iniciar nova corrida ou em testes).
   */
  reset(robotId?: string): void {
    if (robotId) {
      this.lastSeenSeq.delete(robotId);
    } else {
      this.lastSeenSeq.clear();
    }
  }
}
