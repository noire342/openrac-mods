# Audio della scena spaziale — 0.6

La colonna sonora e gli effetti del pack locale provengono dal combattimento
originale di Gemlik. Motore, turbo, laser, impatti e distruzioni conservano
la propria sorgente PCM. I profili delle navi 531, 532 e 533 restano distinti;
per il loro volo continuo il pack usa come fallback dichiarato il caccia
originale 69. È un adattamento alle navi cinematiche impiegate dalla mod.

La nave verde usa motore e turbo, senza suoni di sparo. I motori si fermano
quando la nave viene distrutta e ripartono al minimo dopo la ripartenza
locale. La musica mantiene la stessa voce e la stessa posizione. Il pack
originale lascia vuoto il ruolo opzionale `respawn`, senza introdurre un
jingle o un effetto aggiuntivo.

## Responsabilità e ciclo di vita

| Componente | Responsabilità |
|---|---|
| `SceneAssets::audio` | PCM S16 decodificato, frequenza, canali, gain/pitch di origine e intervallo di loop. |
| `SceneAudio` | Musica, motore e turbo della nave; consumo dei nuovi ID di sparo e impatto; danni, pausa e rilascio delle voci. |
| `SceneAudioSdk` | Traduzione dei descrittori e dei gruppi nell'ABI generica del runtime. |
| SDK 1.13 | Copia e mix dei campioni sul motore e sul dispositivo audio di OpenRAC, proprietà delle risorse e pulizia della scena. |

La Session inizializza l'audio nel primo tick della scena, quando possiede
l'acquisizione esclusiva SDK. Il controller legge lo stato della simulazione;
il disegno non avvia suoni. Gli ID di colpi ed eventi persistenti impediscono
duplicazioni durante catch-up o ridisegni. I contatori del danno separano
colpi allo scafo, distruzione e ripartenza locale.

Sono previste al massimo tre voci continue — musica, motore e turbo — e
29 effetti, per un totale di 32. I campioni hanno 14 slot semantici, dei quali
13 obbligatori nel pack originale; `respawn` è facoltativo e rimane assente.
Il tick non alloca memoria. La chiusura, anche dopo un errore, ferma le voci
e rilascia i campioni prima che termini l'acquisizione SDK.

La pausa congela i cursori di entrambi i gruppi e il raccordo del motore.
La ripresa continua dai cursori conservati. La distruzione e il respawn
rimangono dentro lo stesso viaggio: la musica continua, senza riavviare
la propria introduzione.

## Mixer e volumi del gioco

Le voci musicali usano il gruppo Musica; motori ed effetti usano il gruppo
Effetti. Il runtime applica i volumi ordinari del gioco, poi il volume globale
e l'uscita audio già selezionati in OpenRAC. La mod non introduce impostazioni
di volume dedicate, un secondo dispositivo o un player del sistema operativo.

Al primo campione riprodotto, il runtime conserva i cursori dell'audio nativo
del mondo sospeso. Li ripristina al termine della scena. Non servono chiamate
a funzioni di un overlay Gemlik non caricato né scritture nella memoria del
gioco per gestire i suoni.

L'SDK accetta PCM S16 interlacciato, mono o stereo, da 8 a 96 kHz. Il loop
`[loop_begin_frame, loop_end_frame)` ha estremo finale esclusivo; un finale
zero indica la lunghezza del campione. L'introduzione precede il primo loop
e viene suonata una sola volta. Il runtime copia il PCM e gestisce ricampionamento,
pitch e pan all'uscita. I suoi limiti generici sono 16 campioni, 64 voci,
256 MiB e 600 secondi per campione; il controller della mod usa limiti più stretti.

## Interfacce e risorse

L'estensione generica **SDK 1.13** aggiunge creazione, rilascio, riproduzione,
aggiornamento, stop, pausa e lettura dello stato delle voci. Solo la mod che
possiede la scena esclusiva può usare queste risorse. Il prefisso della
tabella SDK precedente rimane invariato; le vecchie build e le loro ricevute
rimangono distinte dalla 0.6.

L'**API pubblica Space Voyages v1** conserva struttura, fasi, query e callback
descritte in [API.md](API.md). Il controller audio non modifica quell'API,
i progressi o il formato dei salvataggi.

Il reader accetta `audio.tsv` versione 1 con i ruoli originali completi,
verifica metadati, dimensioni, loop, parametri e percorsi, quindi espone PCM
di proprietà di `SceneAssets`. Un pack legacy senza il manifest conserva
le capacità precedenti; dati audio presenti ma malformati vengono rifiutati.
Estrazione e provenienza delle risorse restano nella documentazione degli
[asset](ASSETS.md); audio originale e ISO non vengono inclusi nel sorgente.

## Prove host

Nella verifica host documentata della 0.6 sono passati 32 test CTest. Per
l'audio comprendono:

- `space_scene_audio`: 37 controlli del controller con backend simulato,
  inclusi loop, 15 spari/s senza duplicazioni, pausa dei cursori, respawn,
  capacità fisse, errori e 10.000 tick senza allocazioni.
- `space_audio_assets`: 118 controlli del reader, con 56 cataloghi malformati
  rifiutati; PCM firmato, stereo, loop, parametri, dimensioni e percorsi.
