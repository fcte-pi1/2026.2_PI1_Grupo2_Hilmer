import type { Metadata } from 'next';
import { Geist, Geist_Mono, Iosevka_Charon_Mono } from 'next/font/google';
import './globals.css';
import { Header } from '@/components/Header';
import { TelemetryProvider } from '@/context/TelemetryContext';

const geistSans = Geist({
  variable: '--font-geist-sans',
  subsets: ['latin'],
});

const geistMono = Geist_Mono({
  variable: '--font-geist-mono',
  subsets: ['latin'],
});

const iosevka = Iosevka_Charon_Mono({
  variable: '--font-iosevka',
  subsets: ['latin'],
  weight: ['400', '700'],
});

export const metadata: Metadata = {
  title: 'Micromouse - Telemetria',
  description:
    'apresenta os dados e informações vindas em tempo real do micromouse',
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html
      lang="pt-br"
      className={`${geistSans.variable} ${geistMono.variable} ${iosevka.variable} h-full antialiased`}
    >
      <body className="min-h-full flex flex-col bg-[#13143C] text-white">
        <TelemetryProvider>
          <Header />
          <main className="px-3 py-6 flex-1 mx-auto w-full">{children}</main>
        </TelemetryProvider>
      </body>
    </html>
  );
}
