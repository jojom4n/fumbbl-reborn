// =============================================================================
// Storage — Unified persistence abstraction
// Uses Tauri Store inside the desktop app (platform-specific paths handled
// automatically by Tauri:
//   - Windows: %APPDATA%\com.saruman.fumbbl-reborn\fumbbl-credentials.json
//   - macOS:   ~/Library/Application Support/com.saruman.fumbbl-reborn/
//   - Linux:   ~/.local/share/com.saruman.fumbbl-reborn/)
// and falls back to localStorage when running in a plain browser.
// =============================================================================

export interface Storage {
  get<T>(key: string): Promise<T | null>;
  set(key: string, value: unknown): Promise<void>;
  delete(key: string): Promise<void>;
}

let cached: Storage | null = null;

/**
 * Returns the available storage backend (singleton).
 * Tauri Store is preferred; localStorage is the browser fallback.
 */
export async function getStorage(): Promise<Storage> {
  if (cached) return cached;

  const isTauri = typeof (window as any).__TAURI_INTERNALS__ !== 'undefined';

  if (isTauri) {
    console.log('[Storage] Using Tauri Store (platform-specific path)');
    const { Store } = await import('@tauri-apps/plugin-store');
    const store = await Store.load('fumbbl-credentials.json');
    cached = {
      get: async <T,>(key: string): Promise<T | null> => {
        return (await store.get<T>(key)) ?? null;
      },
      set: async (key: string, value: unknown): Promise<void> => {
        await store.set(key, value);
        await store.save();
      },
      delete: async (key: string): Promise<void> => {
        await store.delete(key);
        await store.save();
      },
    };
    return cached;
  }

  console.log('[Storage] Using localStorage (browser mode)');
  cached = {
    get: async <T,>(key: string): Promise<T | null> => {
      const val = localStorage.getItem(key);
      return val ? JSON.parse(val) : null;
    },
    set: async (key: string, value: unknown): Promise<void> => {
      localStorage.setItem(key, JSON.stringify(value));
    },
    delete: async (key: string): Promise<void> => {
      localStorage.removeItem(key);
    },
  };
  return cached;
}
