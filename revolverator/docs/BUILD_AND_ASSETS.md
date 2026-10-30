# Build e asset della 0.2

La mod richiede R&C1 NTSC-U e gli asset estratti dalla propria copia del gioco.
I builder leggono il formato livelli 3; non richiedono R&C2 e non modificano la
ISO. I sorgenti pubblicabili non includono il trapano o le texture originali.

## Plugin

Dal checkout `rac1-mods`, con CMake, un compilatore C++20 e SDK OpenRAC 1.3:

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

## Client e utility integrate

La mod usa il client ufficiale Windows x64 NTSC-U invariato (hash B7F3762A…94081).
I due helper DLL sono inclusi nella directory del plugin e inizializzati dalla
mod, senza card esterne. `NativeAssetBundle` legge 19 manifest originali e crea
copie proprie con percorsi assoluti agli asset del pacchetto. L'alias esatto
in sola lettura di KernelBase CreateFileW permette il caricamento senza cambiare
`--levels`, aggiungere `--mods` o sostituire l'eseguibile.

Il reader JSON upstream GPL rimane in `third_party/openrac` con NOTICE. Serializer,
staging, discovery e cleanup sono adapter separati, testati con root esplicite.
Gli XP opzionali richiedono il companion 0.6.3-arsenal/API v4.

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

## Pacchetto nativo del launcher

La release usa `revolverator-0.2.0-windows-x86_64.zip` e SHA256SUMS, con manifest,
DLL principale, utility private, asset companion e notices. Il root index della
repository elenca solo le due mod pubbliche. Le utility restano dipendenze
interne del pacchetto, anche se i rispettivi sorgenti/test sono separati.

I corresponding sources comprendono il reader GPL, stager, helper e dipendenze
MinHook; l'eventuale header HD usato dal binario deve essere fornito con i suoi
input corrispondenti separatamente dal normale snapshot Git privo di art del gioco.
I precedenti script di avvio e ZIP esterni sono workflow storici di sviluppo.
