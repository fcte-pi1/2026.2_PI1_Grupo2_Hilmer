import { formatarData, formatarTempo, formatarVelocidade } from '@/lib/format';

describe('formatarTempo', () => {
  it.each([
    [0, '00:00'],
    [9, '00:09'],
    [60, '01:00'],
    [75.9, '01:15'],
    [3599, '59:59'],
  ])('formata %s s como %s', (segundos, esperado) => {
    expect(formatarTempo(segundos)).toBe(esperado);
  });
});

describe('formatarVelocidade', () => {
  it.each([
    [0, '0.00 m/s'],
    [0.3, '0.30 m/s'],
    [2.5, '2.50 m/s'],
  ])('formata %s como %s', (valor, esperado) => {
    expect(formatarVelocidade(valor)).toBe(esperado);
  });
});

describe('formatarData', () => {
  it('converte o ISO para o horário de São Paulo', () => {
    // 17:05 UTC = 14:05 em São Paulo (UTC-3)
    const texto = formatarData('2026-10-01T17:05:00Z');
    expect(texto).toMatch(/01\/10/);
    expect(texto).toMatch(/14:05/);
  });
});
