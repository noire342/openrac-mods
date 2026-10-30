# Cinematic Journeys 0.1.1

Mod indipendente per Ratchet & Clank / OpenRAC, PAL e NTSC-U.

- Impedisce lo skip accidentale dei filmati, delle cutscene e di decollo/atterraggio.
- Per saltare volontariamente: **tieni L1 + L2 + R1 + R2 e premi Start**.
- Mantiene disponibili Start nei menu e la pausa durante il gioco.
- Impostazione **Durata minima del viaggio (secondi)** nel menu Mods del launcher: 5 di default, da 0 a 30. Le impostazioni si applicano al successivo avvio del gioco.
- Il minimo riguarda il volo ordinario tra pianeti. Il gioco completa il passaggio animato in corso prima di proseguire: la durata effettiva puo superare il minimo. Gli arrivi speciali della storia che iniziano gia con il passaggio finale mantengono la sequenza originale.
- La combinazione completa consente di abbreviare anche il viaggio. Il caricamento effettivo deve comunque terminare.

Installa lo ZIP Windows dal menu Mods del launcher e abilita Cinematic Journeys. Non richiede una versione modificata di OpenRAC e non cambia i salvataggi. Nessun nuovo pannello nell'HUD.

La protezione usa i pulsanti logici PS2: valgono anche le corrispondenti associazioni della tastiera. Il comando di sviluppo `--skip-movies` salta i filmati prima che il giocatore della mod riceva input e non e coperto. Altre mod che riscrivono i comandi dopo questa possono cambiare il risultato; mantieni Cinematic Journeys dopo le mod di rimappatura.

## OpenRAC 0.10.0

La 0.1.1 è verificata sul client ufficiale Windows NTSC-U 0.10.0. Mantiene API 1.1,
con build e test anche su SDK 1.10.1. Aggiunge controlli dei servizi richiesti,
dei report nel guest stack del port e del caricamento fallito. I tre gruppi di test
coprono policy, runtime e ciclo di caricamento. [Compatibilità e riproduzione](docs/COMPATIBILITY.md).

## Struttura

`policy.*` contiene regole e temporizzazione senza SDK; `discs.hpp` concentra le differenze PAL/NTSC; `runtime.*` adatta memoria, clock e ordine dei callback; `plugin.cpp` gestisce solo caricamento e callback C. Il clock viene iniettato, quindi i test non aspettano secondi reali. Nessuna dipendenza dalle altre mod di questa cartella di lavoro.

## Build

SDK ufficiale OpenRAC mod SDK 1.10.1. Windows x64, CMake e Visual Studio:

```powershell
python tools/fetch_sdk.py
cmake -S . -B build -DOPENRAC_SDK="C:/path/openrac-mod-sdk-1.10.1"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

L'output installabile e in `build/package/cinematic_journeys`. Il pacchetto sorgenti include i test; non include dati del gioco, ISO o salvataggi.

Il download dell'SDK e fissato da `sdk.lock` e verificato con SHA-256. Per i log diagnostici della mod: variabile `CINEMATIC_JOURNEYS_TRACE=1`. La fixture nativa di transizione si compila solo con `-DCINEMATIC_BUILD_QA=ON` e non fa parte dello ZIP installabile.
