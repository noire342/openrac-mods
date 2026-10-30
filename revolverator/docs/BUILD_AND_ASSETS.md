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

## Runtime richiesto

La 0.2 usa il bridge arsenale: base OpenRAC `77a2f052` più 30 hook tipizzati.
La logica delle armi rimane nel plugin; il bridge espone confini di menu,
preview, scelta rapida, venditore, pickup e ciclo dei salvataggi. Non cambia
l'ABI generica del modding. Il runtime standard privo di questi hook viene
rifiutato con il nome dell'hook mancante.

Nel workspace di sviluppo la patch e la ricetta sono in
`work/arsenal-bridge-tools/`; la build separata è
`work/build-openrac-arsenal-bridge/`. Applicare la patch alla base corretta,
rigenerare i binding con `sdk/tools/gen_binding.py rac1`, poi compilare il target
`openrac-rac1-ntsc` secondo il toolchain OpenRAC. Non sovrascrivere l'eseguibile
ufficiale durante queste prove. Gli asset di gioco restano input locali.

Per gli XP opzionali usare il companion privato `weapon-xp 0.6.3-arsenal`, con
API v4. Non basta un provider che espone soltanto le precedenti API XP.

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

## Consegna privata

La consegna portatile 0.2 deve contenere componenti compatibili: plugin, runtime
bridge, companion XP opzionale e asset ricostruiti localmente, con hash e
argomenti della sessione. La preparazione non installa o pubblica nulla.

Il packager aggiornato `package_private_bundle.py` richiede `--runtime` e
`--bridge-patch`; per XP usare `--weapon-xp` con `--weapon-xp-source` che punta
ai sorgenti corrispondenti. Per il motore indicare `--engine-source` alla copia
OpenRAC separata. Nel repository pubblico patch e istruzioni sono in
[`docs/arsenal-bridge`](../../docs/arsenal-bridge/README.md).

`Start-Revolverator.ps1 -PrepareOnly` prepara manifest e profilo persistente
senza avviare il gioco. I controlli Windows PowerShell 5.1 conservano gli input
originali, aggiungono le tre classi e riusano `.profile`; la ricevuta di sola
preparazione non dichiara una prova nativa. La preview resta distinta dalla
versione definitiva e ogni artefatto distribuito richiede il suo hash.
