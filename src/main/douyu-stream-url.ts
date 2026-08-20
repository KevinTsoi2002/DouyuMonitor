const ALLOWED_HOST_SUFFIXES = ['.douyucdn.cn', '.douyucdn2.cn', '.edgesrv.com'];

export type DouyuStreamUrlErrorCode = 'INVALID_RESPONSE' | 'UNSAFE_STREAM_URL';

export class DouyuStreamUrlError extends Error {
  constructor(public readonly code: DouyuStreamUrlErrorCode, message = code) {
    super(message);
    this.name = 'DouyuStreamUrlError';
  }
}

export function parseAllowedDouyuFlvUrl(value: unknown): URL {
  if (typeof value !== 'string' || value.length === 0) {
    throw new DouyuStreamUrlError('INVALID_RESPONSE');
  }

  let url: URL;
  try {
    url = new URL(value);
  } catch {
    throw new DouyuStreamUrlError('UNSAFE_STREAM_URL');
  }

  const hostname = url.hostname.toLowerCase();
  if (
    !['http:', 'https:'].includes(url.protocol)
    || !ALLOWED_HOST_SUFFIXES.some((suffix) => hostname.endsWith(suffix))
  ) {
    throw new DouyuStreamUrlError('UNSAFE_STREAM_URL');
  }
  return url;
}

export function isAllowedDouyuStreamUrl(value: unknown): boolean {
  try {
    parseAllowedDouyuFlvUrl(value);
    return true;
  } catch {
    return false;
  }
}
