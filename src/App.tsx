import React, { useState, useEffect } from 'react';
import { Store } from '@tauri-apps/plugin-store';

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
    
    const sessionData = await sessionResponse.json();
    
    // Salva le credenziali
    const store = await Store.load(STORE_FILE);
    await store.set('credentials', { clientId, clientSecret });
    await store.save();

    alert("Connessione riuscita! Credenziali salvate.");
  } catch (error) {
    console.error("Errore autenticazione:", error);
    alert("Errore: " + error);
  }
}

function App() {
  const [credentials, setCredentials] = useState<Credentials>({
    clientId: '',
    clientSecret: ''
  });
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    const loadCredentials = async () => {
      try {
        const store = await Store.load(STORE_FILE);
        const saved = await store.get<Credentials>('credentials');
        if (saved) {
          setCredentials(saved);
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
    await authenticateFumbbl(credentials.clientId, credentials.clientSecret);
  };

  if (isLoading) return <div className="min-h-screen bg-[#1a1b26] flex items-center justify-center text-white">Caricamento...</div>;

  return (
    <div className="min-h-screen bg-[#1a1b26] flex items-center justify-center p-4 font-sans">
      <div className="bg-[#24283b] p-8 rounded-xl shadow-2xl w-full max-w-md border border-[#414868]">
        <h1 className="text-3xl font-bold text-white mb-2 text-center">
          FUMBBL Reborn
        </h1>
        <p className="text-gray-400 text-center mb-6 text-sm">
          Inserisci le tue credenziali API per connetterti
        </p>
        
        <form onSubmit={handleSubmit} className="space-y-4">
          <div>
            <label className="block text-blue-400 text-xs font-bold uppercase mb-1 tracking-wider">Client ID</label>
            <input 
              type="text" 
              className="w-full bg-[#1f2335] text-white p-3 rounded-lg border border-[#414868] focus:border-blue-500 focus:ring-1 focus:ring-blue-500 focus:outline-none transition"
              placeholder="Inserisci Client ID"
              value={credentials.clientId}
              onChange={(e) => setCredentials({...credentials, clientId: e.target.value})}
            />
          </div>
          
          <div>
            <label className="block text-blue-400 text-xs font-bold uppercase mb-1 tracking-wider">Client Secret</label>
            <input 
              type="password" 
              className="w-full bg-[#1f2335] text-white p-3 rounded-lg border border-[#414868] focus:border-blue-500 focus:ring-1 focus:ring-blue-500 focus:outline-none transition"
              placeholder="Inserisci Client Secret"
              value={credentials.clientSecret}
              onChange={(e) => setCredentials({...credentials, clientSecret: e.target.value})}
            />
          </div>
          
          <button 
            type="submit" 
            className="w-full bg-blue-600 hover:bg-blue-700 text-white font-bold py-3 px-4 rounded-lg transition duration-200 shadow-lg mt-4"
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
  );
}

export default App;
