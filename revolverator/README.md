# Revolverator

Ricostruzione della meccanica del Revolverator tagliato da Ratchet & Clank:
colpo ravvicinato, presa di un nemico, sollevamento sulla trivella, rotazione e
lancio. Usa la geometria e la texture del trapano già presente a Hoven.

**Build di sviluppo per R&C1 NTSC-U su OpenRAC**, verificata nel gioco per presa,
rotore, lancio, munizioni, venditore e convivenza con Weapon XP. I dieci gruppi
CTest passano; lo smoke di caricamento comprende i livelli 0–18. Questo non
equivale a una partita completa o alla verifica di ogni boss. PAL non è
dichiarata compatibile. Vedi [esiti e limiti](docs/NATIVE_VALIDATION.md).

La pubblicazione corrente comprende **solo sorgenti e strumenti**: nessuna DLL,
mesh o texture originale e nessuna icona HD derivata dal gioco. Per compilare,
vedi [Build dalla radice della repository](../docs/BUILDING.md); gli asset vengono
generati localmente dalla propria copia NTSC-U. La ricetta originale della
cartuccia è stata generata con assistenza AI, come indicato nei [crediti](../CREDITS.md).

## Integrazione nel gioco

La mod **sostituisce il Walloper nello slot 18**: riutilizza possesso, acquisto,
selezione rapida e salvataggio nativi. Non aggiunge un nuovo slot inventario.
Un salvataggio che possiede già il Walloper conserva quel possesso, ora mostrato
come Revolverator; i suoi contatori non vengono azzerati o riempiti a ogni livello.

Il bilanciamento attualmente impostato nel codice è:

| Voce | Valore base |
|---|---:|
| Disponibilità in una nuova partita | Dalla prima visita a Hoven |
| Prezzo standard dell'arma | 15.000 bolt |
| Capacità | 10 cariche |
| Consumo | 1 carica per attivazione |
| Raccolta da un pickup | 2 cariche, fino alla capacità |
| Prezzo di una carica dal venditore | 25 bolt |

Il pickup rappresenta **una singola cartuccia energetica industriale**: corpo
rosso/bruno, terminali in ottone, nervature metalliche e indicatore ambra.
Non contiene punte di trivella di ricambio.

## Controlli e presentazione

- Premi brevemente **Cerchio** per il colpo ravvicinato.
- Tieni **Cerchio** vicino a un nemico ammesso per prenderlo; rilascia per lanciarlo.
- La presa conserva il punto colpito, invece di collocare sempre il nemico in piedi.
- Il corpo dell'arma resta fermo: ruota solo la punta originale del trapano.
- Con munizioni la punta gira lentamente a riposo; durante la presa segue la
  stessa rotazione del nemico. A vuoto dà un breve scatto e torna allo stesso angolo.
- L'HUD usa il riquadro originale, il contatore nativo e il simbolo scelto
  `59901:13`. La barra di carica compare nel riquadro esistente durante la presa.
- Gli archi elettrici del Walloper sono esclusi dal Revolverator.

Le prove riproducibili e i relativi limiti sono riportati negli
[esiti nativi](docs/NATIVE_VALIDATION.md). La
[lista di regressione](docs/QA_CHECKLIST.md) rimane disponibile per le verifiche
successive, comprese le situazioni della campagna non coperte dallo smoke.

La memoria per braccio e rotore viene riservata prima delle risorse facoltative.
Quando lo spazio del livello non basta, l'arma continua a funzionare: a Rilgar
rimane l'icona originale 32×32; i testi aggiunti per i pickup possono restare
quelli nativi. L'upscale 128×128 non deve compromettere il combattimento.

## Componenti della build privata

Servono tre componenti compatibili tra loro:

1. Il plugin nativo `revolverator` costruito con l'SDK OpenRAC.
2. Il pack `revolverator-model-02`: corpo sul giunto 0, sola punta sul giunto 4.
3. L'estensione locale `revolverator-ammo-levels-03/levels`, che aggiunge le
   rappresentazioni della cartuccia per pickup e venditore.

Il plugin richiede l'ABI dichiarata nel suo manifest e gli hook elencati in
`openrac-mod.json`. Si compila con l'SDK 1.3; non richiede una ricompilazione
dell'eseguibile OpenRAC. Il modello del trapano e l'eventuale icona HD derivano
dalla copia locale del gioco e restano nel flusso di build privato.

La consegna privata può essere un unico ZIP da estrarre e avviare con
`Start-Revolverator.ps1`. Il suo involucro **non è un nuovo formato riconosciuto
dall'importatore standard del launcher**: lo script usa il runtime installato,
prepara manifest estesi e registri del plugin in una sessione separata, e non
sovrascrive dati o salvataggi installati. Non assegna automaticamente l'arma.

## Sviluppo

- [Build, asset e consegna privata](docs/BUILD_AND_ASSETS.md)
- [Criteri di verifica prima della consegna](docs/QA_CHECKLIST.md)
- [API per altre mod](docs/API.md)
- [Pacchetto sorgente e limiti di pubblicazione](docs/PACKAGING.md)
- [Integrazione opzionale dell'icona HD](docs/HD_ICON_INTEGRATION.md)

Il controller e i calcoli di presa/posa sono separati dagli adattatori che leggono
la memoria del gioco. Munizioni, venditore, alias di classe, rotore e HUD hanno
responsabilità distinte e ripristinano soltanto le modifiche ancora possedute.
L'API pubblica scambia valori copiati sul thread del gioco, senza esporre puntatori
alla memoria ospite.
