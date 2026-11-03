# Changelog — fumbbl-qt

Format: una voce per versione, data, punti principali. La versione di
riferimento è `project(fumbbl-qt VERSION ...)` in `CMakeLists.txt`
(unica sorgente di verità; `src/main.cpp` la legge via `FUMBBL_VERSION`).

## 0.1.1 — 2026-11-03
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
