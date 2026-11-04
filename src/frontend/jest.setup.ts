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
