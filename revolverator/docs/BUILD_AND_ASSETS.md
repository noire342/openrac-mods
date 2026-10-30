# Build e asset della consegna privata

## Requisiti e percorsi

Gli esempi PowerShell partono dalla radice di questa repository. Sostituire i percorsi
dei tool se si usa un'altra installazione. I builder leggono gli asset estratti di
**R&C1 NTSC-U, formato livelli 3**; non leggono R&C2 e non modificano la ISO.

```powershell
$Mod = Join-Path $PWD 'revolverator'
$Work = Split-Path -Parent $PWD
$Sdk = Join-Path $PWD '.sdk/openrac-mod-sdk-1.3.0'
$Python = 'python'
$Editor = Join-Path $env:LOCALAPPDATA 'OpenRAC Launcher/openrac/editor'
$Levels = Join-Path $env:USERPROFILE 'OpenRAC/active/rac1/data/port/ntsc'
$CMake = 'cmake'
$CTest = 'ctest'
$Build = Join-Path $Work 'build-revolverator'
```

Servono Python 3.10+ e Pillow per gli strumenti grafici. La compilazione C++
richiede un toolchain C++17 e CMake; usare il generatore Visual Studio installato
o il generatore predefinito della propria piattaforma.

## Plugin e controlli automatici

```powershell
& $CMake -S $Mod -B $Build `
  "-DOpenRACMod_DIR=$Sdk/cmake" -DBUILD_TESTING=ON
& $CMake --build $Build --config Release
& $CTest --test-dir $Build -C Release --output-on-failure
```

Il package nativo generato è `build-revolverator/package/revolverator`. Il
manifest deve contenere la DLL della piattaforma sotto `plugin`. La suite
CTest copre controller, posa/presa, munizioni, venditore, rotore, arena ospite,
localizzazione, adattatore nativo del rotore, alias di classe e icona HD.
I test non avviano il gioco.

L'icona HD è opzionale. Per una build privata, generare l'header con
`build_hd_icon.py` e `encode_hd_icon.py`, poi configurare
`-DREVOLVERATOR_HD_ICON_DIR=<cartella-con-revolverator_hd_icon_data.hpp>`.
Senza header resta disponibile il simbolo nativo 32×32. Estrattore, formato CSM1
e footprint logico sono descritti in [HD_ICON_INTEGRATION.md](HD_ICON_INTEGRATION.md).

## Trapano originale con sola punta rotante

Usare una cartella di output nuova: il builder rifiuta di sovrascrivere una build
precedente o una cartella attualmente usata da una sessione.

```powershell
& $Python "$Mod/tools/build_model_pack.py" --levels $Levels `
  --out "$Work/revolverator-model-rebuild-01" --editor $Editor
& $Python "$Mod/tests/model_rotor_test.py" --levels $Levels `
  --pack "$Work/revolverator-model-rebuild-01" --editor $Editor
& $Python "$Editor/overrides.py" check "$Work/revolverator-model-rebuild-01"
```

Questa è la ricetta di `revolverator-model-02`. Prende classe 164 da Hoven e ne
mantiene geometria, topologia, normali, UV e texture originali. Applica soltanto
la conversione di scala necessaria per lo slot del Walloper:

| Parte | Vertici | Triangoli | Giunto del modello risultante |
|---|---:|---:|---:|
| Corpo, impugnatura e motore | 1.338 | 1.338 | 0 |
| Punta a spirale, componente originale 35 | 208 | 202 | 4 |

Non esistono triangoli con vertici divisi fra corpo e punta. Il file contiene una
sola istanza della mesh e conserva tutte le cinque matrici inverse di bind del
Walloper richieste dall'importatore. `NativeRotor` compensa la matrice nativa del
giunto 4 e usa il vero asse della punta; non riscrive lo scheletro condiviso.
La geometria sorgente locale verificata ha SHA-256
`947e9ccc5611fcd2bd5f3c31769ece6495a1608a0a5a919d8691b46fb383ac32`.
`rotor.json` registra questi dati per ciascuno dei 19 livelli.

Questo pack è un normale pack di sostituzione formato 1 e passa i controlli di
compatibilità dello scheletro dello stacker. Le immagini e la geometria derivate
dal gioco sono asset privati, esclusi dal pacchetto sorgente pubblico.

## Cartuccia energetica e alias del venditore

```powershell
& $Python "$Mod/tools/build_ammunition.py" --levels $Levels `
  --out "$Work/revolverator-ammo-rebuild-01"
& $Python "$Mod/tests/ammunition_model_test.py" `
  --overlay "$Work/revolverator-ammo-rebuild-01"
