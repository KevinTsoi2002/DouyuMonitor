export function escapeStt(value: string): string {
  return value.replace(/@/g, '@A').replace(/\//g, '@S');
}

export function unescapeStt(value: string): string {
  return value.replace(/@S/g, '/').replace(/@A/g, '@');
}

export function serializeStt(value: Record<string, string | number>): string {
  return Object.entries(value)
    .map(([key, entry]) => `${escapeStt(key)}@=${escapeStt(String(entry))}/`)
    .join('');
}

export function parseStt(raw: string): Record<string, string> {
  const result: Record<string, string> = {};
  for (const part of raw.split('/')) {
    if (!part) continue;
    const separator = part.indexOf('@=');
    if (separator < 1) continue;
    const key = unescapeStt(part.slice(0, separator));
    result[key] = unescapeStt(part.slice(separator + 2));
  }
  return result;
}
