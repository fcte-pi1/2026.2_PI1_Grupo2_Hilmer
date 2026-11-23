import { render, screen, waitFor } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { HistoricoDashboard } from '@/components/historico/HistoricoDashboard';
import type { CorridaResumo } from '@/lib/corridas';

const mockRefresh = jest.fn();

jest.mock('next/navigation', () => ({
  useRouter: () => ({
    refresh: mockRefresh,
  }),
}));

// Evita problemas de ResizeObserver do Recharts no jsdom
jest.mock('@/components/historico/GraficoCorridas', () => ({
  GraficoCorridas: ({ titulo }: { titulo: string }) => <div>{titulo}</div>,
}));

const corridasMock: CorridaResumo[] = [
  {
    id: 'run-1',
    labirinto: '4x4',
    status: 'FINALIZADA',
    iniciadaEm: '2026-11-10T10:00:00Z',
    finalizadaEm: '2026-11-10T10:00:15Z',
    duracaoSegundos: 15,
    velocidadeMedia: 0.3,
    bateriaInicial: 100,
    bateriaFinal: 98,
  },
  {
    id: 'run-2',
    labirinto: '8x4',
    status: 'EM_ANDAMENTO',
    iniciadaEm: '2026-11-10T11:00:00Z',
    finalizadaEm: null,
    duracaoSegundos: null,
    velocidadeMedia: 0.25,
    bateriaInicial: 95,
    bateriaFinal: null,
  },
  {
    id: 'run-3',
    labirinto: '12x4',
    status: 'ABORTADA',
    iniciadaEm: '2026-11-10T12:00:00Z',
    finalizadaEm: '2026-11-10T12:01:00Z',
    duracaoSegundos: 60,
    velocidadeMedia: 0.2,
    bateriaInicial: 90,
    bateriaFinal: 85,
  },
];

