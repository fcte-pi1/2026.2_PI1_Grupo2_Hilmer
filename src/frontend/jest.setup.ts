import '@testing-library/jest-dom';
import {
  instalarMatchMedia,
  instalarResizeObserver,
  restaurarViewport,
} from '@/__tests__/helpers/viewport';

// O jsdom não implementa matchMedia nem ResizeObserver (o Recharts precisa do
// segundo no ResponsiveContainer). Instalados para todos os testes.
if (typeof window !== 'undefined') {
  instalarMatchMedia();
  instalarResizeObserver();
  afterEach(() => restaurarViewport());
}
jest.mock('next/navigation', () => ({
  useRouter: () => ({
    push: jest.fn(),
    replace: jest.fn(),
    refresh: jest.fn(),
    back: jest.fn(),
    forward: jest.fn(),
    prefetch: jest.fn(),
  }),
  usePathname: () => '/historico',
  useSearchParams: () => new URLSearchParams(),
}));