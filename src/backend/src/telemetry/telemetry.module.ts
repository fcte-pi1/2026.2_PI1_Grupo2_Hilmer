import { Module } from '@nestjs/common';
import { EventEmitterModule } from '@nestjs/event-emitter';
import { SequenceTrackerService } from './services/sequence-tracker.service';
import { TelemetryValidationService } from './services/telemetry-validation.service';

@Module({
  imports: [EventEmitterModule.forRoot()],
  providers: [SequenceTrackerService, TelemetryValidationService],
  exports: [SequenceTrackerService, TelemetryValidationService],
})
export class TelemetryModule {}
