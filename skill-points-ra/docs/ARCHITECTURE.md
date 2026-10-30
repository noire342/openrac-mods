# Struttura e contratti

| Componente | Responsabilità |
| --- | --- |
| `Definition` / catalogo generato | ID, autore, testo e regola originali immutabili |
| `Ledger` | Progressi permanenti, idempotenza, formato versionato e checksum |
| `Engine` | Adattatore del runtime ufficiale rcheevos; dipende da `IMemory` |
| `AchievementPolicy` | Selezione delle sfide attive, indipendente dagli sblocchi permanenti |
| `FileStore` | Profilo esclusivo, sostituzione atomica, rifiuto dei dati danneggiati |
| `Menu` | Ordine 30 originali + 148 nuovi e navigazione circolare |
| `NativeUi` | Collegamento ai pannelli e ai renderer originali |
| `PauseAccess` | Visibilità e navigazione del pannello Premi già presente sotto Opzioni |
| `GameMemory` (`game_memory.hpp`) | Letture degli operandi RA limitate alla RAM PS2 |
| `Runtime` (`runtime.*`) | Composizione delle dipendenze e ciclo del gioco |
| Servizio C (`service.*`) | Catalogo, totali ed eventi attraverso ABI v1 |
| `plugin.cpp` | Validazione API, caricamento e inoltro degli hook |

Il modello e la logica dei progressi non dipendono dall'SDK. `IMemory` consente test
con memoria sintetica e un adattatore di gioco separato. L'API pubblica usa strutture
C con dimensione/versione e non espone classi C++ o dettagli del salvataggio.

Ogni obiettivo ha un'istanza rcheevos distinta: una lettura indiretta di dati assenti
su un altro pianeta non blocca il resto del set. Le letture sono limitate alla RAM
PS2 di 32 MiB. Uno sblocco che usa un operando illeggibile viene scartato e riarmato.
Le condizioni, gli hit counter, le pause, i reset e le misure rimangono gestiti da
rcheevos. I contatori di sessione non vengono serializzati come progressi permanenti.

La selezione è configurazione di sessione: non modifica il catalogo importato o il
`Ledger`. Le regole escluse non vengono eseguite; un cambio di selezione azzera lo
stato temporaneo della relativa regola. Il menu filtra gli ID esclusi preservando
ordine e selezione, e calcola i contatori soltanto sulle voci attive. Il servizio C
mantiene il catalogo storico e aggiunge letture separate per i totali attivi.

Gli operandi RA indicano già indirizzi NTSC-U della RAM attiva. Non vengono rilocati
una seconda volta come se fossero indirizzi dell'eseguibile di avvio. Gli oggetti UI
sono invece individuati mediante la coppia verificata di callback originali; pagina,
schermate ed entrata Goodies vengono raggiunte seguendo i puntatori del gioco.

`PauseAccess` segue il collegamento alla pausa dalla pagina Goodies e verifica i
sette pannelli originali, le etichette localizzate e la destinazione del settimo.
Durante la pausa abilita soltanto `MenuSystem.unkD8`, usato dal renderer per lo slot 6,
e collega Su/Giù a quel pannello. Il gioco riscrive i collegamenti a ogni apertura:
l'adattatore li aggiorna anche alla riapertura. Ripristina il flag quando si lascia
la pausa e ripristina soltanto i campi ancora posseduti durante l'unload.
Non cambia i flag della storia, della memory card o delle ricompense; sketchbook,
armi d'oro e altri extra conservano i requisiti originali. Il pannello conserva il
nome originale **Premi/Goodies** e apre tutte le voci native, inclusi i Punti Stile.

## Adattatore del menu sullo stock SDK

Lo stock SDK non registra nuove funzioni guest. Il menu chiama funzioni attraverso
indirizzi guest, quindi l'adattatore usa il punto esistente `rac1.level.load_gameplay`,
il cui contratto `u32(i32)` è stato verificato nell'eseguibile installato. Gli indirizzi
degli oggetti menu sono positivi e inferiori a 32 MiB; i ritorni sono flag non negativi:
parametro e risultato hanno la stessa rappresentazione ABI a 32 bit dei callback menu.

Gli indirizzi per i 19 overlay sono generati dalla tabella del commit OpenRAC fissato.
Prima di modificare un campo viene verificata l'identità della funzione originale e
del relay. Si sostituiscono solo callback di disegno degli oggetti già presenti nella
pagina Punti Stile (stato 0x24, elenco con flag 0x200). Il gestore originale dei comandi,
la voce del Menu Premi e lo sketchbook vengono conservati. Non si modifica codice macchina e non si aggiungono
pannelli o gestori di tasti globali. Gli indici che il codice originale legge rimangono
sempre nell'intervallo 0–29; l'indice completo vive nella mod.

Il relay intercetta soltanto gli indirizzi degli oggetti posseduti, mentre la pagina
Punti stile è quella attiva. I veri caricamenti vengono inoltrati all'originale una
sola volta, dopo avere ripristinato i callback. Anche l'unload ripristina solo i campi
che contengono ancora il valore scritto dalla mod, preservando modifiche successive.

Questo relay deve essere l'ultimo nella catena degli hook di caricamento: gli altri
intercettori non devono vedere una chiamata UI come un caricamento. Il manifest indica
perciò `load.after` per le mod note. Il test nativo con `weapon-xp` osserva la sua API
e richiede zero falsi eventi `LEVEL_LOADED` durante la navigazione. Un futuro SDK con
registrazione esplicita di callback potrà sostituire soltanto questo adattatore.

## Provenienza e aggiornamenti

`data/3070.json` contiene soltanto il set, senza dati di autenticazione o progressi
di account. Il generatore richiede 178 ID unici e 667 punti, identifica i 30 punti
originali e conserva la definizione completa con SHA-256. Il checksum del file e la
provenienza sono in `data/provenance.json`. Rcheevos e SDK hanno versioni fissate.

Per aggiornare il set occorre rieseguire generazione e test e verificare gli indirizzi
sul port; il gioco non scarica aggiornamenti delle regole in modo automatico.

La 0.1.3 verifica il prefisso API usato tramite `size`, versione e puntatori dei servizi;
non confronta `sizeof(openrac_api)` dell’SDK corrente con un client precedente.
SDK 1.10.1 compila ancora una DLL API 1.1. Il generatore del relay accetta
`--source` e `--revision`: nessun percorso privato o checkout fratello è richiesto.
