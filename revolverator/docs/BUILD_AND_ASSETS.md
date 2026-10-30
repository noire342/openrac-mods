# Build e asset della 0.2

La mod richiede R&C1 NTSC-U e gli asset estratti dalla propria copia del gioco.
I builder leggono il formato livelli 3; non richiedono R&C2 e non modificano la
ISO. I sorgenti pubblicabili non includono il trapano o le texture originali.

## Plugin

Dal checkout `rac1-mods`, con CMake, un compilatore C++17 e SDK OpenRAC 1.3:

```powershell
cmake -S revolverator -B <build> -DOpenRACMod_DIR=<sdk>/cmake -DBUILD_TESTING=ON
cmake --build <build> --config Release
ctest --test-dir <build> -C Release --output-on-failure
```

Il package formato 2 viene scritto sotto `<build>/package/revolverator`. Il
manifest richiede ABI 1.1 e gli hook nominati in `openrac-mod.json`. I test host
coprono controller, presa/posa, munizioni, rotore, registro, sidecar, menu, alias
e consumer XP; non sostituiscono una prova nel gioco. Su Linux il target usa
`${CMAKE_DL_LIBS}` e un export esplicito per `openrac_revolverator_query`.

## Client e provider richiesti

La 0.2 usa il client ufficiale OpenRAC Windows x64 NTSC-U invariato, base
`77a2f052`, SHA256
`b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081`.
La mod Arsenal Compatibility 0.1.0 si carica per prima, risolve 30 funzioni via
`api->function` e prepara detour disabilitati con MinHook 1.3.4. La loro
attivazione richiede un consumer unico e un commit completo; gli altri 24 hook
Revolverator rimangono registrati nell'SDK ufficiale.

Non serve una patch engine, una ricompilazione del client o un EXE sostitutivo.
La vecchia patch del bridge resta soltanto documentazione storica. Per XP
opzionali usare il companion `weapon-xp 0.6.3-arsenal` con API v4.

## Asset aggiuntivi

Gli esempi usano cartelle di output nuove e percorsi scelti localmente:

```powershell
python revolverator/tools/build_model_pack.py --levels <livelli-ntsc> --editor <editor-openrac> --out <trapano>
python revolverator/tools/build_ammunition.py --levels <livelli-ntsc> --weapon-pack <trapano> --out <livelli-estesi>
```

Il builder del modello usa la classe originale del trapano di Hoven e la
geometria/scheletro necessari al suo impiego come arma. L'output predefinito è
una classe visuale aggiuntiva 2017; `--target-class 180` identifica invece il
vecchio workflow di sostituzione e non è la configurazione 0.2.

Corpo e impugnatura sono separati dalla spirale rotante. La cartuccia è generata
dalla ricetta originale `assets/ammunition/recipe.json`: una singola cella
industriale, non un gruppo di trivelle. Il suo modello e la texture sono stati
creati con assistenza AI. Pickup e venditore usano i rispettivi alias visuali,
mentre raccolta, quantità e prezzo passano dalle operazioni native.

Le viste estese collegano gli asset immutati; non modificarli attraverso questi
collegamenti. I manifest base devono restare identici prima e dopo la build.
Nel workspace le fixture 0.2 usano `work/revolverator-model-additive-01` e
`work/revolverator-levels-additive-01/levels`. Questi percorsi sono output locali,
non dipendenze da includere nel repository pubblico.

L'upscale privato del simbolo nativo usa Real-ESRGAN. Per includere l'header
locale configurare `REVOLVERATOR_HD_ICON_DIR`; senza di esso rimane il fallback
nativo. Il clone 128×128 deve mantenere il footprint HUD 32×32. Vedi
[contratto dell'icona](HD_ICON_INTEGRATION.md).

## Preview portatile senza client

`package_private_bundle.py` richiede `--compat-plugin` e i corrispondenti
`--compat-source`; contiene plugin, sorgenti, script e asset companion.
`--runtime` verifica in sola lettura un client ufficiale installato, ma non
archivia mai l'EXE. `--bridge-patch`, se usato, è solo riferimento storico.
Per XP indicare `--weapon-xp` e `--weapon-xp-source` corrispondenti.

`Avvia-Prova.cmd` usa una prova Novalis vuota e isolata con 20 cariche.
L'avvio normale mantiene `.profile`; `-StartUnlocked` è opzionale, mentre il
setting `start_unlocked` rimane disattivato per default. `-PrepareOnly` prepara
manifest e percorsi senza avviare il gioco. Hash, sorgenti e ricevuta nativa
devono riferirsi esattamente ai plugin distribuiti; la preparazione non è una
prova nel gioco e la preview rimane distinta dalla versione definitiva.
