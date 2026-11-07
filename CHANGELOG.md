# Changelog — fumbbl-qt

Format: una voce per versione, data, punti principali. La versione di
riferimento è `project(fumbbl-qt VERSION ...)` in `CMakeLists.txt`
(unica sorgente di verità; `src/main.cpp` la legge via `FUMBBL_VERSION`).

## 0.2.8 — 2026-11-07
- **Login: il match di verifica non tocca più MAI l'UI** (fix
  strutturale, non di timing). Il protocollo FUMBBL verifica la
  password SOLO come parte del join di un game (verificato nel
  sorgente ffb-server: `FumbblRequestCheckAuthorization` approva il
  join — il client diventa per forza spettatore di quella partita e il
  server inizia subito a streamingarne lo stato). Ora, dalla partenza
  del join di verifica fino alla wipe post-login, TUTTI i dati di
  quella partita (game state, model sync, orologio, chat) sono
  soppressi nel controller: il campo resta sullo stadio vuoto durante
  e dopo il login, finché l'utente non sceglie una partita da
  Spectate Game. I rami di fallimento (rejection/close/fatal)
  alzano la soppressione.

## 0.2.7 — 2026-11-07
- **Credenziali in file: mai più plaintext**. Il file 0600
  (~/.config/fumbbl-reborn/creds) ora memorizza `password_md5=<hex>` —
  l'MD5 della password, che è proprio la credenziale del protocollo
  (il join invia HMAC-MD5(MD5(password), challenge); il client non ha
  mai più bisogno del testo chiaro). Stesso valore che già va nel
  keychain; nessun'altra libreria, nessun formato inventato. Al
  riavvio il dialog precompila coach + sentinel (pallini) e usa l'MD5;
  digitando una nuova password si sovrascrive.
- **Login: il match della burst di join non resta più visualizzato**.
  Il server invia gameState/modelSync nella stessa raffica del
  serverJoin: quei frame erano già in coda quando il login veniva
  chiuso, e ri-renderizzavano la partita DOPO la pulizia. La wipe dello
  stato partita ora corre al giro successivo di event loop (dopo i
  frame in coda; il socket intanto è in chiusura e non arrivano più
  stati).

## 0.2.6 — 2026-11-07
- **Remember me: la password non si perde più se il keychain non c'è**:
  il file 0600 veniva "stripato" (password rimossa) SUBITO al login,
  prima ancora che la scrittura keychain (asincrona) venisse confermata
  — e lo store vive nel dialog che muore subito dopo l'accept(). Senza
  Secret Service sul bus (es. Niri senza Kwallet avviato) la password
  finiva NESSUNAParte: campo vuoto al riavvio. Ora: la password digitata
  resta nel file 0600 come safety net fino alla conferma dello store
  (strip su conferma o al prossimo avvio quando il lookup trova l'entry);
  senza keychain il file resta il fallback stabile.
- **Login non apre più un match a caso**: il join di verifica atterrava
  su una partita live (l'ultima spettata, altrimenti la prima) e il
  client restava fermo A SPETTARLA. Il login abilita solo
  l'interfaccia: a verifica fatta la sessione di verifica viene chiusa
  (teardown silenzioso, close 1000 senza messaggio), lo stato della
  partita viene azzerato e lo status mostra "Logged in as … — choose a
  match". La partita si sceglie da Spectate Game.

## 0.2.5 — 2026-11-07
- **Login subito dopo una password sbagliata**: il server chiude la
  connessione (1000) nella stessa raffica dello status di rifiuto; quel
  close arrivava nel gate come fallimento della verifica in corso:
  dialog aperto con "Connection closed (1000: )" e login non completato,
  anche se la sessione era già live. Ora: il rifiuto viene segnalato
  PRIMA che il socket del tentativo venga abbattuto (il close di
  teardown arriva dopo che lo stato è già consumato, e resta silenzioso),
  e il gate ignora comunque i close che appartengono a un tentativo
  precedente (es. il close del logout in volo al retry).
- **Logout attivo già dal primo login**: il coach attivo era impostato
  solo quando si sceglieva una partita dal dialog Spectate; col gate di
  login restava vuoto (e Logout in grigio) fino ad allora. Il coach
  confermato dal server al login lo abilita subito.

## 0.2.4 — 2026-11-07
- **La X del dialog di login chiude davvero il client**: nel 0.2.3 il
  quit emesso dal gate andava perso — il gate gira PRIMA del main event
  loop (l'`exec()` del dialog) e l'`app.exec()` che partiva dopo non
  ricordava il quit: il dialog si chiudeva ma il client continuava a
  funzionare. Ora `main.cpp` esce quando il gate si chiude senza login
  (X / Esc / Quit).

## 0.2.3 — 2026-11-07
- **Bottone Login attivo mentre si digita**: lo stato enable/disable
  seguiva solo la perdita di focus del campo coach — col modulo
  compilato il bottone restava grigio e non cliccabile. Ora si
  ricalcola a ogni modifica di entrambi i campi.
- **La X del dialog di login chiude il client**: prima chiudeva solo il
  dialog e lasciava l'utente nell'UI completa senza login. Chiudere il
  gate (X / Esc) è ora la stessa decisione del bottone "Quit" — il
  client si chiude, l'UI non viene mai consegnata senza autenticazione.
- **Niente crash all'uscita col keychain in volo**: se il client
  chiudeva mentre la lookup/scrittura keychain era ancora in corso
  (round trip D-Bus lento), il thread di lavoro consegnava il risultato
  a un oggetto già distrutto (segfault). I worker ora verificano che il
  store esista prima di consegnare; il risultato in eccesso si scarta.

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
