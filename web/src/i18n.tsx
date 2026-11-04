import { createContext, ReactNode, useCallback, useContext, useEffect, useState } from 'react';
import { api, Language } from './api';

// Translations are public/i18n/<code>.json, shared with lt2http itself (/info, state_localized)

type Texts = Record<string, string>;

interface I18n {
  language: string;
  languages: Language[];
  setLanguage: (code: string) => void;
  // The text of a key, {0}, {1}... replaced by the arguments
  t: (key: string, ...args: (string | number)[]) => string;
}

const I18nContext = createContext<I18n | null>(null);

const STORAGE_KEY = 'language';
const RTL = ['he', 'ar', 'fa'];

const loadTexts = async (code: string): Promise<Texts> => (await fetch(`/web/i18n/${code}.json`)).json();

function saved(): string | null {
  try {
    return localStorage.getItem(STORAGE_KEY);
  } catch {
    return null;
  }
}

// The saved language, else the browser's, else the language setting of lt2http
function pickLanguage(languages: Language[], configured: string): string {
  const codes = languages.map((l) => l.code);
  const match = (wanted: string) => {
    const code = wanted.toLowerCase().replace('-', '_');
    const lang = code.split('_')[0];
    return (
      codes.find((c) => c === code) ??
      codes.find((c) => c === `${lang}_${lang}`) ??
      codes.find((c) => c.startsWith(`${lang}_`))
    );
  };
  const stored = saved();
  if (stored && codes.includes(stored)) return stored;
  for (const wanted of navigator.languages ?? [navigator.language]) {
    const found = wanted && match(wanted);
    if (found) return found;
  }
  return configured;
}

export function I18nProvider({ children }: { children: ReactNode }) {
  const [languages, setLanguages] = useState<Language[]>([]);
  const [language, setCurrent] = useState('en_gb');
  const [texts, setTexts] = useState<Texts>({});
  const [english, setEnglish] = useState<Texts>({});

  const setLanguage = useCallback(async (code: string) => {
    const loaded = await loadTexts(code).catch(() => ({}));
    setTexts(loaded);
    setCurrent(code);
    const lang = code.split('_')[0];
    document.documentElement.lang = lang;
    document.documentElement.dir = RTL.includes(lang) ? 'rtl' : 'ltr';
    try {
      localStorage.setItem(STORAGE_KEY, code);
    } catch {
      // Storage can be blocked
    }
  }, []);

  useEffect(() => {
    loadTexts('en_gb').then(setEnglish);
    api
      .languages()
      .then((data) => {
        setLanguages(data.languages);
        return setLanguage(pickLanguage(data.languages, data.default));
      })
      .catch(() => setLanguage('en_gb'));
  }, [setLanguage]);

  const t = useCallback(
    (key: string, ...args: (string | number)[]) =>
      (texts[key] || english[key] || key).replace(/\{(\d+)\}/g, (all, i: string) =>
        args[Number(i)] !== undefined ? String(args[Number(i)]) : all,
      ),
    [texts, english],
  );

  return <I18nContext.Provider value={{ language, languages, setLanguage, t }}>{children}</I18nContext.Provider>;
}

export function useI18n(): I18n {
  const context = useContext(I18nContext);
  if (!context) throw new Error('useI18n needs an I18nProvider');
  return context;
}
