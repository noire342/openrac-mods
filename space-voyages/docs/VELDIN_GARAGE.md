# Veldin Casa/Garage e Space Voyages 0.8

La 0.8 collega il viaggio libero al provider PAL di Veldin Casa. Il codice
separa osservazione della flotta, politica delle rotte e caricamento dell'host.
La modalità campaign del provider e i gate privati consentono le prove
congiunte; il giro completo con Original **non è ancora certificato**.
Le prove parziali e quelle fallite sono elencate sotto. Nessuna di queste
istruzioni descrive una sostituzione dell'installazione abituale dell'utente.

## Requisiti e identità

I voli normali di campagna mantengono il requisito minimo del bridge scene/audio
**SDK 1.13**. I sorgenti 0.8 si compilano con gli header 1.14, ma verificano
dimensione della tabella, versione e callback prima di usare la parte nuova.
La rotta opzionale Casa richiede **SDK 1.14** e il provider Windows PAL
compatibile già caricato; la sola API osservativa del garage non concede
il diritto d'ingresso. Il supporto alle destinazioni aggiunte non è implicito
nel runtime ufficiale OpenRAC 0.10.0.
L'header osservativo del provider Garage v1, con snapshot da 176 byte, rimane
invariato. È un contratto distinto dall'API pubblica Space Voyages v1.

| Identità | Programma nativo | Risoluzione |
| --- | --- | --- |
| Veldin finale della campagna | 18 | Lookup di campagna del registro host |
| Veldin Casa/Garage | 18 | Destinazione `ADDED`, manifest ID esatto `veldin-casa` |
| Veldin iniziale/tutorial | 0 | Gestione narrativa originale, esclusa dalle rotte Casa |

Casa e finale non sono due righe intercambiabili del menu. Un handle è opaco
e vale con la propria generazione di registro; non è un numero di livello,
una posizione nel menu o un'autorizzazione ricavata dall'overlay caricato.
La variante interna è `DestinationRoute{native_level, variant}`, con
`Campaign` o `VeldinGarage`. Il provider si identifica come
`noire342.veldin-home`, nella regione e generazione verificate.

## Scelta di Veldin e progressione

Il corpo celeste Veldin rimane uno, nella posizione fissa del catalogo.
Prima della coordinata finale 18, Casa è selezionabile soltanto quando:

- il provider PAL ha fornito uno snapshot `READY` valido;
- versione, mondo, regione, generazione e revisione dello snapshot sono coerenti;
- il registro SDK 1.14 risolve `veldin-casa` come destinazione aggiunta sul
  programma 18, con record e handle validi.

In assenza di questi requisiti non viene creato uno sblocco sostitutivo.
La mod non imposta la coordinata 18 e non entra nel tutorial 0. Dopo il vero
sblocco 18, il marker conduce al finale della campagna. Rimane valida anche
la regola generale di ritorno alla propria origine di campagna: riconoscerla
non assegna la sua coordinata.

`READY` descrive una flotta verificata; non significa da solo che il mondo
attualmente caricato sia Casa. Per una partenza da Casa servono anche
`ACTIVE` e l'identità logica corrente confermata dall'host. Slot di attori,
classe visibile e programma 18 non vengono usati come identità persistenti.
Se il provider non è idoneo, cambia generazione o contraddice la nave fisica,
la mod conserva il viaggio nativo invece di inventare uno stato compatibile.
Al primo callback d'imbarco `READY` può essere ancora pending: la Session
attende durante la sequenza e ripete la verifica al vero handoff, prima di
`begin_departure` e del bypass del menu. Origine logica, generazioni, identità
della nave e profilo visuale devono ancora coincidere. Una classe 531 osservata
non viene mai interpretata come proprietà, selezione Green o consenso Original.

## Navi e responsabilità

Il volo libero conserva le tre navi canoniche e implementa Original nel pack
`flightfx-07-final`. La nave fisicamente imbarcata
deve essere coerente con la selezione verificata del garage:

| Identità stabile | Modello Space | Slot | Attore nel mondo | Controller nativo | Armi |
| --- | --- | --- | --- | --- | --- |
| Original, 0 | `ship_original`, fonte 2047/530 | 3 | 2047 | 531, variante 0 | Assenti |
| Green, 1 | `ship_531` | 0 | 531 | 531, variante 0 | Assenti |
| Red, 2 | `ship_532` | 1 | 532 | 532, variante 1 | Laser della mod |
| Purple, 3 | `ship_533` | 2 | 533 | 533, variante 2 | Laser della mod |

Il modello Original mantiene il proprio rig a 31 joint e non viene identificato
con il controller temporaneo 531. Fuori dallo scope delle chiamate native,
`SharedShip` può puntare all'attore 2047. Il controllo di imbarco accetta tale
attore soltanto dopo una selezione Original `READY` verificata e la validazione
del profilo completo; la classe 2047 da sola non assegna l'identità Original.
La posa cruise riproduce il riferimento seq00
del provider con cupola chiusa e carrello ripiegato. La scala uniforme conserva
le proporzioni e porta il raggio a 1,45, entro gli attuali volumi di collisione
e camera. Due ugelli derivano dagli anelli reali 15/24; il vetro usa la mappatura
SphereUV del renderer nativo. [ASSETS.md](ASSETS.md) descrive fonti e formato.

