import { Module } from '@nestjs/common';
import { EventEmitterModule } from '@nestjs/event-emitter';
import { ConnectionMonitorService } from './services/connection-monitor.service';
import { SequenceTrackerService } from './services/sequence-tracker.service';
import { TelemetryIngestService } from './services/telemetry-ingest.service';
import { TelemetryValidationService } from './services/telemetry-validation.service';
import { TelemetryEventsLogger } from './telemetry-events.logger';

// Listener temporário de log dos eventos (ver telemetry-events.logger.ts).
const debugProviders = process.env.TELEMETRY_DEBUG_EVENTS === 'true' ? [TelemetryEventsLogger] : [];

@Module({
  imports: [EventEmitterModule.forRoot()],
  providers: [
    SequenceTrackerService,
    TelemetryValidationService,
    ConnectionMonitorService, // watchdog (RNF18) e taxa em Hz (RNF15)
    TelemetryIngestService, // servidor ws em /ingest, porta 3001
    ...debugProviders,
  ],
  exports: [SequenceTrackerService, TelemetryValidationService, ConnectionMonitorService],
})
export class TelemetryModule {}
