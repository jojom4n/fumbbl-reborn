// =============================================================================
// App — Entry point with authentication and debug support
// =============================================================================

import { useState, useEffect, useCallback } from 'react';
import DashboardLayout from './components/Dashboard/DashboardLayout';
import { FumbblDebugPanel } from './components/Debug/FumbblDebugPanel';
import { GameProvider } from './contexts/GameContext';
import { LoginScreen } from './components/Auth/LoginScreen';

// -----------------------------------------------------------------------------
// Storage abstraction: Tauri Store -> localStorage fallback
// Platform-specific paths are handled automatically by Tauri:
//   - Windows: %APPDATA%\com.saruman.fumbbl-reborn\fumbbl-credentials.json
//   - macOS:   ~/Library/Application Support/com.saruman.fumbbl-reborn/fumbbl-credentials.json
//   - Linux:   ~/.local/share/com.saruman.fumbbl-reborn/fumbbl-credentials.json
//   - Browser: localStorage
// -----------------------------------------------------------------------------

async function getStorage() {
  // Check if Tauri is available
  const isTauri = typeof (window as any).__TAURI_INTERNALS__ !== 'undefined';

  if (isTauri) {
    console.log('[App] Using Tauri Store (platform-specific path)');
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
        await store.save();
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
  username: string;
}

interface ConnectResult {
  credentials: Credentials;
  saveCredentials: boolean;
}

// -----------------------------------------------------------------------------
// OAuth2 Authentication
// -----------------------------------------------------------------------------

async function authenticateFumbbl(
  clientId: string,
  clientSecret: string
): Promise<{ accessToken: string; sessionToken: string } | null> {
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

    if (!response.ok) throw new Error("OAuth2 token request failed");

    const data = await response.json();
    const accessToken = data.access_token;

    // Get session token
    const sessionUrl = "https://fumbbl.com/api/auth/getToken";
    const sessionResponse = await fetch(sessionUrl, {
      method: "POST",
      headers: {
        "Accept": "application/json",
        "Authorization": `Bearer ${accessToken}`
      }
    });

    if (!sessionResponse.ok) throw new Error("Session token request failed");

    const tokenText = await sessionResponse.text();
    // CRITICAL: API returns JSON-encoded string, must parse
    let sessionToken: string;
    try {
      sessionToken = JSON.parse(tokenText);
    } catch {
      sessionToken = tokenText.trim();
    }

    return { accessToken, sessionToken };
  } catch (error) {
    console.error("[App] Errore autenticazione:", error);
    return null;
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
  const [savedCredentials, setSavedCredentials] = useState<Credentials | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [isConnecting, setIsConnecting] = useState(false);
  const [isAuthenticated, setIsAuthenticated] = useState(false);
  const [authError, setAuthError] = useState<string | null>(null);
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

  // Detect storage and load saved credentials
  useEffect(() => {
    const init = async () => {
      const s = await getStorage();
      setStorage(s);

      try {
        const saved = await s.get<Credentials>('fumbbl_credentials');
        if (saved) {
          console.log('[App] Credenziali salvate trovate');
          setSavedCredentials(saved);
        }
      } catch (e) {
        console.error("[App] Errore caricamento credenziali", e);
      } finally {
        setIsLoading(false);
      }
    };
    init();
  }, []);

  const handleConnect = useCallback(async (result: ConnectResult): Promise<boolean> => {
    const { credentials, saveCredentials } = result;
    setIsConnecting(true);
    setAuthError(null);

    try {
      // Step 1: Validate OAuth2 credentials
      const authResult = await authenticateFumbbl(credentials.clientId, credentials.clientSecret);
      if (!authResult) {
        setAuthError('Autenticazione OAuth2 fallita. Verifica Client ID e Client Secret.');
        return false;
      }

      // Step 2: Save credentials only if user opted in
      if (storage && saveCredentials) {
        await storage.set('fumbbl_credentials', credentials);
        console.log('[App] Credenziali salvate con successo');
      } else if (!saveCredentials) {
        console.log('[App] Credenziali NON salvate (scelta utente)');
      }

      // Step 3: Update state
      setSavedCredentials(credentials);
      setIsAuthenticated(true);
      return true;
    } catch (error) {
      setAuthError((error as Error).message || 'Errore durante la connessione');
      return false;
    } finally {
      setIsConnecting(false);
    }
  }, [storage]);

  const handleClearCredentials = useCallback(async () => {
    if (storage) {
      await storage.delete('fumbbl_credentials');
      console.log('[App] Credenziali rimosse');
    }
    setSavedCredentials(null);
    setIsAuthenticated(false);
  }, [storage]);

  const handleToggleDebug = () => {
    // Use history API to avoid page reload that resets React state
    setShowDebug(prev => {
      const next = !prev;
      const url = new URL(window.location.href);
      url.searchParams.set('debug', next.toString());
      window.history.pushState({ debug: next.toString() }, '', url);
      return next;
    });
  };

  // Pass credentials to GameProvider via serviceConfig (including username)
  const serviceConfig = savedCredentials ? {
    clientId: savedCredentials.clientId,
    clientSecret: savedCredentials.clientSecret,
    username: savedCredentials.username,
  } : {};

  if (isLoading) {
    return (
      <div className="h-screen w-screen bg-[#121212] flex items-center justify-center text-white">
        <div className="text-center">
          <div className="text-4xl mb-3">🏈</div>
          <div className="text-gray-500 text-sm">Caricamento...</div>
        </div>
      </div>
    );
  }

  return (
    <GameProvider serviceConfig={serviceConfig}>
      <div className="h-screen w-screen bg-[#121212] overflow-hidden flex flex-col">
        {isAuthenticated ? (
          <>
            <DashboardLayout
              onToggleDebug={handleToggleDebug}
              isDebugEnabled={showDebug}
              onLogout={handleClearCredentials}
              username={savedCredentials?.username}
            />
            {/* Debug panel as bottom overlay (doesn't affect main layout) */}
            {showDebug && (
              <div className="absolute bottom-0 left-0 right-0 z-50 max-h-[50vh] overflow-y-auto">
                <FumbblDebugPanel />
              </div>
            )}
          </>
        ) : (
          <LoginScreen
            onConnect={handleConnect}
            onClearCredentials={handleClearCredentials}
            savedCredentials={savedCredentials}
            isLoading={isConnecting}
            error={authError}
          />
        )}
      </div>
    </GameProvider>
  );
}

export default App;