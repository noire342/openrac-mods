# Punti stile: RetroAchievements

Mod nativa autonoma per **OpenRAC, Ratchet & Clank NTSC-U (SCUS_971.99)**.

Il set completo di [RetroAchievements, gioco 3070](https://retroachievements.org/game/3070),
fotografato il 28 ottobre 2026: **178 obiettivi, 667 punti**. I 30 punti stile originali
corrispondono a 30 achievement del set; vengono mantenuti senza duplicati e affiancati
da 148 nuove sfide.

## Installazione e uso

Importa `skill-points-ra-0.1.2-windows-x86_64.zip` dal menu **Mods** del launcher e attiva
**Punti stile: RetroAchievements**. In gioco apri **Pausa → Premi → Punti stile**:
la voce **Premi**, subito sotto **Opzioni**, è disponibile fin dall'inizio della partita.
Non occorre completare la storia o sbloccare gli extra per consultare gli obiettivi.
Usa i comandi originali: **Su/Giù** per scorrere e **Triangolo** per uscire.
In inglese il percorso è **Pause → Goodies → Skill Points**.

La versione **0.1.1** corregge l'accesso nella normale campagna: la 0.1.0 estendeva
la schermata ma lasciava nascosta la voce di pausa nelle nuove partite. Aggiorna la
mod dal launcher importando il nuovo ZIP; il profilo dei progressi rimane compatibile.

Dalla **0.1.2**, nelle impostazioni della mod trovi **Sfida a tempo di Kerwan**,
disattivata di default. Controlla soltanto l'obiettivo **Real Men Can Sving Vithout
Silly Toys** (ID 264462): percorso a ostacoli di Kerwan entro 1:08, senza helipack o
jetpack. Con il toggle spento la regola non viene valutata e la sfida non compare
nell'elenco e nei contatori: **177 obiettivi, 657 punti**. Riattivandolo dal launcher
tornano **178 obiettivi, 667 punti** al prossimo avvio. Eventuali sblocchi precedenti
restano nel profilo e ricompaiono riattivando la sfida. Non serve cambiare profilo.

Il contatore, i titoli e le descrizioni usano i pannelli, i caratteri, i colori e le
transizioni già presenti. L'elenco usa gli indicatori di completamento originali e
conserva i nomi localizzati dei primi 30 punti. I 30 punti originali conservano le illustrazioni originali;
le nuove sfide mostrano descrizione, stato e posizione nell'elenco. I titoli e le
descrizioni importati mantengono la lingua inglese del set ufficiale.

La mod funziona offline. Non richiede login durante il gioco, non invia sblocchi al sito
e non contiene password, token o chiavi API. Non richiede la mod delle armi né una
modifica di OpenRAC. Le due mod hanno DLL, identificatori e progressi distinti.

Il launcher ha l'indicazione di caricarla dopo `weapon-xp`, `difficulty-mod` e
`item-randomizer`, se presenti. Nei file `plugins.json` gestiti a mano mantieni questo
ordine: **le altre mod prima, `skill-points-ra` dopo**. Altre mod che intercettano
`rac1.level.load_gameplay` devono precederla: il relativo adattatore UI è descritto in
[ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Progressi e campagne

Il profilo predefinito è `campagna-1`. I progressi vengono scritti in modo atomico in:

```text
%LOCALAPPDATA%/OpenRAC/skill-points-ra/ntsc-campagna-1.spra
```

Il file è distinto dagli slot della memory card e dai dati di `weapon-xp`. Caricare un
vecchio salvataggio del gioco non annulla gli achievement locali già ottenuti. Per
una nuova campagna indipendente scegli un altro nome di profilo nelle impostazioni
della mod. Una seconda istanza non può scrivere contemporaneamente lo stesso profilo;
un file danneggiato viene rifiutato e preservato.

I 30 punti stile già presenti nel salvataggio vengono riconosciuti. Gli altri obiettivi
seguono le regole originali: per esempio, un'acquisizione che richiede il passaggio
da “non posseduto” a “posseduto” non viene attribuita retroattivamente solo perché
carichi un salvataggio che contiene già l'oggetto. I contatori di una singola sessione
non vengono trasformati in contatori permanenti. I requisiti sulle armi e le esclusioni
dei glitch presenti nel set rimangono nelle definizioni originali.

Gli obiettivi aggiunti non scrivono i flag dei 30 punti originali e non alterano le
condizioni originali di sblocco dello sketchbook o degli altri extra.

## API e struttura

L'API C versione 1 permette alle altre mod di leggere catalogo, progressi e totali e di
ricevere gli eventi di sblocco e arresto. Non permette di attribuire obiettivi a mano.
L'header è [include/skill_points_api.h](include/skill_points_api.h); l'esempio
[examples/achievement_observer.c](examples/achievement_observer.c) carica il servizio
dalla DLL già presente, senza una dipendenza di collegamento.

Il modello dei progressi, il motore rcheevos, la persistenza e l'adattatore del menu
sono separati e testabili. Vedi [API.md](docs/API.md) e
[ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Compilazione

Il sorgente comprende il motore MIT rcheevos, fissato al commit
`bbc79b58a5a2a0b98dfbf2be6ca9e42c764661ce`. L'SDK OpenRAC 1.3.0 viene verificato
tramite SHA-256 da `sdk.lock`.

```powershell
python tools/fetch_sdk.py
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

È possibile usare un SDK già estratto tramite `-DOPENRAC_SDK=<cartella>`. La normale
compilazione usa catalogo e indirizzi già generati; non richiede un checkout di OpenRAC
né accesso a RetroAchievements.

## Verifica

Cinque gruppi di test coprono tutte le definizioni, transizioni e duplicati, il formato
dei progressi e il blocco del profilo, la navigazione, l'adattatore UI sui 19 overlay e
il caricamento della DLL con un consumatore dell'API scritto in C.

La prova sul port Windows 0.9.0 ha verificato caricamento, menu originale esteso,
salvataggio e uno sblocco controllato attraverso `Item_Give(Pyrocitor)` in una sessione
di prova isolata. La prova con `weapon-xp` verifica anche che il menu non generi falsi
eventi di caricamento livello nella mod delle armi. Questi controlli **non equivalgono
al completamento manuale delle 178 sfide in una campagna**. PAL e altre revisioni del
gioco non sono dichiarate compatibili.

Per la 0.1.1 il percorso dalla pausa iniziale fino all'elenco è stato percorso con
input controller simulati dal port, senza scrivere la pagina attiva o la selezione.
Verificati anche ritorno al gioco e riapertura della pausa su una campagna non completata,
oltre all'aggancio dei menu sui 19 livelli. I salvataggi reali non sono stati modificati.

## Attribuzioni

Achievement: **Gollawiz, zizom e Fridge**, [set originale](https://retroachievements.org/game/3070).
Le definizioni originali e i relativi autori sono in `data/3070.json`; l'origine e la
mappatura dei punti originali sono in `data/provenance.json`.

Motore: [RetroAchievements/rcheevos](https://github.com/RetroAchievements/rcheevos),
copyright RetroAchievements.org, licenza MIT in `vendor/rcheevos/LICENSE`.
SDK: [OpenRAC](https://github.com/OpenRAC/OpenRAC). Codice della mod GPL-3.0-or-later;
header API ed esempio consumatore MIT.
