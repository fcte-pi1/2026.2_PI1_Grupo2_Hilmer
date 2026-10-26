import nextJest from 'next/jest.js';

const createJestConfig = nextJest({ dir: './' });

/** @type {import('jest').Config} */
const config = {
  testEnvironment: 'jsdom',
  setupFilesAfterEnv: ['<rootDir>/jest.setup.ts'],
  moduleNameMapper: { '^@/(.*)$': '<rootDir>/src/$1' },
  testMatch: ['<rootDir>/src/__tests__/**/*.test.{ts,tsx}'],
  clearMocks: true,
  collectCoverageFrom: [
    'src/components/**/*.{ts,tsx}',
    'src/context/**/*.{ts,tsx}',
    'src/lib/**/*.{ts,tsx}',
    '!src/lib/mock-*.ts',
    '!**/*.d.ts',
  ],
};

export default createJestConfig(config);
