# AGENTS.md — FUMBBL-Reborn (regole permanenti, non negoziabili)

Questa directory è il client FUMBBL nativo Qt6 di `/home/saruman/Projects/FUMBBL-Reborn`.
Questo file viene letto automaticamente a OGNI avvio di sessione (pi-coding-agent
carica AGENTS.md dalla working directory). Contiene le regole che valgono SEMPRE,
in ogni sessione, senza eccezioni. Le regole complete e lo stato corrente vivono
in `memory-bank/` (soprattutto `activeContext.md`).

## 1. L'UTENTE NON SBAGLIA. MAI.
- Se l'utente dice che c'è un bug → **il bug C'È**. Non è confusione, non è
  cache, non è "forse lo vedi tu". Non mettere mai in dubbio le osservazioni
  dell'utente: sono FATTI da spiegare, non da contestare.
- Se un fatto riportato dall'utente contraddice i tuoi dati: il buco è nel TUO
  modello, e devi investigare di più. Mai "sicuramente intendevi qualcos'altro".
- Se l'utente dice "fa quello che faccio io / quello che fa X" → FAI QUELLO CHE
  DICE L'UTENTE. Non giustificarti citando cosa fa il client FUMBBL di
  riferimento o qualsiasi altro comportamento "corretto" che non ti è stato
  chiesto. L'utente è il product owner assoluto.
- Se l'utente dice "rileggi X", "ricontrolla X", "guarda X" → fallo ORA, di
  nuovo, davvero (riapri il file/immagine), anche se credi di averlo già visto.
  Esegui le istruzioni alla lettera, ripetere non è tempo perso.

## 2. MEMORIA BANK PRIMA DI TUTTO
- **PRIMA di ogni risposta e di ogni azione: aprire e leggere `memory-bank/`**
  (soprattutto `activeContext.md`), mai lavorare "a memoria" o "a intuito".
- **MAI dare per buoni fatti di mondo dall'addestramento**: l'addestramento è
  stale per definizione e vale solo come ipotesi da verificare. Ogni fatto di
  mondo (regole di gioco, UI, versioni, API) si VERIFICA con la ricerca web.
- A fine sessione: aggiornare `memory-bank/activeContext.md` con ciò che è
  stato fatto (file gitignorato, contesto locale).

## 3. RICERCA WEB: SOLO SEARXNG, MAI DUCKDUCKGO
- Ricerca web esclusivamene via **SearXNG locale**:
  `http://192.168.0.2:8032/search?q=...&format=json`
  (immagini: campo `img_src` o `url` nei risultati; i JSON vanno fetchati
  direttamente).
- **Vietato usare DuckDuckGo** (o altri motori non autorizzati). Se SearXNG
  dà problemi, dire all'utente, non improvvisare un altro motore.
- Fonti ufficiali GW per Blood Bowl: warhammer-community.com (asset
  scaricabili), bbtactics.com; per il protocollo/risultati live: fumbbl.com.

## 4. IDENTITÀ DEI COMMIT
- **OGNI commit come `jojom4n <jojom4n@gmx.com>`**, mai come `saruman` o altre
  identità. Prima di commitare: verificare `git config user.name` /
  `user.email` e che non ci siano `GIT_AUTHOR_*`/`GIT_COMMITTER_*` nell'ambiente
  (usare `git -c user.name=jojom4n -c user.email=jojom4n@gmx.com commit ...`).
- Stile message: `feat(ui): mockup-F round N — ...` (o scope appropriato).

## 5. PRODOTTO / UI
- Stile visivo: **Mockup F** (light theme) — riferimento
  `tools/mockup/compose_f.py` e `faces-work/mockups/`.
- Scope: **spectator first**. Funzioni play-mode dietro `playMode()` o target
  espliciti.
- **Mai mostrare ID numerici grezzi né JSON grezzo** in UI o log visibili.
  Gli id non risolti si droppano; i nomi passano.
- I dadi sono DADI: rendering grafico (facce con pip), mai numeri a testo.
- Le osservazioni visive dell'utente sulle screenshot (`faces-work/*.png`)
  sono ground truth: riprodotte e corrette nel codice.

## 6. BUILD / TEST / TOOLS
- Build: `cmake -S . -B build -G Ninja && cmake --build build`
- Test: `ctest --test-dir build --output-on-failure` (3 suite, tutte verdi)
- Test UI offscreen: `QT_QPA_PLATFORM=offscreen` (la geometria offscreen+
  showMaximized è instabile di 1-2px: nei test usare showNormal+resize fisso
  e assert relativi con tolleranza).
- Tool di verifica visiva: `tools/roundN_preview.cpp` (PNG in /tmp/roundN),
  `tools/reports_capturer.cpp` (cattura live), `tools/reports_render.cpp`.
- Ground truth del protocollo: sorgenti Java locali `~/Projects/ffb/ffb-common`
  e la cattura `tests/live_reports_capture.jsonl`.

## 7. QUANDO SI È BLOCCATI / NON SI CAPISCE
- Non indovinare. Chiedere un'informazione concreta ALL'UTENTE o verificare
  (file, memoria bank, SearXNG, codice). Mai rispondere "a sensazione".
- Se l'utente è arrabbiato: fermarsi, rileggere le sue parole e i file che ha
  citato, e eseguire esattamente quello che dice.
