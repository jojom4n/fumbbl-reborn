import React, { useState, useEffect } from 'react';
import { Store } from '@tauri-apps/plugin-store';
import DashboardLayout from './components/Dashboard/DashboardLayout';

const STORE_FILE = 'fumbbl-credentials.json';

interface Credentials {
  clientId: string;
  clientSecret: string;
}

async function authenticateFumbbl(clientId: string, clientSecret: string) {
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
    
    // Save credentials
    const store = await Store.load(STORE_FILE);
    await store.set('credentials', { clientId, clientSecret });
    await store.save();

    return true; // Success
  } catch (error) {
    console.error("Errore autenticazione:", error);
    alert("Errore: " + error);
    return false;
  }
}

function App() {
  const [credentials, setCredentials] = useState<Credentials>({
    clientId: '',
    clientSecret: ''
  });
  const [isLoading, setIsLoading] = useState(true);
  const [isAuthenticated, setIsAuthenticated] = useState(false); // <-- STATE FOR THE DASHBOARD

  useEffect(() => {
    const loadCredentials = async () => {
      try {
        const store = await Store.load(STORE_FILE);
        const saved = await store.get<Credentials>('credentials');
        if (saved) {
          setCredentials(saved);
          setIsAuthenticated(true); // <-- IF CREDENTIALS EXIST, GO TO DASHBOARD
        }
      } catch (e) {
        console.error("Errore caricamento credenziali", e);
      } finally {
        setIsLoading(false);
      }
    };
    loadCredentials();
  }, []);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    const success = await authenticateFumbbl(credentials.clientId, credentials.clientSecret);
    if (success) {
      setIsAuthenticated(true); // <-- SWITCH TO DASHBOARD
    }
  };

  if (isLoading) return <div className="h-screen w-screen bg-[#121212] flex items-center justify-center text-white">Caricamento...</div>;

  return (
    <div className="h-screen w-screen bg-[#121212] overflow-hidden">
      {isAuthenticated ? (
        <DashboardLayout /> // <-- RENDER THE DASHBOARD
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
                  const store = await Store.load(STORE_FILE);
                  await store.delete('credentials');
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
          </div>
        </div>
      )}
    </div>
  );
}

export default App;