describe('HistoricoDashboard - Exclusão de corridas', () => {
  beforeEach(() => {
    jest.clearAllMocks();
    global.fetch = jest.fn();
  });

  it('deve manter o botão de exclusão desabilitado quando nenhuma corrida estiver selecionada', () => {
    render(<HistoricoDashboard corridas={corridasMock} />);

    const botaoExcluir = screen.getByRole('button', {
      name: /excluir selecionadas/i,
    });
    expect(botaoExcluir).toBeDisabled();
  });

  it('deve impedir a seleção de corridas em andamento', async () => {
    const user = userEvent.setup();
    render(<HistoricoDashboard corridas={corridasMock} />);

    const checkboxEmAndamento = screen.getByLabelText(
      'Selecionar corrida #2',
    );

    expect(checkboxEmAndamento).toBeDisabled();
    expect(checkboxEmAndamento).not.toBeChecked();

    await user.click(checkboxEmAndamento);

    expect(checkboxEmAndamento).not.toBeChecked();
    expect(
      screen.getByRole('button', { name: /excluir selecionadas/i }),
    ).toBeDisabled();
  });

  it('deve habilitar o botão com a contagem correta e abrir o modal com a quantidade de corridas selecionadas', async () => {
    const user = userEvent.setup();
    render(<HistoricoDashboard corridas={corridasMock} />);

    const checkbox1 = screen.getByLabelText('Selecionar corrida #1');
    const checkbox2 = screen.getByLabelText('Selecionar corrida #2');
    const checkbox3 = screen.getByLabelText('Selecionar corrida #3');

    expect(checkbox2).toBeDisabled();

    await user.click(checkbox1);
    await user.click(checkbox3);

    expect(checkbox1).toBeChecked();
    expect(checkbox2).not.toBeChecked();
    expect(checkbox3).toBeChecked();

    const botaoExcluir = screen.getByRole('button', {
      name: 'Excluir selecionadas (2)',
    });
    expect(botaoExcluir).toBeEnabled();

    await user.click(botaoExcluir);

    expect(screen.getByRole('dialog')).toBeInTheDocument();
    expect(screen.getByText('2 corridas selecionadas')).toBeInTheDocument();
  });

  it('deve fechar o modal, limpar checkboxes e NÃO fazer requisição ao clicar em Cancelar', async () => {
    const user = userEvent.setup();
    render(<HistoricoDashboard corridas={corridasMock} />);

    const checkbox1 = screen.getByLabelText('Selecionar corrida #1');
    await user.click(checkbox1);
    expect(checkbox1).toBeChecked();

    await user.click(
      screen.getByRole('button', { name: 'Excluir selecionadas (1)' }),
    );
    expect(screen.getByRole('dialog')).toBeInTheDocument();

    await user.click(screen.getByRole('button', { name: 'Cancelar' }));

    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
    expect(checkbox1).not.toBeChecked();
    expect(global.fetch).not.toHaveBeenCalled();
  });

  it('deve selecionar todas as corridas elegíveis, ignorando as que estão em andamento', async () => {
    const user = userEvent.setup();
    render(<HistoricoDashboard corridas={corridasMock} />);

    const checkboxTodas = screen.getByLabelText('Selecionar todas as corridas');
    const checkbox1 = screen.getByLabelText('Selecionar corrida #1');
    const checkbox2 = screen.getByLabelText('Selecionar corrida #2');
    const checkbox3 = screen.getByLabelText('Selecionar corrida #3');

    await user.click(checkboxTodas);

    expect(checkboxTodas).toBeChecked();
    expect(checkbox1).toBeChecked();
    expect(checkbox2).toBeDisabled();
    expect(checkbox2).not.toBeChecked();
    expect(checkbox3).toBeChecked();
    expect(
      screen.getByRole('button', { name: 'Excluir selecionadas (2)' }),
    ).toBeEnabled();
  });

  it('deve fechar o modal, limpar seleção e NÃO fazer requisição ao clicar no botão X', async () => {
    const user = userEvent.setup();
    render(<HistoricoDashboard corridas={corridasMock} />);

    const checkboxTodas = screen.getByLabelText('Selecionar todas as corridas');
    await user.click(checkboxTodas);

    await user.click(
      screen.getByRole('button', { name: 'Excluir selecionadas (2)' }),
    );
    await user.click(screen.getByRole('button', { name: 'Fechar modal' }));

    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
    expect(checkboxTodas).not.toBeChecked();
    expect(screen.getByLabelText('Selecionar corrida #1')).not.toBeChecked();
    expect(screen.getByLabelText('Selecionar corrida #2')).not.toBeChecked();
    expect(screen.getByLabelText('Selecionar corrida #3')).not.toBeChecked();
    expect(global.fetch).not.toHaveBeenCalled();
  });

  it('deve enviar DELETE /runs apenas com as corridas selecionadas e elegíveis, limpar seleção, fechar modal e atualizar dados ao confirmar', async () => {
    const user = userEvent.setup();
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: true,
      json: async () => ({ deletedCount: 2 }),
    });

    render(<HistoricoDashboard corridas={corridasMock} />);

    await user.click(screen.getByLabelText('Selecionar corrida #1'));
    await user.click(screen.getByLabelText('Selecionar corrida #3'));

    await user.click(
      screen.getByRole('button', { name: 'Excluir selecionadas (2)' }),
    );
    await user.click(
      screen.getByRole('button', { name: 'Confirmar exclusão' }),
    );

    await waitFor(() => {
      expect(global.fetch).toHaveBeenCalledWith(
        expect.stringMatching(/\/runs$/),
        expect.objectContaining({
          method: 'DELETE',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ ids: ['run-1', 'run-3'] }),
        }),
      );
    });

    await waitFor(() => {
      expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
    });

    expect(screen.getByLabelText('Selecionar corrida #1')).not.toBeChecked();
    expect(screen.getByLabelText('Selecionar corrida #2')).not.toBeChecked();
    expect(screen.getByLabelText('Selecionar corrida #3')).not.toBeChecked();
    expect(mockRefresh).toHaveBeenCalledTimes(1);
  });

  it('deve exibir mensagem de erro no modal caso a exclusão no backend falhe', async () => {
    const user = userEvent.setup();
    (global.fetch as jest.Mock).mockResolvedValueOnce({
      ok: false,
      status: 500,
      json: async () => ({ message: 'Falha ao excluir corridas no banco.' }),
    });

    render(<HistoricoDashboard corridas={corridasMock} />);

    await user.click(screen.getByLabelText('Selecionar corrida #1'));
    await user.click(
      screen.getByRole('button', { name: 'Excluir selecionadas (1)' }),
    );
    await user.click(
      screen.getByRole('button', { name: 'Confirmar exclusão' }),
    );

    const alerta = await screen.findByRole('alert');
    expect(alerta).toHaveTextContent('Falha ao excluir corridas no banco.');
    expect(screen.getByRole('dialog')).toBeInTheDocument();
    expect(mockRefresh).not.toHaveBeenCalled();
  });
});