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
  identità. `jojom4n@gmx.com` è l'UNICA email valida: **mai inventare email
  "plausibili"** (incidente 26/ott: un commit è uscito con
  `jojom4n@fumbbl-reborn` — email fabbricata — e un altro storico era rimasto
  `saruman <saruman@local>`; entrambi riscritti via rebase, erano nel range
  non pushato).
- Prima di commitare: verificare `git config user.name` / `user.email` e che
  non ci siano `GIT_AUTHOR_*`/`GIT_COMMITTER_*` nell'ambiente (usare
  `git -c user.name=jojom4n -c user.email=jojom4n@gmx.com commit ...`).
- **Dopo un batch di commit: verificare l'INTERO log**
  (`git log --format="%an <%ae> %cn <%ce>" | sort -u`), non solo l'ultimo.
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

## 5bis. ASSET GRAFICI E FONTI UFFICIALI (PERMANENTI, richieste esplicitamente)
- **Tutti gli asset grafici si fanno SEMPRE con Qwen-Image** (ComfyUI, CPU),
  MAI disegnati a mano in QPainter/codice (niente simboli/icone vettoriali
  "fatti a mano" in codice). Il codice (QPainter) serve solo per la struttura
  vettoriale del CAMPO (griglia, linee, testo) e per il layout; ogni elemento
  grafico (simboli, icone, texture, sfondi) è un asset Qwen.
- **I simboli ufficiali (dugout Reserves/KO/Injured, ecc.) devono essere
  PERFETTI e PRESSOCHÉ IDENTICI a quelli ufficiali della GW**: si parte
  SEMPRE dall'immagine ufficiale di riferimento e si replica con Qwen-Image
  (text2image o image2image sul riferimento), non approssimazioni.
- **SOLO EDIZIONE 2025 (BB2025 / Third Season, Games Workshop)**: il client è
  per BB2025. **Scartare TUTTE le informazioni dell'edizione 2020** e non
  cercare MAI nulla dell'edizione 2020 (rulebook 2020, asset 2020, ecc.):
  le uniche fonti affidabili sono quelle dell'edizione 2025.
- Fonti ufficiali GW per BB2025: warhammer-community.com (articoli,
  download, digi-rulebook su assets.warhammer-community.com), foto ufficiali
  dei prodotti (box/pitch/dugout 2025), bbtactics.com.
- **4K-READY**: tutti gli asset (stadio, pubblico, texture, dadi, volti)
  devono essere pronti per 4K e belli su monitor 4K (generare ad alta
  risoluzione e/o upscalare; niente asset pixelati/soft a 4K).

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
- **La GPU è occupata da ME**: l'agente è servito da `llama-server`
  (Qwen3.8-27B, `-ngl 99`) che gira sulla GPU. rocm-smi mostra quindi
  sempre molta VRAM usata: NON è un leak, NON è un processo orfano, e
  "No KFD PIDs" non è una prova di GPU libera. ComfyUI (e qualsiasi altro
  carico) va SEMPRE su CPU (`--cpu`): la GPU non si "libera" mai mentre
  l'agente gira.

## 7. QUANDO SI È BLOCCATI / NON SI CAPISCE
- Non indovinare. Chiedere un'informazione concreta ALL'UTENTE o verificare
  (file, memoria bank, SearXNG, codice). Mai rispondere "a sensazione".
- Se l'utente è arrabbiato: fermarsi, rileggere le sue parole e i file che ha
  citato, e eseguire esattamente quello che dice.
