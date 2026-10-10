import { SequenceTrackerService } from './sequence-tracker.service';

describe('SequenceTrackerService', () => {
  let service: SequenceTrackerService;

  beforeEach(() => {
    service = new SequenceTrackerService();
  });

  it('deve aceitar o primeiro pacote de um robô com qualquer seq inicial', () => {
    const result = service.processSequence('mm-01', 0);
    expect(result.isValid).toBe(true);
    expect(result.isGap).toBe(false);
    expect(result.lastSeq).toBe(0);
    expect(service.getLastSequence('mm-01')).toBe(0);
  });

  it('deve aceitar pacotes sequenciais contínuos (seq + 1)', () => {
    service.processSequence('mm-01', 0);
    const res1 = service.processSequence('mm-01', 1);
    const res2 = service.processSequence('mm-01', 2);

    expect(res1.isValid).toBe(true);
    expect(res1.isGap).toBe(false);
    expect(res2.isValid).toBe(true);
    expect(res2.isGap).toBe(false);
    expect(service.getLastSequence('mm-01')).toBe(2);
  });

  it('deve descartar pacotes duplicados com seq igual ao anterior', () => {
    service.processSequence('mm-01', 5);
    const result = service.processSequence('mm-01', 5);

    expect(result.isValid).toBe(false);
    expect(result.isGap).toBe(false);
    expect(result.expectedSeq).toBe(6);
    expect(result.lastSeq).toBe(5);
  });

  it('deve descartar pacotes antigos (fora de ordem) com seq menor que o último processado', () => {
    service.processSequence('mm-01', 10);
    const result = service.processSequence('mm-01', 8);

    expect(result.isValid).toBe(false);
    expect(result.isGap).toBe(false);
    expect(result.expectedSeq).toBe(11);
  });

  it('deve identificar lacuna/buraco na transmissão (seq > lastSeq + 1), aceitar o pacote e atualizar a sequência', () => {
    service.processSequence('mm-01', 10);
    // Salta de 10 para 13 (perda estimada de 2 pacotes: 11 e 12)
    const result = service.processSequence('mm-01', 13);

    expect(result.isValid).toBe(true);
    expect(result.isGap).toBe(true);
    expect(result.expectedSeq).toBe(11);
    expect(result.lastSeq).toBe(13);
    expect(service.getLastSequence('mm-01')).toBe(13);
  });

  it('deve manter o controle de múltiplos robôs de forma independente', () => {
    service.processSequence('mm-01', 10);
    service.processSequence('mm-02', 100);

    expect(service.getLastSequence('mm-01')).toBe(10);
    expect(service.getLastSequence('mm-02')).toBe(100);

    const resRobot1 = service.processSequence('mm-01', 11);
    const resRobot2 = service.processSequence('mm-02', 101);

    expect(resRobot1.isValid).toBe(true);
    expect(resRobot2.isValid).toBe(true);
  });

  it('deve resetar o histórico do robô quando solicitado', () => {
    service.processSequence('mm-01', 20);
    service.reset('mm-01');

    expect(service.getLastSequence('mm-01')).toBeUndefined();

    // Novo primeiro pacote aceito
    const result = service.processSequence('mm-01', 0);
    expect(result.isValid).toBe(true);
  });
});
