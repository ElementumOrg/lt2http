import { useCallback, useEffect, useRef, useState } from 'react';

// The page from the address: #/torrent/<hash> gives ['torrent', '<hash>']
export function useHashRoute(): string[] {
  const parse = () => window.location.hash.replace(/^#\/?/, '').split('/').filter(Boolean).map(decodeURIComponent);
  const [route, setRoute] = useState(parse);
  useEffect(() => {
    const update = () => setRoute(parse());
    window.addEventListener('hashchange', update);
    return () => window.removeEventListener('hashchange', update);
  }, []);
  return route;
}

export const navigate = (path: string) => {
  window.location.hash = path;
};

export interface Polled<T> {
  data: T | undefined;
  error: Error | undefined;
  // Loads again now, like after an action
  reload: () => void;
}

// Loads data and loads it again every interval while the component is shown
export function usePolling<T>(load: () => Promise<T>, interval = 2000, deps: unknown[] = []): Polled<T> {
  const [data, setData] = useState<T>();
  const [error, setError] = useState<Error>();
  const loadRef = useRef(load);
  loadRef.current = load;
  const timer = useRef<number>(undefined);
  const active = useRef(true);

  const reload = useCallback(async () => {
    window.clearTimeout(timer.current);
    try {
      const result = await loadRef.current();
      if (active.current) {
        setData(result);
        setError(undefined);
      }
    } catch (e) {
      if (active.current) setError(e as Error);
    }
    if (active.current && interval > 0) timer.current = window.setTimeout(reload, interval);
  }, [interval]);

  useEffect(() => {
    active.current = true;
    setData(undefined);
    reload();
    return () => {
      active.current = false;
      window.clearTimeout(timer.current);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, deps);

  return { data, error, reload };
}
