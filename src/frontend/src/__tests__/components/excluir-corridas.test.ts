import { excluirCorridas } from '@/lib/corridas';

describe('excluirCorridas', () => {
  beforeEach(() => {
    jest.clearAllMocks();
    global.fetch = jest.fn();
  });

  it('deve enviar DELETE /runs com os IDs no corpo JSON e retornar deletedCount', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => ({ deletedCount: 2 }),
    });

    const resultado = await excluirCorridas(['run-1', 'run-2']);

    expect(global.fetch).toHaveBeenCalledWith(
      expect.stringMatching(/\/runs$/),
      {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ids: ['run-1', 'run-2'] }),
      },
    );
    expect(resultado).toEqual({ deletedCount: 2 });
  });

  it('deve lançar erro formatado quando o backend retornar lista de erros de validação', async () => {
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: false,
      status: 400,
      json: async () => ({
        message: ['ids should not be empty'],
      }),
    });

    await expect(excluirCorridas([])).rejects.toThrow(
      'ids should not be empty',
    );
  });
});