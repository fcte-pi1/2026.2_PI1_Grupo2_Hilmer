import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import HistoricoErro from '@/app/historico/error';

describe('Histórico: tela de erro', () => {
  it('mostra a mensagem do erro', () => {
    render(
      <HistoricoErro error={new Error('API fora do ar')} retry={() => {}} />,
    );
    expect(
      screen.getByRole('heading', { name: 'Histórico de corridas' }),
    ).toBeInTheDocument();
    expect(
      screen.getByText('Não foi possível carregar o histórico: API fora do ar'),
    ).toBeInTheDocument();
  });

  it('chama retry ao clicar em Tentar novamente', async () => {
    const retry = jest.fn();
    const user = userEvent.setup();
    render(<HistoricoErro error={new Error('falha')} retry={retry} />);

    await user.click(screen.getByRole('button', { name: 'Tentar novamente' }));

    expect(retry).toHaveBeenCalledTimes(1);
  });
});
