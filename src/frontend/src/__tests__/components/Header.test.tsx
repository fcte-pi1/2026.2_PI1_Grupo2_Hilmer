import { render, screen } from '@testing-library/react';
import { Header } from '@/components/Header';

const mockPathname = jest.fn();
jest.mock('next/navigation', () => ({
  usePathname: () => mockPathname(),
}));

// Troca o Link do Next por um <a> simples, para o teste não depender do roteador.
jest.mock('next/link', () => {
  const React = jest.requireActual('react');
  return {
    __esModule: true,
    default: ({
      href,
      children,
      ...resto
    }: {
      href: string;
      children: unknown;
    }) => React.createElement('a', { href, ...resto }, children),
  };
});

describe('Header', () => {
  it('mostra título, conexão e bateria', () => {
    mockPathname.mockReturnValue('/');
    render(<Header />);
    expect(screen.getByText('PIH_02 - M.O.U.S.E')).toBeInTheDocument();
    expect(screen.getByText('Conectado')).toBeInTheDocument();
    expect(screen.getByText('67%')).toBeInTheDocument();
  });

  it('destaca o link do Dashboard em /', () => {
    mockPathname.mockReturnValue('/');
    render(<Header />);
    expect(screen.getByRole('link', { name: 'Dashboard' })).toHaveClass(
      'font-bold',
    );
    expect(screen.getByRole('link', { name: 'Histórico' })).toHaveClass(
      'opacity-50',
    );
  });

  it('destaca o link do Histórico em /historico', () => {
    mockPathname.mockReturnValue('/historico');
    render(<Header />);
    expect(screen.getByRole('link', { name: 'Histórico' })).toHaveClass(
      'font-bold',
    );
    expect(screen.getByRole('link', { name: 'Dashboard' })).toHaveClass(
      'opacity-50',
    );
  });

  it('aponta os links para as rotas corretas', () => {
    mockPathname.mockReturnValue('/');
    render(<Header />);
    expect(screen.getByRole('link', { name: 'Dashboard' })).toHaveAttribute(
      'href',
      '/',
    );
    expect(screen.getByRole('link', { name: 'Histórico' })).toHaveAttribute(
      'href',
      '/historico',
    );
  });
});
