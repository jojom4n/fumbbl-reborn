# Changelog — fumbbl-qt

Format: una voce per versione, data, punti principali. La versione di
riferimento è `project(fumbbl-qt VERSION ...)` in `CMakeLists.txt`
(unica sorgente di verità; `src/main.cpp` la legge via `FUMBBL_VERSION`).

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
