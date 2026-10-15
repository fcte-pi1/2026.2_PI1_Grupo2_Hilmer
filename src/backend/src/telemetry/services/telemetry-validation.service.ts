import { Injectable, Logger } from '@nestjs/common';
import { EventEmitter2 } from '@nestjs/event-emitter';
import { plainToInstance } from 'class-transformer';
import { validate, ValidationError } from 'class-validator';
import {
  BaseMessageDto,
  EventPayloadDto,
  TelemetryPayloadDto,
} from '../dto/telemetry.dto';
import { SequenceTrackerService } from './sequence-tracker.service';

export const TELEMETRY_EVENTS = {
  TELEMETRY_RECEIVED: 'telemetry.received',
  RUN_STARTED: 'run.started',
  RUN_GOAL_REACHED: 'run.goal_reached',
  RUN_FINISHED: 'run.finished',
  RUN_CALIBRATED: 'run.calibrated',
} as const;

export interface IngestedMessageEnvelope<T = BaseMessageDto> {
  data: T;
  recebido_em: Date;
}

export interface IngestStats {
  totalReceived: number;
  validProcessed: number;
  malformedJson: number;
  validationFailed: number;
  duplicateSeq: number;
}

@Injectable()
export class TelemetryValidationService {
  private readonly logger = new Logger(TelemetryValidationService.name);

  private stats: IngestStats = {
    totalReceived: 0,
    validProcessed: 0,
    malformedJson: 0,
    validationFailed: 0,
    duplicateSeq: 0,
  };

  constructor(
    private readonly sequenceTracker: SequenceTrackerService,
    private readonly eventEmitter: EventEmitter2,
  ) {}

  /**
   * Processa com resiliência extrema uma mensagem pura (string ou buffer) vinda do WebSocket.
   * Não lança exceções para não derrubar o socket (atende RNF13).
   */
  async processRawMessage(
    rawMessage: string | Buffer,
  ): Promise<BaseMessageDto | null> {
    this.stats.totalReceived++;
    const recebidoEm = new Date();

    // 1. Parse Seguro de JSON
    let parsed: any;
    try {
      const messageString =
        typeof rawMessage === 'string'
          ? rawMessage
          : rawMessage.toString('utf-8');
      parsed = JSON.parse(messageString);
    } catch (err: any) {
      this.stats.malformedJson++;
      this.logger.warn(
        `[TelemetryValidation] JSON inválido recebido no WebSocket. Descartando mensagem sem derrubar conexão. Detalhe: ${err.message}`,
      );
      return null;
    }

    if (!parsed || typeof parsed !== 'object') {
      this.stats.malformedJson++;
      this.logger.warn(
        `[TelemetryValidation] Mensagem recebida não é um objeto JSON válido. Descartada.`,
      );
      return null;
    }

    // 2. Mapeamento e Validação com DTO (class-validator)
    let dtoInstance: BaseMessageDto;

    if (parsed.type === 'telemetry') {
      dtoInstance = plainToInstance(TelemetryPayloadDto, parsed);
    } else if (parsed.type === 'event') {
      dtoInstance = plainToInstance(EventPayloadDto, parsed);
    } else {
      this.stats.validationFailed++;
      this.logger.warn(
        `[TelemetryValidation] Tipo de mensagem desconhecido: "${parsed.type}". Deve ser "telemetry" ou "event". Descartada.`,
      );
      return null;
    }

    const errors = await validate(dtoInstance);
    if (errors.length > 0) {
      this.stats.validationFailed++;
      const formattedErrors = this.formatValidationErrors(errors);
      this.logger.warn(
        `[TelemetryValidation] Contrato violado para o robô "${parsed.robot_id || 'desconhecido'}": ${formattedErrors}`,
      );
      return null;
    }

    // 3. Controle de Sequência (seq)
    const seqResult = this.sequenceTracker.processSequence(
      dtoInstance.robot_id,
      dtoInstance.seq,
    );

    if (!seqResult.isValid) {
      this.stats.duplicateSeq++;
      // Mensagem repetida ou fora de ordem descartada
      return null;
    }

    // 4. Emissão de Eventos Internos
    this.emitInternalEvents(dtoInstance, recebidoEm);

    this.stats.validProcessed++;
    return dtoInstance;
  }

  /**
   * Dispara os eventos no barramento do NestJS baseado no tipo e conteúdo do pacote.
   */
  private emitInternalEvents(dto: BaseMessageDto, recebidoEm: Date): void {
    const envelope: IngestedMessageEnvelope = {
      data: dto,
      recebido_em: recebidoEm,
    };

    if (dto.type === 'telemetry') {
      this.eventEmitter.emit(TELEMETRY_EVENTS.TELEMETRY_RECEIVED, envelope);
    } else if (dto.type === 'event') {
      const eventDto = dto as EventPayloadDto;
      switch (eventDto.event) {
        case 'RUN_START':
          this.eventEmitter.emit(TELEMETRY_EVENTS.RUN_STARTED, envelope);
          break;
        case 'CALIBRATION_DONE':
          this.eventEmitter.emit(TELEMETRY_EVENTS.RUN_CALIBRATED, envelope);
          break;
        case 'GOAL_REACHED':
          this.eventEmitter.emit(TELEMETRY_EVENTS.RUN_GOAL_REACHED, envelope);
          this.eventEmitter.emit(TELEMETRY_EVENTS.RUN_FINISHED, envelope);
          break;
        case 'RUN_END':
          this.eventEmitter.emit(TELEMETRY_EVENTS.RUN_FINISHED, envelope);
          break;
        default:
          this.logger.debug(
            `[TelemetryValidation] Evento desconhecido ou não mapeado: ${(eventDto as any).event}`,
          );
      }
    }
  }

  private formatValidationErrors(errors: ValidationError[]): string {
    return errors
      .map((err) => {
        const constraints = err.constraints
          ? Object.values(err.constraints).join(', ')
          : '';
        const children =
          err.children && err.children.length > 0
            ? ` (${this.formatValidationErrors(err.children)})`
            : '';
        return `campo "${err.property}": ${constraints}${children}`;
      })
      .join(' | ');
  }

  getStats(): IngestStats {
    return { ...this.stats };
  }

  resetStats(): void {
    this.stats = {
      totalReceived: 0,
      validProcessed: 0,
      malformedJson: 0,
      validationFailed: 0,
      duplicateSeq: 0,
    };
  }
}