```

La ricetta predefinita è `assets/ammunition/recipe.json`: genera una sola
cartuccia energetica, 612 triangoli, texture 128×128 e altezza nativa 0,389 m.
È la ricetta dell'output `revolverator-ammo-levels-03`. Il builder:

1. Clona i manifest dei 19 livelli di gioco e dei due livelli menu 98/99.
2. Collega i grandi asset immutati con junction e hardlink; non li riscrive.
3. Aggiunge classe visuale 32000 per i pickup e alias 2016 per il venditore.
4. Fa puntare entrambe le classi alla stessa mesh e alla stessa scala della
   classe nativa 225, attualmente `0.027870021760463715`.
5. Confronta gli hash dei manifest originali prima e dopo la generazione.

`NativeAmmoAlias` rende disponibile l'alias 2016 nello slot residente della
classe 225. Il venditore può così istanziare il proprio modello senza dipendere
da un hook di rendering assente nel runtime installato. La logica di raccolta,
quantità, salvataggio e acquisto resta quella nativa, associata allo slot 18.

L'output contiene collegamenti a dati installati: è una vista per esecuzione e
QA, non un progetto su cui modificare gli asset collegati. `art/model.glb` è una
versione in metri per DCC; i modelli nei livelli sono convertiti nello spazio
nativo 225. `preview_ammunition.py` renderizza il GLB effettivo con Blender.

## Un unico ZIP privato, senza modificare l'engine

L'importatore standard attuale separa plugin formato 2 e sostituzioni formato 1.
Il packager SDK include manifest, librerie e documenti, non un'estensione dei
manifest dei livelli. Lo stacker dei modelli salta classi assenti nei manifest
base. Pertanto non si deve presentare il seguente involucro come un nuovo formato
importabile dal launcher con un solo trascinamento.

Lo strumento privato raccoglie i due package standard e la sola nuova cartuccia:

```powershell
& $Python "$Mod/tools/package_private_bundle.py" `
  --plugin "$Build/package/revolverator" `
  --models "$Work/revolverator-model-02" `
  --ammunition-overlay "$Work/revolverator-ammo-levels-03" `
  --out "$Work/deliveries/revolverator-0.1.0-private.zip"
```

Lo ZIP contiene `Start-Revolverator.ps1`, il plugin, il pack del trapano, il GLB e
PNG della cartuccia, documenti e un inventario SHA-256. Non contiene la ISO, i
salvataggi, RAM o manifest originali dei livelli. Contiene comunque il trapano e
la texture derivati dalla copia locale: **non è un artefatto approvato per la
pubblicazione**. Lo script di packaging non installa, non avvia e non carica nulla
su servizi esterni.

Dopo l'estrazione, verificare prima la sola preparazione:

```powershell
& './Start-Revolverator.ps1' -PrepareOnly -SessionRoot 'C:/percorso/sessione-nuova'
```

PowerShell 5.1+ genera localmente l'overlay dei manifest e un registro plugin
separato, copiando solo gli asset propri della cartuccia. Non richiede Python
sul PC che esegue il bundle. I percorsi predefiniti sono quelli dell'installazione
locale del launcher; `-Runtime`, `-InstalledData` e `-Settings` li sostituiscono.
`session.json` registra argomenti e controlli effettuati con `started:false`.

Senza `-PrepareOnly` lo script apre il runtime installato con `--levels` e
`--mods`. Usa cards e dati di sessione separati, `--no-card`, impostazioni copiate
e nessuna altra mod installata. Non assegna automaticamente l'arma e non copia
progressi di campagna. Il parametro facoltativo `-Level 1` seleziona Novalis per
una prova di sviluppo; l'acquisto normale resta vincolato a Hoven.

Il bundle privato non cambia il formato del launcher né l'eseguibile OpenRAC.
La pubblicazione corrente comprende solo sorgenti e strumenti. Le prove native
eseguite e i limiti della preview sono in [Validation](../../docs/VALIDATION.md#revolverator-source-preview);
[QA_CHECKLIST.md](QA_CHECKLIST.md) resta la lista completa delle regressioni.

## Stato dell'audit degli asset

La ricostruzione corrente è riproducibile con i due builder: test di partizione
del rotore e test della cartuccia controllano i 19 modelli, gli alias 2016/32000 e
l'integrità dei manifest base. Gli output di audit sono separati dai pack usati
dalle sessioni attive. Questa prova di generazione non certifica animazioni,
venditore o combattimento nel runtime.
