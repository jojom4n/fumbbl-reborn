# Changelog — fumbbl-qt

Format: una voce per versione, data, punti principali. La versione di
riferimento è `project(fumbbl-qt VERSION ...)` in `CMakeLists.txt`
(unica sorgente di verità; `src/main.cpp` la legge via `FUMBBL_VERSION`).

## 0.2.2 — 2026-11-06
- **Login al client all'avvio**: dialog modale (Coach + Password +
  "Remember me") sopra lo stadio — niente è cliccabile (nemmeno i menu)
  finché il login non è verificato. Le credenziali sono verificate contro
  il server FUMBBL al momento dell'accesso (join attempt: il server
  controlla la password nel join — `FumbblRequestCheckAuthorization`):
  ok → UI completa (si è già nello stadio della partita di verifica,
  ultima guardata o prima live); rifiuto → errore del server nel dialog,
  si riprova. "Remember me" (default on) → keychain `{coach, MD5}`.
- **Campo password sempre a pallini**: echo mode Password in ogni
  momento (anche durante la digitazione); con entry in keychain il campo
  mostra 8 pallini (sentinel) — la prima pressione lo sostituisce; la
  password digitata ha sempre la priorità sull'MD5 salvata (stale).
- **Dialog "Spectate" senza credenziali**: solo lista partite + filtri +
  Game ID + Spectate/doppio click — usa le credenziali del login del
  client. Rimosso anche il bottone "Load matches" residuo.
- **Logout (Ctrl+K) → login gate riaperto**: fine sessione + credenziale
  ricordata rimossa dal file (l'entry keychain resta); l'utente inserisce
  nuove credenziali oppure chiude il client dal button "Quit" del dialog.
- **Chat log: word wrap** — i messaggi lunghi vanno a capo invece di
  uscire dal pannello.

## 0.2.1 — 2026-11-06
- **Log out (PC condiviso)**: `File ▸ Log out` (Ctrl+K) chiude la sessione
  del coach attivo e rimuove la credenziale ricordata: il dialog del
  giocatore successivo si apre vuoto. L'entry nel keychain resta: un coach
  che torna ripassa solo il nome, mai la password.
- **Cambio coach nel dialog**: editando il campo coach, password digitata e
  MD5 salvata del coach precedente vengono scartate e il nuovo coach è
  risolto dal keychain (se già ricordato, niente re-digitazione).
- **Password digitata ha priorità**: se c'è una password nel campo, il
  login usa il suo MD5 anche se nel keychain c'è un MD5 stantio (coach
  cambiato, password di sito cambiata).
- **Player Info non appare all'avvio**: il pannello è guidato solo dall'
  hover sul campo — lo stato "visibile" salvato era il residuo dell'ultimo
  hover, non una scelta. Posizione/dimensioni salvate restano.

## 0.2.0 — 2026-11-06
- **Login ufficiale FFB (coach + password)**: sostituisce l'OAuth2 a token
  REST. Il client ora fa lo stesso handshake del client Java ufficiale:
  `clientRequestVersion` → `clientPasswordChallenge` → risposta
  challenge/response (HMAC-MD5 di `MD5(password)`) → `clientJoin`, tutto sul
  WebSocket. Niente Client ID/Secret, niente token REST per l'auth.
  Flusso verificato live contro fumbbl.com.
- **Keychain di sistema per le credenziali** ("Remember credentials",
  default on): il keychain (Windows Credential Manager, macOS Keychain,
  Linux Secret Service) memorizza `{coach, MD5(password)}` — mai il
  plaintext. Senza keychain disponibile: fallback a file locale `0600`
  (coach + password), come prima.
- **Dialog "Spectate Game…" riscritto**: campi Coach/Password (al posto di
  Client ID/Secret/Username), warning sul keychain, pre-compilazione dalle
  credenziali salvate; se il keychain ha l'MD5 la password non va più
  riscritta.
- **Nessun auto-retry su login rifiutato**: un `serverStatus` di rifiuto
  (es. "Wrong Password") è un errore visibile e fermo — il server limita i
  fallimenti ripetuti. Il retry automatico (con challenge fresca) resta
  solo per il timeout silenzioso senza risposta.
- Test: `passwordchallenge_test` con vettori ground truth generati dalla
  classe Java ufficiale; contratti di auth del dialog in `ui_wiring_test`.

## 0.1.1 — 2026-11-03
- **Release distribuibili** (GitHub Releases, tag `v*`): binario
  `FUMBBL-Reborn`, bundle Linux self-contained (tar.gz + AppImage) e zip
  Windows, con Qt e plugin inclusi (`qt.conf` pinna il layout flat; plugin
  tls incluso per l'HTTPS verso fumbbl.com). CI: Qt 6.8.3 precompilato
  via `jurplel/install-qt-action` (niente build da sorgente) + AppImage
  con `linuxdeploy` (v. `.github/workflows/release.yml`).
- **Icona dell'app**: casco verde/oro generato con Qwen-Image
  (`share/icon.png` per AppImage/bundle, `share/icon.ico` incorporato
  nell'exe Windows).
- Versioning reale del client (sostituisce la gestione a "round"): versione
  unica in CMake, mai hardcodata nel codice.
- Cleanup completo del codice: commenti tecnici e brevi (nessun riferimento
  a round/turn di sviluppo), rimozione di API morte (es.
  `GameModel::findPlayerByNumber`, `FieldView::setTurn`, i setter dei nomi
  team non usati, `HeaderView::fixedHeight`).
- Il chat del client ora trasmette via WebSocket quando la sessione è
  joined (`FumbblWs::sendChat`).
- Ridipinta la strip dei box una sola volta (nessuna pittura ridondante
  nella passata zoomata).
- Renamed tool di verifica: `round13_preview` → `hud_preview`,
  `round14_preview` → `move_numbers_preview` (nomi funzionali).
- Rimossa la duplicazione del blocco `DICE_RCC` in CMake.

## 0.1.0 — 2026-11-02
- Prima versione tracciata del client spettatore nativo Qt6
  (stadio, campo, dadi, action log, chat, dugout, pioggia).
