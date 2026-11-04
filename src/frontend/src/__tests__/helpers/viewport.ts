import { act } from '@testing-library/react';

/**
 * Larguras de referência da issue #309 (px).
 * Os breakpoints do Tailwind v4 são sm 640, md 768, lg 1024, xl 1280.
 */
export const LARGURAS = { celular: 360, tablet: 768, desktop: 1280 } as const;
export type Dispositivo = keyof typeof LARGURAS;
export const DISPOSITIVOS = Object.keys(LARGURAS) as Dispositivo[];

/** Para usar com it.each: [['celular', 360], ['tablet', 768], ['desktop', 1280]]. */
export const CASOS_DE_LARGURA = Object.entries(LARGURAS) as [
  Dispositivo,
  number,
][];

const LARGURA_PADRAO_JSDOM = 1024;

/** Avalia uma media query simples de largura, ex.: "(min-width: 768px)". */
function avaliarMediaQuery(consulta: string, largura: number): boolean {
  const partes = consulta.split(/\band\b/i).map((p) => p.trim());
  return partes.every((parte) => {
    const m = parte.match(/^\(\s*(min|max)-width:\s*(\d+(?:\.\d+)?)px\s*\)$/);
    if (!m) return false; // outras features (prefers-color-scheme etc.) = falso
    const limite = Number(m[2]);
    return m[1] === 'min' ? largura >= limite : largura <= limite;
  });
}

/** window.matchMedia que responde de acordo com window.innerWidth. */
export function instalarMatchMedia() {
  window.matchMedia = ((consulta: string) => ({
    get matches() {
      return avaliarMediaQuery(consulta, window.innerWidth);
    },
    media: consulta,
    onchange: null,
    addEventListener: () => {},
    removeEventListener: () => {},
    addListener: () => {},
    removeListener: () => {},
    dispatchEvent: () => false,
  })) as unknown as typeof window.matchMedia;
}

/**
 * ResizeObserver inerte (o jsdom não tem layout). Guarda as instâncias para os
 * testes conferirem que o ResponsiveContainer do Recharts observou o contêiner.
 */
export class ResizeObserverFalso {
  static instancias: ResizeObserverFalso[] = [];
  alvos: Element[] = [];
  constructor(public callback: ResizeObserverCallback) {
    ResizeObserverFalso.instancias.push(this);
  }
  observe(alvo: Element) {
    this.alvos.push(alvo);
  }
  unobserve(alvo: Element) {
    this.alvos = this.alvos.filter((a) => a !== alvo);
  }
  disconnect() {
    this.alvos = [];
  }
}

export function instalarResizeObserver() {
  globalThis.ResizeObserver =
    ResizeObserverFalso as unknown as typeof ResizeObserver;
}

/** Simula a largura da janela (e avisa quem escuta "resize"). */
export function setViewport(largura: number) {
  act(() => {
    Object.defineProperty(window, 'innerWidth', {
      configurable: true,
      writable: true,
      value: largura,
    });
    window.dispatchEvent(new Event('resize'));
  });
}

export function restaurarViewport() {
  setViewport(LARGURA_PADRAO_JSDOM);
}

/* ------------------------------------------------------------------ */
/* Resolução das classes responsivas do Tailwind                       */
/* ------------------------------------------------------------------ */

const BREAKPOINTS: Record<string, number> = {
  sm: 640,
  md: 768,
  lg: 1024,
  xl: 1280,
  '2xl': 1536,
};

/**
 * O jsdom não carrega o CSS do Tailwind, então não dá para medir o layout.
 * Esta função faz o que o navegador faria com as classes: entre as `opcoes`
 * (utilitários do mesmo grupo, ex.: flex-col / flex-row) que estão no
 * elemento, vale a de maior breakpoint que já foi atingido na `largura`.
 * Retorna undefined se nenhuma das opções se aplica.
 */
export function classeEfetiva(
  elemento: Element,
  opcoes: string[],
  largura: number,
): string | undefined {
  let vencedora: { base: string; peso: number } | undefined;
  for (const token of Array.from(elemento.classList)) {
    const partes = token.split(':');
    const base = partes[partes.length - 1];
    if (!opcoes.includes(base)) continue;
    const prefixos = partes.slice(0, -1);
    // Só entendemos prefixos de breakpoint; hover:, dark: etc. são ignorados.
    if (prefixos.some((p) => !(p in BREAKPOINTS))) continue;
    const peso = prefixos.length ? BREAKPOINTS[prefixos[0]] : 0;
    if (peso > largura) continue;
    if (!vencedora || peso >= vencedora.peso) vencedora = { base, peso };
  }
  return vencedora?.base;
}
