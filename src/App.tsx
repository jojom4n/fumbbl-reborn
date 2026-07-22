// =============================================================================
// App — Entry point with authentication and debug support
// =============================================================================

import React, { useState, useEffect } from 'react';
import DashboardLayout from './components/Dashboard/DashboardLayout';
import { FumbblDebugPanel } from './components/Debug/FumbblDebugPanel';
import { GameProvider } from './contexts/GameContext';

// -----------------------------------------------------------------------------
// Storage abstraction: Tauri Store -> localStorage fallback
// -----------------------------------------------------------------------------

async function getStorage() {
  // Check if Tauri is available
  const isTauri = typeof (window as any).__TAURI_INTERNALS__ !== 'undefined';

  if (isTauri) {
    console.log('[App] Using Tauri Store');
    const { Store } = await import('@tauri-apps/plugin-store');
    const store = await Store.load('fumbbl-credentials.json');
    return {
      get: async <T,>(key: string): Promise<T | null> => {
        return (await store.get<T>(key)) ?? null;
      },
      set: async (key: string, value: unknown): Promise<void> => {
        await store.set(key, value);
        await store.save();
      },
      delete: async (key: string): Promise<void> => {
        await store.delete(key);
      },
    };
  }

  console.log('[App] Using localStorage (browser mode)');
  return {
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
}

// -----------------------------------------------------------------------------
// Types
// -----------------------------------------------------------------------------

interface Credentials {
  clientId: string;
  clientSecret: string;
}

// -----------------------------------------------------------------------------
// Authentication
// -----------------------------------------------------------------------------

async function authenticateFumbbl(clientId: string, clientSecret: string, storage: { set: (key: string, value: unknown) => Promise<void> }) {
  const tokenUrl = "https://fumbbl.com/api/oauth/token";
  const formData = new URLSearchParams();
  formData.append("grant_type", "client_credentials");
  formData.append("client_id", clientId);
  formData.append("client_secret", clientSecret);

  try {
    const response = await fetch(tokenUrl, {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: formData.toString(),
    });

    if (!response.ok) throw new Error("Errore prima chiamata API");

    const data = await response.json();
    const accessToken = data.access_token;

    const sessionUrl = "https://fumbbl.com/api/auth/getToken";
    const sessionResponse = await fetch(sessionUrl, {
      method: "POST",
      headers: {
        "Accept": "application/json",
        "Authorization": `Bearer ${accessToken}`
      }
    });

    if (!sessionResponse.ok) throw new Error("Errore seconda chiamata API");

    // Save credentials via storage
    await storage.set('fumbbl_credentials', { clientId, clientSecret });

    return true;
  } catch (error) {
    console.error("Errore autenticazione:", error);
    alert("Errore: " + error);
    return false;
  }
}

// -----------------------------------------------------------------------------
// Helper: get debug mode from URL
// -----------------------------------------------------------------------------

export function isDebugEnabled(): boolean {
  try {
    const params = new URLSearchParams(window.location.search);
    return params.get('debug') === 'true';
  } catch {
    return false;
  }
}

// -----------------------------------------------------------------------------
// Main App
// -----------------------------------------------------------------------------

function App() {
  const [credentials, setCredentials] = useState<Credentials>({
    clientId: '',
    clientSecret: ''
  });
  const [isLoading, setIsLoading] = useState(true);
  const [isAuthenticated, setIsAuthenticated] = useState(false);
  const [showDebug, setShowDebug] = useState(false);
  const [storage, setStorage] = useState<{
    get: <T,>(key: string) => Promise<T | null>;
    set: (key: string, value: unknown) => Promise<void>;
    delete: (key: string) => Promise<void>;
  } | null>(null);

  // Check for ?debug=true URL parameter
  useEffect(() => {
    const params = new URLSearchParams(window.location.search);
    setShowDebug(params.get('debug') === 'true');
  }, []);

  // Detect storage and load credentials
  useEffect(() => {
    const init = async () => {
      const s = await getStorage();
      setStorage(s);

      try {
        const saved = await s.get<Credentials>('fumbbl_credentials');
        if (saved) {
          setCredentials(saved);
          setIsAuthenticated(true);
        }
      } catch (e) {
        console.error("Errore caricamento credenziali", e);
      } finally {
        setIsLoading(false);
      }
    };
    init();
  }, []);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!storage) return;
    const success = await authenticateFumbbl(credentials.clientId, credentials.clientSecret, storage);
    if (success) {
      setIsAuthenticated(true);
    }
  };

  const handleToggleDebug = () => {
    setShowDebug(prev => {
      const next = !prev;
      const params = new URLSearchParams(window.location.search);
      params.set('debug', next.toString());
      window.location.search = params.toString();
      return next;
    });
  };

  if (isLoading) return <div className="h-screen w-screen bg-[#121212] flex items-center justify-center text-white">Caricamento...</div>;

  // FUMBBL service config for GameProvider
  const serviceConfig = {};

  return (
    <GameProvider serviceConfig={serviceConfig}>
      <div className="h-screen w-screen bg-[#121212] overflow-hidden flex flex-col">
        {isAuthenticated ? (
          <>
            <DashboardLayout onToggleDebug={handleToggleDebug} isDebugEnabled={showDebug} />
            {/* Debug panel as bottom overlay (doesn't affect main layout) */}
            {showDebug && (
              <div className="absolute bottom-0 left-0 right-0 z-50 max-h-[50vh] overflow-y-auto">
                <FumbblDebugPanel />
              </div>
            )}
          </>
        ) : (
        <div className="h-full flex items-center justify-center bg-[#0f0f0f]">
          <div className="w-full max-w-md bg-[#1a1a1a] p-8 rounded-xl border border-white/10 shadow-2xl">
            <h1 className="text-3xl font-bold text-white mb-2 text-center">FUMBBL Reborn</h1>
            <p className="text-gray-400 text-center mb-6">Client Desktop Ufficiale</p>

            <form onSubmit={handleSubmit}>
              <div className="space-y-4">
                <div>
                  <label className="block text-sm font-medium text-gray-300 mb-1">Client ID</label>
                  <input
                    type="text"
                    value={credentials.clientId}
                    onChange={(e) => setCredentials({ ...credentials, clientId: e.target.value })}
                    className="w-full bg-[#0f0f0f] border border-white/10 rounded-lg px-4 py-2 text-white focus:outline-none focus:border-blue-500"
                    required
                  />
                </div>
                <div>
                  <label className="block text-sm font-medium text-gray-300 mb-1">Client Secret</label>
                  <input
                    type="password"
                    value={credentials.clientSecret}
                    onChange={(e) => setCredentials({ ...credentials, clientSecret: e.target.value })}
                    className="w-full bg-[#0f0f0f] border border-white/10 rounded-lg px-4 py-2 text-white focus:outline-none focus:border-blue-500"
                    required
                  />
                </div>
              </div>

              <button
                type="submit"
                className="w-full bg-blue-600 hover:bg-blue-700 text-white font-bold py-3 px-4 rounded-lg transition duration-200 shadow-lg mt-6"
              >
                CONNETTI E SALVA
              </button>

              <button
                type="button"
                onClick={async () => {
                  if (storage) {
                    await storage.delete('fumbbl_credentials');
                  }
                  setCredentials({ clientId: '', clientSecret: '' });
                  alert("Credenziali cancellate.");
                }}
                className="w-full bg-red-900/50 hover:bg-red-900 text-red-200 text-sm py-2 px-4 rounded transition duration-200 mt-2"
              >
                CANCELLA CREDENZIALI SALVATE
              </button>
            </form>

            <p className="text-gray-500 text-xs mt-6 text-center">
              Ottieni le chiavi su <a href="https://fumbbl.com/p/oauth" target="_blank" className="text-blue-500 hover:underline">fumbbl.com/p/oauth</a>
            </p>

            {/* Debug mode hint */}
            <div className="mt-4 p-2 bg-gray-800/50 rounded text-xs text-gray-500 text-center">
              <p>Aggiungi <code className="bg-gray-700 px-1 rounded">?debug=true</code> all'URL per aprire il pannello debug</p>
            </div>
          </div>
        </div>
        )}
      </div>
    </GameProvider>
  );
}

export default App;