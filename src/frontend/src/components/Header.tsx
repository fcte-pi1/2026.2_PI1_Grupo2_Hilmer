'use client';

import { usePathname } from 'next/navigation';
import Link from 'next/link';
import { Connection } from './Connection';
import { BatteryLevel } from './BatteryLevel';
import { mockData } from '@/lib/mock-data';
export function Header() {
  const pathname = usePathname();
  return (
    <header className="flex flex-col items-start gap-4 px-4 py-3 border-b bg-[#393A99] text-white font-mono">
      <div className="flex w-full flex-wrap justify-between gap-4">
        <div>
          <p className="text-3xl font-bold">PIH_02 - M.O.U.S.E</p>
          <p className="text-lg">
            Máquina de Orientação e Ultrapassagem de Sistemas Espaciais
          </p>
        </div>
        <div className="flex items-center gap-6">
          <Connection connected={mockData.connected} />
          <BatteryLevel level={mockData.batteryLevel} />
        </div>
      </div>
      <nav className="flex gap-4 self-center">
        <Link
          href="/"
          className={
            pathname === '/' ? 'font-bold bg-[#172860]/20' : 'opacity-50'
          }
        >
          Dashboard
        </Link>
        <Link
          href="/historico"
          className={
            pathname === '/historico'
              ? 'font-bold bg-[#172860]/20'
              : 'opacity-50'
          }
        >
          Histórico
        </Link>
      </nav>
    </header>
  );
}
