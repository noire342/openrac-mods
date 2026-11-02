# Asteroidi originali, Space Voyages 0.5.0

I campi usano i modelli nativi di Gemlik: classe 212 (3.584 triangoli) e
classe 1412 (522 triangoli). Sono due varianti stabili, assegnate alla
generazione; non vengono scambiate in base alla distanza. Geometria,
materiali e sette texture originali sono conservati. Un atlas condiviso
mantiene il wrapping dei texel e consente un draw per variante e campo.
Le risorse PAL e NTSC estratte e normalizzate coincidono; la provenienza
dei dischi rimane separata. Vedere anche [ASSETS.md](ASSETS.md).

## Distribuzione e contatti

Le posizioni degli otto campi sono progettate dalla mod lungo rotte diverse
e a quote diverse. Ogni campo contiene 22 rocce, per un totale invariato di
176. Il volume complessivo è 2,184 volte quello dei tre campi precedenti:
la densità media scende di circa il 54%. Le posizioni dipendono da un
generatore e da seed espliciti, indipendenti dagli
sblocchi, dal salvataggio, dalla regione e dalla frequenza di rendering.
Le rotte passano vicino ai campi, ciascuno con un varco centrale libero.
L'involucro di esclusione intorno ai pianeti protegge i decolli e tutti
gli avvicinamenti radiali, prima di qualunque selezione della destinazione.

Le collisioni usano sfere conservative che contengono la geometria ruotata.
L'involucro della nave di 1,50 unità comprende i tre scafi originali misurati.
Il controllo continuo del segmento impedisce di attraversare una roccia
con il turbo o durante il recupero di più tick. Nei contatti si conserva
lo spostamento tangenziale e si frena: yaw, pitch, roll, salute, inventario
e munizioni non vengono comandati dal resolver. Il pilota continua a usare
le equazioni originali di Gemlik. Le riparazioni di posizioni esterne non
valide sono un percorso eccezionale distinto dalla collisione ordinaria.

La camera conserva il comportamento nativo nello spazio libero. Vicino
a una roccia viene protetto anche il volume dell'occhio; il ripristino
avviene durante i tick della simulazione. La pausa ferma sia gli oggetti
sia il loro tempo di animazione. Il breve avvicinamento animato continua
invece a far avanzare quel tempo, anche senza nuovi tick del caccia.

## Rendering e manutenzione

L'estensione usa le funzioni di instancing già esposte dal bridge SDK 1.12:
non richiede ulteriori modifiche al runtime. Buffer di geometria, istanze
e texture vengono creati una volta durante l'ingresso nero e rilasciati
alla chiusura della scena. La rotazione avviene nello shader; il disegno
della mod non crea oggetti né aggiorna i buffer ogni frame. Sedici batch sono
sufficienti per gli otto campi e le due varianti; il frustum culling è
conservativo e non applica soglie di distanza che facciano sparire le rocce.
Il backend Vulkan mantiene il proprio workaround di copia nel frame ring
per i buffer di istanze: 8.448 byte per presentazione con tutte le 176.
Non viene dichiarato un caricamento GPU nullo.

Una texture RGBA32F contiene stato vivo e istante di distruzione delle rocce.
Viene aggiornata soltanto quando cambia la revisione del combattimento;
la geometria e gli indici delle istanze restano statici. Lo shader confronta
il tempo interpolato con il momento esatto dell'impatto, evitando che una
roccia sparisca prima che il colpo la raggiunga. Contatti della nave, camera
e colpi ignorano immediatamente le rocce distrutte. Lo stato persiste dopo
la distruzione della nave e viene ripristinato soltanto al nuovo viaggio.
Due frammenti visivi del modello originale 1412 accompagnano la rottura;
non introducono collisioni, ricompense o un nuovo formato di salvataggio.

`asteroid_field`, `asteroid_contacts` e `asteroid_renderer` hanno contratti
separati e test di geometria, simulazione e ciclo di vita. Il servizio
pubblico Space Voyages v1 rimane compatibile. Le posizioni non vengono
salvate: nessun nuovo formato o progresso di campagna viene introdotto.

## Risorse e prove

`tools/asteroid_assets.py` aggiunge le due varianti a un nuovo pack locale,
conservando i payload della galassia precedente. Non sovrascrive i pack
0.3.3. Il repository non contiene modelli o texture commerciali estratti.

Le prove host sono distinte dal gioco: il renderer viene verificato anche
con una facciata GL registrata, ma quel test non dimostra compilazione GLSL
o correttezza del driver. Le catture e le misure native, con gli hash
effettivamente eseguiti, sono registrate in [VALIDATION.md](VALIDATION.md).