- `space_session_audio`: 233 controlli sulla Session e sull'adattatore SDK di
  produzione, con handle simulati, tick senza draw, pausa, assenza di volumi
  aggiuntivi e pulizia dopo errori. Il caso render usa un errore GL controllato.
- `space_scene_audio_sdk`: conformità dell'adattatore ai descrittori, gruppi,
  flag, handle e risultati dell'ABI SDK 1.13.

L'audit del runtime SDK 1.13 ha inoltre superato otto test host. Verifica
campioni realmente miscelati, loop, ricampionamento, pitch, pan, gruppi di
volume, pausa, proprietà del PCM, handle obsoleti, limiti, assenza di
allocazioni nel mix/play e pulizia su uscita o errore. Una DLL di prova usa
le chiamate SDK attraverso il loader; sono verificati anche congelamento
e ripresa dei cursori audio nativi e compatibilità del loader con mod precedenti.

## Fixture native e uscita audio

Le seguenti prove si riferiscono alla DLL 0.6 con SHA-256
`3ab19b845106a6a77b503d377b4861d379ef73b93b3289b6b8afb5e13c7d6762`
e ai pack audio originali 04. Il manifest audio è identico nelle estrazioni
regionali; runtime, risorse grafiche e profili PAL/NTSC restano espliciti.

| Fixture | Esito osservato |
|---|---|
| PAL arancione, 6.500 frame | 1.023 cue laser, 20 impatti e 4 rocce distrutte, coerenti con il combattimento. |
| PAL viola, 6.500 frame | 1.023 cue laser, 15 impatti e 2 rocce distrutte. |
| PAL verde, urti e pausa, 11.500 frame | Nessun laser; 9 cue di urto, 3 distruzioni e 3 ripartenze. Una sola partenza musicale, con il motore riavviato dopo ogni morte e il ruolo respawn assente. |
| PAL con uscita ordinaria, 4.800 frame | Mixer attivo sul percorso di uscita del gioco; 598 cue laser, 15 impatti e 1 roccia distrutta. |

Tutte e quattro le fixture hanno caricato 13 campioni, chiuso normalmente,
eseguito la pulizia della scena e registrato zero errori e zero cue scartati.
Le prime tre usano l'audio deterministico di prova; l'ultima usa il dispositivo.

I due tour completi, uno PAL e uno NTSC-U, hanno verificato ciascuno 18 arrivi,
252 confronti dei progressi, 36 della camera e 36 della nave. Tutte le 18
scene di ogni tour hanno acquisito 13 campioni, avviato la musica una volta,
miscelato PCM non nullo e osservato la callback di chiusura. La nave verde
è rimasta integra e disarmata. Sono caricati anche Weapon XP 0.6.1 e Cinematic
Journeys 0.1.0 con minimo di volo a 5 secondi, nelle copie congelate della prova.
Il controllo avviene su progressione sintetica senza scheda e partenze
`native-prepare`; non costituisce una prova della campagna o dell'imbarco
manuale su tutti i pianeti.

Dopo i 18 arrivi verificati, il supervisore ha inviato `WM_CLOSE` soltanto al
proprio processo: entrambi sono usciti con codice 0. PAL ha registrato 309.850
frame e NTSC 224.702, entro il massimale di 500.000. Sono uscite cooperative,
non raggiungimenti del limite. I tour usano audio deterministico e volume
globale zero: verificano mix e ciclo di vita, senza dimostrare l'udibilità.
I contatori nativi non mostrano direttamente gli handle dopo la chiusura;
riacquisizioni successive, callback e test host della pulizia sono evidenze
distinte.

Una cattura WASAPI separata sull'endpoint Realtek provato ha registrato 12 s
di uscita stereo a 48 kHz: 576.000 frame, di cui 575.916 non nulli, picco
0,425812 e RMS 0,076717, senza campioni prossimi al fondo scala. La baseline
di 3 s ha registrato 144.000 frame nulli. Endpoint al 45%, non silenziato,
immutato prima/dopo; nessun microfono fisico o player esterno. È una prova
del segnale digitale sull'endpoint, non dell'ascolto fisico degli altoparlanti.

Il routing dei normali gruppi Musica/Effetti e la loro legge di volume sono
verificati nel codice e nei test del mixer. Non è stata eseguita una scansione
manuale dei cursori del menu sul dispositivo. La campagna completa, altri
endpoint, revisioni del gioco e configurazioni audio richiedono prove proprie.

Le ricevute mantengono esplicite anche le differenze di contorno: tutti i file
già presenti nelle baseline sono rimasti invariati; al bundle del runtime
sono stati aggiunti file di patch, documentazione e audit non eseguiti.
L'uguaglianza dell'intera directory è quindi falsa. La serializzazione delle
impostazioni private ha ridotto `rim_lift_max_gain` di 0,00001 per esecuzione,
con volumi e controlli di scenario invariati. Le prove non sono misure delle
prestazioni del volo. Le ricevute della 0.5 restano specifiche delle rispettive
identità binarie e scenari; [VALIDATION.md](VALIDATION.md) raccoglie i riferimenti.
