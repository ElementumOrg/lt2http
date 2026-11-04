// Formatting of numbers for display

export function bytes(value: number | null | undefined): string {
  if (value === null || value === undefined || value < 0) return '-';
  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  let i = 0;
  while (value >= 1024 && i < units.length - 1) {
    value /= 1024;
    i++;
  }
  return `${i === 0 ? value : value.toFixed(value >= 100 ? 0 : value >= 10 ? 1 : 2)} ${units[i]}`;
}

export const speed = (value: number) => `${bytes(value)}/s`;

export const percent = (value: number) => `${(value || 0).toFixed(1)}%`;

// -1 means unknown in the API
export const count = (value: number) => (value < 0 ? '-' : String(value));

export function duration(seconds: number): string {
  if (!seconds || seconds < 0) return '-';
  const d = Math.floor(seconds / 86400);
  const h = Math.floor(seconds / 3600) % 24;
  const m = Math.floor(seconds / 60) % 60;
  return d ? `${d}d ${h}h` : h ? `${h}h ${m}m` : `${m}m ${seconds % 60}s`;
}

export function settingValue(value: unknown): string {
  return typeof value === 'object' && value !== null ? JSON.stringify(value) : String(value);
}
