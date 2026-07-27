// =============================================================================
// LoginScreen — Menu principale per autenticazione OAuth2 FUMBBL
// =============================================================================

import { useState, useEffect } from 'react';

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

interface LoginScreenProps {
  /** Callback fired when user clicks "Connetti" */
  onConnect: (result: ConnectResult) => Promise<boolean>;
  /** Callback fired when user clicks "Cancella Credenziali" */
  onClearCredentials: () => void;
  /** Pre-loaded saved credentials (optional) */
  savedCredentials?: Credentials | null;
  /** Current loading state */
  isLoading: boolean;
  /** Current authentication error (optional) */
  error?: string | null;
}

// -----------------------------------------------------------------------------
// LoginScreen Component
// -----------------------------------------------------------------------------

export function LoginScreen({
  onConnect,
  onClearCredentials,
  savedCredentials,
  isLoading,
  error,
}: LoginScreenProps) {
  const [credentials, setCredentials] = useState<Credentials>({
    clientId: savedCredentials?.clientId ?? '',
    clientSecret: savedCredentials?.clientSecret ?? '',
    username: savedCredentials?.username ?? '',
  });

  const [showSecret, setShowSecret] = useState(false);
  const [saveCredentials, setSaveCredentials] = useState(!!savedCredentials);

  // Update form when savedCredentials change
  useEffect(() => {
    if (savedCredentials) {
      setCredentials({
        clientId: savedCredentials.clientId,
        clientSecret: savedCredentials.clientSecret,
        username: savedCredentials.username,
      });
      setSaveCredentials(true);
    }
  }, [savedCredentials]);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    const success = await onConnect({ credentials, saveCredentials });
    if (!success) {
      // On failure, do not clear form so user can retry
    }
  };

  const hasSavedCredentials = savedCredentials && (
    savedCredentials.clientId || savedCredentials.clientSecret
  );

  return (
    <div className="h-full flex items-center justify-center bg-gradient-to-br from-[#0a0a0a] via-[#121212] to-[#0a0a0a]">
      <div className="w-full max-w-md">
        {/* Logo / Header */}
        <div className="text-center mb-8">
          <div className="text-6xl mb-4">🏈</div>
          <h1 className="text-4xl font-bold text-white mb-2 tracking-tight">
            FUMBBL <span className="text-blue-500">Reborn</span>
          </h1>
          <p className="text-gray-500 text-sm">Client Desktop Ufficiale — Blood Bowl</p>
        </div>

        {/* Login Card */}
        <div className="bg-[#1a1a1a] p-8 rounded-2xl border border-white/10 shadow-2xl">
          <h2 className="text-xl font-semibold text-white mb-1 text-center">
            {hasSavedCredentials ? 'Reconnessione' : 'Autenticazione'}
          </h2>
          <p className="text-gray-500 text-xs text-center mb-6">
            {hasSavedCredentials
              ? 'Credenziali trovate — clicca Connetti per accedere'
              : 'Inserisci le credenziali OAuth2 FUMBBL'}
          </p>

          {/* Error Message */}
          {error && (
            <div className="mb-4 p-3 bg-red-900/30 border border-red-800/50 rounded-lg">
              <p className="text-red-400 text-xs text-center">⚠️ {error}</p>
            </div>
          )}

          <form onSubmit={handleSubmit}>
            <div className="space-y-4">
              {/* Client ID */}
              <div>
                <label className="block text-xs font-medium text-gray-400 mb-1.5">
                  Client ID
                </label>
                <input
                  type="text"
                  value={credentials.clientId}
                  onChange={(e) => setCredentials({ ...credentials, clientId: e.target.value })}
                  className="w-full bg-[#0f0f0f] border border-white/10 rounded-lg px-4 py-2.5 text-white text-sm focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500/50 transition-all placeholder:text-gray-600"
                  placeholder="app_client_id"
                  required
                />
              </div>

              {/* Client Secret */}
              <div>
                <label className="block text-xs font-medium text-gray-400 mb-1.5">
                  Client Secret
                </label>
                <div className="relative">
                  <input
                    type={showSecret ? 'text' : 'password'}
                    value={credentials.clientSecret}
                    onChange={(e) => setCredentials({ ...credentials, clientSecret: e.target.value })}
                    className="w-full bg-[#0f0f0f] border border-white/10 rounded-lg px-4 py-2.5 pr-10 text-white text-sm focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500/50 transition-all placeholder:text-gray-600"
                    placeholder="••••••••••••••••"
                    required
                  />
                  <button
                    type="button"
                    onClick={() => setShowSecret(!showSecret)}
                    className="absolute right-3 top-1/2 -translate-y-1/2 text-gray-500 hover:text-gray-300 text-xs"
                  >
                    {showSecret ? '🙈' : '👁️'}
                  </button>
                </div>
              </div>

              {/* Username */}
              <div>
                <label className="block text-xs font-medium text-gray-400 mb-1.5">
                  Username FUMBBL
                </label>
                <input
                  type="text"
                  value={credentials.username}
                  onChange={(e) => setCredentials({ ...credentials, username: e.target.value })}
                  className="w-full bg-[#0f0f0f] border border-white/10 rounded-lg px-4 py-2.5 text-white text-sm focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500/50 transition-all placeholder:text-gray-600"
                  placeholder="il_tuo_username_fumbbl"
                  required
                />
                <p className="text-[10px] text-gray-600 mt-1">
                  Il tuo nome coach su fumbbl.com
                </p>
              </div>
            </div>

            {/* Save Credentials Checkbox */}
            <div className="mt-4">
              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="checkbox"
                  checked={saveCredentials}
                  onChange={(e) => setSaveCredentials(e.target.checked)}
                  className="w-4 h-4 accent-blue-500 rounded"
                />
                <span className="text-sm text-gray-300">
                  {saveCredentials ? '💾 Salva credenziali per il prossimo avvio' : '🔒 Non salvare credenziali'}
                </span>
              </label>
            </div>

            {/* Connect Button */}
            <button
              type="submit"
              disabled={isLoading}
              className="w-full bg-blue-600 hover:bg-blue-700 disabled:bg-blue-800 disabled:cursor-not-allowed text-white font-bold py-3 px-4 rounded-lg transition-all duration-200 shadow-lg shadow-blue-900/30 mt-4 flex items-center justify-center gap-2"
            >
              {isLoading ? (
                <>
                  <svg className="animate-spin h-4 w-4 text-white" xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24">
                    <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4"></circle>
                    <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path>
                  </svg>
                  Connessione in corso...
                </>
              ) : (
                <>
                  🔗 {hasSavedCredentials ? 'Connetti' : 'Connetti'}
                </>
              )}
            </button>

            {/* Clear Credentials Button */}
            {hasSavedCredentials && (
              <button
                type="button"
                onClick={() => {
                  onClearCredentials();
                  setCredentials({ clientId: '', clientSecret: '', username: '' });
                  setSaveCredentials(false);
                }}
                className="w-full bg-red-900/30 hover:bg-red-900/50 border border-red-800/30 text-red-400 text-sm py-2 px-4 rounded-lg transition-all duration-200 mt-3"
              >
                🗑️ Cancella Credenziali Salvate
              </button>
            )}
          </form>

          {/* OAuth Info */}
          <div className="mt-6 p-3 bg-gray-800/30 rounded-lg border border-white/5">
            <p className="text-gray-500 text-[11px] text-center leading-relaxed">
              Ottieni le chiavi OAuth2 su{' '}
              <a
                href="https://fumbbl.com/p/oauth"
                target="_blank"
                rel="noopener noreferrer"
                className="text-blue-500 hover:text-blue-400 underline"
              >
                fumbbl.com/p/oauth
              </a>
            </p>
          </div>
        </div>

        {/* Footer */}
        <p className="text-gray-700 text-[10px] text-center mt-4">
          FUMBBL Reborn v0.1.0 — Non affiliato con Cyan
        </p>
      </div>
    </div>
  );
}

export default LoginScreen;