import { EventEmitter2, EventEmitterModule } from '@nestjs/event-emitter';
import { Test } from '@nestjs/testing';
import { ConnectionMonitorService } from './connection-monitor.service';
import { TELEMETRY_EVENTS } from './telemetry-validation.service';

/** Garante que os @OnEvent estão mesmo ligados ao EventEmitter2 (e não só os métodos corretos). */
describe('ConnectionMonitorService ligado ao barramento de eventos', () => {
  it('reage a telemetry.received e run.finished (RUN_END) emitidos no EventEmitter2', async () => {
    const moduleRef = await Test.createTestingModule({
      imports: [EventEmitterModule.forRoot()],
      providers: [ConnectionMonitorService],
    }).compile();
    await moduleRef.init();

    const emitter = moduleRef.get(EventEmitter2);
    const monitor = moduleRef.get(ConnectionMonitorService);

    emitter.emit(TELEMETRY_EVENTS.TELEMETRY_RECEIVED, {
      data: { robot_id: 'mm-01', type: 'telemetry' },
      recebido_em: new Date(),
    });
    expect(monitor.getState('mm-01')).toBe('online');

    emitter.emit(TELEMETRY_EVENTS.RUN_FINISHED, {
      data: { robot_id: 'mm-01', type: 'event', event: 'RUN_END' },
      recebido_em: new Date(),
    });
    expect(monitor.getState('mm-01')).toBe('finished');

    await moduleRef.close();
  });
});