Il profilo opzionale v2 `original-ship.tsv` deve superare la verifica di scafo,
posa, materiali, bounds, due mount e dei due attori del cockpit. La Session
richiede **`available && cockpit_available`** prima del bypass. Il profilo v1
del pack 06 rimane leggibile come scafo, ma non dichiara un cockpit completo.
I pack precedenti conservano il volo delle tre navi stock; dati presenti ma
malformati vengono rifiutati. Original non viene sostituita da Green.

Original rimane disarmata, senza mirino o suoni laser. Il suo profilo audio
condivide i clip motore/turbo del caccia Gemlik già presenti nel pack 05,
come adattamento dichiarato: nessun nuovo PCM o variazione dei volumi.
Il GLB 2047 contiene lo scafo; il pack 07-final compone separatamente Ratchet 0
e Clank 10. Le matrici arrivano dalla cattura nativa verificata del tick 464,
prima del cleanup, già con scale, basi e modificatori dei sedili applicati.
Il bake applica una sola volta l'inverse bind dei rispettivi GLB. Non viene
sostituita la posa reale con la sola chiave authored 40, che per Ratchet non è
equivalente a causa dello stato effettivo dell'animazione.

Il fit geometrico della canopy traduce entrambi gli attori di +0,055X nelle
unità Space, senza cambiare pose o proporzioni. È un adattamento dichiarato,
non un attachment nativo 530 estratto. I corpi superiori conservano almeno
0,004 unità di margine rispetto ai piani di supporto della cupola. Il pass
chrome di Clank è distinto dal vetro: 445/445 posizioni uniche coincidono con
la sua superficie opaca. La prova GPU congiunta rimane distinta dalla prova
di correttezza del bake.

`GarageProvider` trova il provider già caricato e ne mantiene valida la
libreria durante l'osservazione, senza creare una seconda istanza o avviare
un altro profilo. `GarageBridge` valida copie dello stato; non seleziona
navi, non scrive attori e non monta destinazioni. `DestinationGateway` gestisce
identità registrate, contesto, ticket e token SDK 1.14 senza conoscere Veldin.
`garage_destination_adapter` contiene la policy specifica; `DestinationClient`
è la facciata per il catalogo attuale. La distinzione e i requisiti per nuovi
pianeti sono documentati in [CUSTOM_DESTINATIONS.md](CUSTOM_DESTINATIONS.md).

Il client acquisisce un ticket prima della partenza nativa, catturando
origine, generazione nativa, generazione del registro e nonce diverso da zero.
Durante il volo prepara la meta nello stesso handoff e con lo stesso nonce.
Un avvicinamento annullato rilascia soltanto il token preparato, conservando
la partenza per una nuova scelta. Il commit consuma il token una volta,
dopo la chiusura della scena; l'host seleziona il mount e la continuazione
originale avvia il proprio caricamento. Il client non richiama un loader o
un secondo decollo. Il contratto di rollback preserva l'origine logica,
compreso il Veldin notturno, ed è coperto dai test CPU. La cancellazione
nativa da Casa richiede la propria ricevuta: il solo programma 18 non ne
dimostra l'identità.

## API pubblica e compatibilità

L'[API Space Voyages v1](API.md) resta osservativa, con struttura ed eventi
e header invariati. La nuova identità nave è interna e non aggiunge campi
all'API pubblica. I suoi ID nativi 18 non distinguono Casa dal finale: non espone
la variante interna, il manifest ID, un handle host o comandi di viaggio.
Le altre mod non devono interpretare `origin == 18` come prova di `ACTIVE`
del garage, né usare `candidate_unlocked` per assegnare progressi.

Il supporto ordinario SDK 1.13 prosegue senza la rotta Casa; non si accede alla
coda 1.14 di una tabella più corta. NTSC-U conserva i voli di campagna: la
disponibilità Casa attuale è limitata al provider PAL idoneo. Non vengono
modificati formato dei salvataggi, proprietà delle navi o progressione della
campagna per compensare un provider assente.

## Prove e limiti attuali

Il build del controllo attore del mondo supera **39/39 suite CPU**, senza warning
di compilazione. Log e JUnit sono rispettivamente
`work/space-original-crew-audit-081/final-world-actor-ctest.log` e
`work/build-space-voyages-081-final/final-world-actor-081.xml`.
Le precedenti ricevute 36/36 e 37/37 conservano i propri moduli e scenari.

Le prove rimangono separate per ciò che osservano:

| Prova | Risultato e limite |
| --- | --- |
| Suite CPU integrate | 39/39; identità, gateway, Session, risorse e renderer con backend simulato |
| Gateway generico | 257 identità custom, comprese più destinazioni sullo stesso template; non è scoperta automatica dei pianeti |
| Importer cockpit e reader | 9 test Python e 2 suite C++; pose, ruoli, bounds e chrome verificati, senza avvio del gioco |
| Cattura nativa Original, tick 464 | Attori cinematici vivi 0/10 e modello 2047 con 31 joint; non è un viaggio Original nello spazio |
| Stock Batalia → spazio → Rilgar | PAL e NTSC-U completati con modulo `b8b54d98…` e pack 06, senza provider Garage |
| Prima prova Purple con Garage | Tre viaggi registrati in `stock-purple-roundtrip-02`, con ritorno logico a `veldin-casa`; sky bianco e progressione finale non osservata |
| Purple con Garage, nuova prova `p4` | Tre scene e arrivi 18 → 2 → 18, ultimo mondo logico `veldin-casa`; sky corretto nelle catture esaminate, exit 0 e 488 input immutati. La coordinata finale 18 non è osservata a fine prova |
| Tentativo Original `o2` | Il controllo attore/controller rifiuta il volo Original e conserva il viaggio nativo; exit 0 non rende positiva la prova |
| Tentativo Original `o3` | Crash registrato nel companion, exit 3; prova fallita |
| Tentativo `g4` | Timeout del secondo imbarco del helper QA; nessuna prova completa del giro richiesto |

I log delle prove native parziali sono in
`work/space-garage-joint-qa-081/runs/stock-purple-roundtrip-02` e
`work/sj/r/o2`, `o3`, `g4`. La nuova prova Purple è in `work/sj/r/p4`,
con modulo `cd86a1b2…`, provider `9bd5bbfd…`, pack `07-final` e budget
di 18.000 frame in 16:9. Il fix successivo distingue attore 2047 e
controller 531 nei test CPU; non viene presentato qui come successo nativo
Original senza una nuova prova positiva. Restano da chiudere il giro completo
Original, la cancellazione della lease da Casa, le transizioni delle altre
navi e la verifica finale dei progressi. Il giro Purple non sostituisce
queste prove.

Un'osservazione successiva (`g5`) vede il flag grezzo 18 temporaneamente a 1
dentro Casa e di nuovo a 0 tornando al pianeta 2 di campagna. Questo non prova
una modifica persistente del salvataggio; evidenzia però che la selezione della
rotta finale deve usare l'autorità di progressione di campagna, senza dedurla
dal solo flag grezzo mentre è montata Casa. La correzione e la relativa prova
di instradamento restano aperte.

Il pacchetto cockpit e la sua provenienza sono verificati in
`work/space-original-crew-audit-081/final-verification.json`. La cattura nativa
è `work/veldin-ship/qa-menu-cockpit-11402-41/cockpit-source.json`; il confronto
indipendente del bake è in
`work/space-original-occupants-qa-081/pack07-native-independent-geometry.json`.
Il modello di campagna e la cattura non sostituiscono la prova visuale del
rendering Original nella scena transitoria.

Le ricevute precedenti di campagna sono
`work/space-destination-native-audit-080/receipt.json` e
`work/space-original-integration-audit-080/integrated-receipt.json`.
NTSC-U conserva i due warning di alias già presenti nella baseline 0.6;
non sono stati omessi dal confronto.

Un confronto PAL separato ha riprodotto il prompt nero con l'equazione di
blend sottrattiva ereditata. Ripristinare `GL_FUNC_ADD` rende leggibili i glifi
nello stesso fotogramma, con identici input e progressi. La ricevuta è
`work/space-prompt-blend-audit-080/receipt.json`. La condizione riprodotta
interessa anche altri elementi trasparenti; lo stato grafico del precedente
episodio dell'utente non era stato catturato, quindi non attribuisce a questa
sola causa ogni segnalazione di messaggio assente.

L'audio del pack privato `flightfx-07-final` è byte identico a `flightfx-05`, con
gain 1 per tutti i 13 ruoli. Original condivide i clip esistenti senza
aggiungere un quattordicesimo ruolo o un cue di respawn.
L'attenuazione musicale 0,4 di 04 è stata rimossa senza modificare PCM, loop,
mesh o effetti. Nella prova indipendente **0.7.0**, DLL
`0ab2410c473b2a0bcba4505a5bce88690771ee859f4454d1cfc61264422bfc56`,
PAL e runtime SDK 1.13, il gioco ha completato il budget di 4.800 frame con pulizia
della scena e nessun errore. Una baseline WASAPI di 3 s era silenziosa; la cattura
successiva di 10 s ha registrato 479.927/480.000 frame non nulli, picco 0,376068 e
RMS 0,079802, senza saturazione, valori non finiti o variazione dell'endpoint
Realtek al 45%. È evidenza dell'uscita digitale, non una prova acustica degli
altoparlanti o della preferenza soggettiva del mix.

La ricevuta è
`work/qa-space-pal-audio070-pal-neutral-device-01/audio-device-verification.json`.
Il completamento del budget è attestato dal log nativo e dalla pulizia;
l'exit code OS del processo gioco non è stato raccolto separatamente.
Questa prova non carica il garage reale e non certifica la 0.8. Le prove 0.6
conservano i loro hash e nomi originali: non vengono rinominate come 0.8.
I precedenti scenari audio e i loro limiti rimangono documentati in
[AUDIO.md](AUDIO.md); [VALIDATION.md](VALIDATION.md) mantiene la distinzione
fra build e percorsi di verifica.
