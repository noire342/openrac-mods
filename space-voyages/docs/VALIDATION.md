# Verifiche di Space Voyages

## Build corrente 0.4.0: asteroidi originali

DLL eseguita nelle verifiche finali:
`80c7cb986b70d85c81083e4e59b2c76226b8944cc03e450d7b61ee5d054cad24`.
I runtime SDK 1.12 rimangono quelli della 0.3.3, commit `0f98405b`:
PAL `cf968a37ff295037b74148fa065efe3445646b7a356f631eb46573bef0ed03e2`,
NTSC-U `9af2f5c1fc0f35ae2ebdd8b5ef172370c141d5d1ba9cfb0bce7c31c71eb8f0f9`.
La probe diagnostica separata 0.1.2 ha SHA-256
`d1c8b0363c361678c3235367c286787c7710a49a6c726d16b38a2dc4281a9f80`.

- **18 suite CTest superate**, inclusi layout deterministico, query spaziali,
  contatti continui, camera, rendering instanziato, tempo/pausa e percorso
  diagnostico. I test host distinguono i contratti matematici dalle prove GPU.
- **Tour nativi completi PAL e NTSC-U:** 18 partenze e 18 ritorni a terra per
  regione, 252 confronti di progressi, 36 di camera e 36 di nave. In PAL sono
  caricati anche Weapon XP 0.6.1 e Cinematic Journeys 0.1.0. Tutti i campi
  contengono 176 asteroidi con contatti attivi. Coordinate e visite sono
  preparate in una partita sintetica; il pilota diagnostico genera input.
  Questo verifica le transizioni, non equivale a giocare tutta la campagna.
- **Tre navicelle originali:** classe 531 nei tour; 532 e 533 in due ulteriori
  imbarchi naturali PAL, con la stessa classe alla cattura e dopo l'arrivo.
  Le catture sono campioni, non una verifica di ogni frame della cupola.
- **Urti e pausa nel gioco PAL:** 6.000 frame con bersaglio diagnostico su una
  roccia, massimo 219 tick di risoluzione del contatto e nessun ingresso
  automatico in un pianeta. Sono tick di contatto, non 219 impatti distinti.
  I frame 5.430 e 5.550 durante la pausa coincidono pixel per pixel; alla
  ripresa l'immagine torna a cambiare. La correzione laterale della camera
  non viene attivata in questa traiettoria: i casi stretti sono coperti dai
  test geometrici, non da questo passaggio nel gioco.
- **Presentazione PAL a 60 Hz, 16:10:** 3.140 presentazioni dopo il tick 1.500,
  quando il pilota diagnostico restituisce il controllo agli input raw di
  sterzata, rilascio e turbo. Media 16,678 ms, p99 18,816 ms; un intervallo
  raggiunge 28,914 ms, nessuno supera 41,667 ms. Il p99 del disegno CPU è
  0,646 ms; nell'unico picco il disegno impiega 24,605 ms, quindi non viene
  attribuito soltanto all'attesa del frame. Nessuna inversione dell'orologio
  interpolato. Sono tempi di
  completamento della chiamata present, non misure di scanout del monitor
  né una valutazione soggettiva del pilotaggio. L'outlier rimane documentato.
- **Salvataggio PAL privato:** partenza di 6.000 frame, chiusura durante
  il volo libero e riapertura di 1.800 frame. L'autosave nativo scrive il
  pianeta 3 e il marcatore intenzionale di 17 bolts, recuperati realmente
  alla riapertura. Passano sette confronti di progressi e gli hash di tutti
  i 40 file della scheda reale rimangono invariati. La scheda di prova è
  creata vuota in PAL: non dimostra migrazione di un salvataggio NTSC o
  navigazione manuale della schermata Load Game.

Le fixture finali terminano al limite di frame senza errori hard registrati.
L'assenza di errori driver nei log non è una lettura dei contatori hardware
o una verifica `glGetError` di ogni chiamata. Nel tour NTSC, la chiusura del
runtime riscrive la sola copia privata delle impostazioni: normalizza gli
EOL e serializza `rim_lift_max_gain` da 7,09998 a 7,09997. La ricevuta distingue
questa eccezione dal controllo byte per byte degli altri dieci input;
nessuna impostazione installata è modificata.

Le prove finali e gli hash sono nelle ricevute del pacchetto 0.4.0. Le prove
precedenti restano qui sotto con la propria versione. L'intera matrice di
campagna, tutte le sostituzioni grafiche esterne e ogni combinazione di
messaggio/aspect ratio non sono dichiarate completate. Le risorse mancanti
mantengono il volo ordinario; i due interruttori degli asteroidi non cambiano
il formato di salvataggio o l'API pubblica v1.

## Build precedente 0.3.3 e riferimento PAL

Mod SHA-256:
`8cd2802d701480a2e480699dd0cf7fd2983454e51172b2d5102ead0fc6fbad22`.
I runtime isolati derivano da OpenRAC 0.10.0, SDK 1.12, commit `0f98405b`:

| Versione Windows | Runtime SHA-256 | Tour verificato |
|---|---|---|
| PAL v2.00 | `cf968a37ff295037b74148fa065efe3445646b7a356f631eb46573bef0ed03e2` | 18 partenze e 18 atterraggi, con Weapon XP 0.6.1 e Cinematic Journeys 0.1.0 |
| NTSC-U | `9af2f5c1fc0f35ae2ebdd8b5ef172370c141d5d1ba9cfb0bce7c31c71eb8f0f9` | 18 partenze e 18 atterraggi, senza mod opzionali |

Entrambi i tour seguono la stessa rotta completa, incluso il ritorno finale
Kalebo III → Kerwan. In ciascuno passano 252 confronti di progressi, 36 della
camera e 36 della nave residente durante il volo. La classe fisica 531
all'imbarco coincide con quella catturata e con il nuovo Moby a terra in tutte
le 18 tratte. Nessun errore; uscita normale ai limiti di 250.000 frame PAL e
120.000 NTSC. Input, DLL, runtime e manifest delle risorse mantengono gli hash
registrati prima del lancio. Cinematic usa il minimo nativo di 5 secondi.

Le ricevute sono `qa-space-pal-final-033-resolved-full-01/verification.json` e
`qa-space-ntsc-final-033-resolved-full-01/verification.json`, sotto `work/`.
Sono fixture private senza scheda, coordinate disponibili e visite marcate 2,
preparazione nativa dichiaratamente scriptata e autopilota diagnostico.
Non equivalgono alla campagna completa, a salvataggi persistenti o alle altre
due varianti di nave. Il test breve PAL `qa-space-pal-kalebo-resolved-033-01`
prova separatamente 3→16→3 anche usando l'eseguibile copiato nel bundle locale.

### Blocco PAL risolto nel generatore

Le prove precedenti raggiungevano 18 partenze ma 17 atterraggi. Il difetto
si riproduceva anche con la DLL prima dei nuovi tagli, senza mod opzionali,
sul runtime precedente e a 60 Hz reali. La traccia nativa ha individuato
la causa: il cast della funzione eseguibile che scala i frame perdeva
l'identità del simbolo. Il suo indirizzo `0x1F98C0` collide con DrawUIFrame
nell'overlay di Kalebo. La sfumatura riceveva 1.446.528 frame anziché 12;
la lettura del disco era già terminata.

Il generatore conserva ora l'identità di una chiamata nominata a funzione
presente soltanto nell'eseguibile, usando la normale traduzione diretta e il
corretto adattamento ABI. Puntatori numerici, funzioni degli overlay, hook
e casi ambigui conservano la risoluzione precedente. Non cambia la precedenza
globale del runtime e non introduce un caso speciale per Kalebo o per la mod.
Il test traduce, compila ed esegue un programma con la medesima collisione,
verificando anche che le vere chiamate all'overlay restino corrette.

Discovery hostgen: 175 test, 77 eseguiti e 98 saltati per vincoli POSIX sulla
macchina Windows, nessun fallimento. Passano inoltre 11 test CPU del runtime
e i controlli ABI generati di entrambe le regioni. Il codice generato finale
chiama direttamente lo scaler con 12 e non contiene i checkpoint temporanei
usati per la diagnosi. I runtime precedenti e la prova negativa sono conservati.

### Scatti e presentazione della build corrente

`qa-space-pal-pacing-033-resolved-60-01` misura il runtime PAL e la DLL sopra
identificati: Vulkan nascosto, 1280×800, limite esplicito a 60 Hz, audio realtime,
nessuna cattura PNG o altra QA controllata in esecuzione. La sequenza usa input
raw per curve sui due assi, turbo, rilascio dello stick e ritorno all'assetto.
Dopo 240 tick di riscaldamento esclusi:

- 3.203 presentazioni; media 16,6666 ms, p99 18,2123 ms, massimo 19,3603 ms.
- Nessun intervallo oltre 25 ms, nessun recupero multiplo o arretramento
  dell'orologio interpolato; alpha sempre fra 0 e 1.
- Zero errori e uscita normale a 5.400 frame.

Sono intervalli al completamento della presentazione host: non misurano lo
scanout del monitor né sostituiscono il gradimento soggettivo dei comandi.
La macchina e la risoluzione sono quelle della prova. I confronti numerici
della simulazione nativa e la prova di interpolazione a 144 Hz restano nelle
sezioni storiche seguenti, con i loro hash distinti.

### Raccordi, UI e stati

L'imbarco mantiene le pose e le camere originali. La console salta al frame
nativo 243; la chiusura al 464 precede il flash bianco associato al cruscotto
omesso. Non viene imposta una camera arbitraria. Sono passati 3.342 controlli
della timeline. Il runtime propaga il marcatore nativo di taglio camera per
azzerare la storia dell'interpolazione fra inquadrature distinte.

`qa-space-transitions-033-final-01` contiene 773 PNG consecutivi della stessa
mod, sul precedente runtime NTSC cinematic `d2783acc…`: frame 1900–2672.
Sono stati ispezionati singolarmente i confini 1978–2028 e 2528–2580. Il decollo
sfuma con il helper nativo prima della scena; l'uscita consegna un frame
realmente disegnato e nero. Fra i due renderer, i 18 frame 2546–2563 sono
esattamente neri. Nei confini controllati non compaiono base, console,
riempimenti grigi o camera dentro lo scafo. Nei frame 2620–2638 la cupola
rimane collegata alla nave 531. Il frame finale precede l'atterraggio:
questa cattura non rappresenta tutta la sequenza fino al controllo a terra.
`transizioni.gif` deriva solo da quelle immagini reali, con provenienza propria.

Le fixture PAL correnti `qa-space-pal-ui-033-resolved-43-01` e
`qa-space-pal-ui-033-resolved-1610-01` completano entrambe l'imbarco naturale
con Triangolo, Kerwan→Novalis, atterraggio e controllo a terra: 14 confronti
di progressi per prova, nave 531 preservata, zero errori e uscita normale.
Le immagini mostrano radar proporzionato e dentro i margini in 1024×768 e
1280×800. La prova separata `qa-space-pal-ui-033-resolved-prompt-43-01`,
senza autopilota, mostra il comando nativo di ingresso centrato; il turbo
senza Triangolo non attiva un ingresso automatico.
La fixture `qa-space-pal-ui-033-resolved-locked-1610-01` raggiunge Eudora
bloccata: mostra soltanto «Coordinate sconosciute» e resta in FreeFlight
nonostante il tentativo diagnostico di ingresso. Entrambe le prove aggiuntive
escono normalmente senza Approach o Arrival. Il calcolo con le metriche del
font reale verifica i 18 nomi e la scritta sconosciuta in entrambe le
proporzioni: tutti i 38 bounds rientrano nello schermo senza sovrapporre il
radar. Questi calcoli non vengono presentati come 38 schermate osservate.

Tutte le 11 suite CTest della mod passano. `space_session` integra 72 controlli
sulla Session di produzione con SDK e memoria CPU simulati: pausa, recupero
di più tick prima del draw finale, annullamento dopo cambi di mondo,
rientro dei callback durante la sfumatura e fallback verso l'origine.
Non inizializza GL e non sostituisce prove native. Le 2.376 porzioni dei
buffer render PAL e NTSC sono identiche; la provenienza del disco rimane distinta.

Anche il checkpoint PAL ha una prova nativa separata: una scheda vuota è
stata creata con le routine PAL originali; il salvataggio di base a Kerwan
contiene 0 bolts. Dopo una variazione diagnostica a 17, l'imbarco naturale
esegue l'autosave mantenendo Kerwan nel file. La chiusura normale avviene
durante FreeFlight a 6.000 frame. In un secondo processo, senza forzare la
destinazione, il salvataggio riapre Kerwan e ripristina effettivamente 17 bolts.
Tutti i sette confronti di progressi passano e i 40 file della scheda utente
restano identici. Non è stato usato alcun payload NTSC nella scheda PAL.
Le fixture sono `qa-space-pal-card-native-033-03` e
`qa-space-pal-card-reopen-033-03`; la probe diagnostica 0.1.2 ha identità
distinta dalla 0.1.1 dei tour. Le due fixture preliminari restano negative:
una nuova partita richiede prima l'abilitazione nativa dell'autosave e la
conferma con Croce del relativo avviso originale, ora incluse nel setup.
Questa prova non dimostra la migrazione di una campagna NTSC a PAL.

La preview locale include la probe diagnostica 0.1.2 e il suo sorgente, mentre
i tour conservano la 0.1.1 congelata. Uno smoke separato esegue la preparazione
del vero launcher PAL, adattando solo visibilità, audio e limite per l'avvio
automatico: imbarco naturale e volo manuale senza autopilota, zero errori e
uscita normale a 3.600 frame. Con la stessa probe 0.1.2 passa anche il percorso
naturale NTSC Kerwan→Novalis, con 14 confronti di progressi, nave 531 e controllo
restituito. Il cambio della probe non modifica la DLL di Space Voyages.

Restano da estendere le prove native alle altre
navicelle, alle sostituzioni grafiche, alle varianti narrative e alla matrice
completa di input. I tour sintetici non vengono presentati come copertura
della campagna o come approvazione umana dei controlli.

## Correzione scatti e riassestamento, 0.3.2

Evidenze locali Windows NTSC-U, 31 ottobre 2026. La migrazione PAL usa una
propria fixture e non eredita automaticamente gli esiti NTSC descritti qui.

### Separazione fra simulazione e presentazione

Il controller fisico conserva le equazioni native: 100.000 confronti degli
helper e 168 profili di input non rilevano differenze rispetto al riferimento.
La presentazione continua usa gli stessi coefficienti, senza la soglia finale
`limit*.01`. L'audit di 81 profili comprende i vertici reali delle tre navicelle
e una proiezione a 1.920×1.080. Nel rilascio del turbo rettilineo il passo nel
frame della vecchia soglia FOV passa da 4,28 a 0,217 pixel; il bank finale da
2,84 a 0,19 pixel. Sono passi di movimento, non una dichiarazione di errore
visivo nullo o una misura del frame rate.

Le nove suite CTest allora presenti passavano: flight, voyage, render_math,
input_adapter, presentation, disc_binding e tre cataloghi di risorse. I test di presentazione
verificano che scafo e camera usino lo stesso istante interpolato, anche con
turbo, attraversamento ±π e inclinazioni estreme. Il riferimento nativo e il
companion visivo sono verificati separatamente.

Il bridge SDK1.12 separa i tick60Hz dai frame presentati; l'estensione opzionale
è in coda e mantiene il prefisso SDK1.11. Risorse statiche e pipeline vengono
riscaldate durante l'ingresso nero, senza scrivere pixel visibili, per anticipare
il costo del primo utilizzo delle stazioni. Le risorse si rilasciano al termine
della scena. Un completamento attende conferma del draw finale: più tick di
recupero prima di un frame non possono saltare il nero consegnato al loader.

### Presentazione nativa misurata

Le due fixture condividono runtime NTSC
`e519610417698085363d85df458ef4caead016302c56464eadb0c88061409d9f`
e DLL intermedia
`751f94692c89a1dddbe01924194967ae661e084e4bb9dba2493aa233a645c82a`.
Quella DLL include le correzioni visive e il riscaldamento, prima dell'adattatore
regionale e della conferma del frame finale aggiunti alla build seguente.

- `qa-space-pacing-032-01`: sequenza raw di curve, rilasci, turbo e recupero,
  Vulkan nascosto, cap60Hz. Dopo 240 tick iniziali esclusi: 2.889 presentazioni,
  media16,667ms, p99 18,130ms, massimo20,242ms, nessun intervallo oltre25ms,
  nessun recupero multiplo né arretramento dell'orologio interpolato.
- `qa-space-pacing-032-144hz-02`: cap144Hz e VSync disattivata, durante lavori
  CPU PAL separati. La verifica riguarda l'interpolazione: 4.030 presentazioni,
  2.342 fra due tick, alpha sempre in0..1 e nessun arretramento. Non è un
  confronto prestazionale a macchina libera.

Sono intervalli misurati al ritorno della chiamata di presentazione, non una
misura fisica dello scanout del monitor né approvazione soggettiva del giocatore.
Il CSV registra ciascun present, separando draw, present, attesa e recupero;
la sola media dei callback del controller non era sufficiente. Il caricamento
iniziale delle risorse resta distinto dalla misura a regime.

### Regressione completa del catalogo

`qa-space-regression-032-full-01` usa DLL finale con adattatore PAL/NTSC e
conferma del draw:
`9aff2e0caf3263b242880aa5e655a99da31381442d4cd2dc412440037f37d114`.
Con il runtime NTSC sopra indicato ha completato18 partenze e18 atterraggi,
incluso il ritorno finale a Kerwan. Tutti i252 confronti di coordinate, visite,
bolts, munizioni, oggetti e livelli sono passati durante il volo. Passati anche
36 confronti della camera residente e36 della nave residente. La classe fisica
531 all'imbarco coincide con quella al Ground di tutti18 nuovi mondi.
Zero errori, uscita normale dopo120.000 frame.

È una fixture privata senza scheda, con coordinate abilitate e visite marcate2,
preparazione nativa esplicitamente scriptata e autopilota diagnostico. Non
costituisce prova della campagna completa o delle altre due varianti di nave.

La fixture naturale `qa-space-regression-032-ntsc-01` prova un imbarco reale
Kerwan→Orxon e il caricamento concluso conservando531. Il secondo imbarco
non avviene: la fixture non concede la maschera O2, e il ramo nativo di Orxon
con Clank avvia la cutscene9 invece della partenza ordinaria. Non viene
presentata come un ritorno completato o come un errore accertato della mod.

Per la cupola, il confronto nativo in `qa-space-glass-fixed-01` elimina la
separazione nel medesimo filmato di viaggio che la mostrava prima. Il runtime
con pacing include quella correzione. Salvataggi, varianti narrative e matrice
video mantengono ricevute proprie: l'esito del catalogo non le sostituisce.

## Evidenze storiche del prototipo

Evidenze locali del 30 ottobre 2026. Queste prove riguardano Windows NTSC-U e
la galassia a tre destinazioni; non attestano PAL, l'intera campagna o
l'approvazione soggettiva del pilotaggio.

## Correzione controlli

Build DLL SHA-256:
`604a423a540f1f0c12483037f7ba15203be92b9512d0db909bc0f8aede2dee82`.

Cinque suite CTest superate: flight, voyage, render_math, input_adapter, assets.
Le trasformazioni vengono confrontate con la sequenza letterale nativa
Rx/Ry/Rz: 1.651 controlli, incluse inclinazioni visive oltre ±90 gradi.
L'adattatore raw ha 18 controlli su assi, D-pad, specchiatura, modalità
verticali, direzione vista e ritorno all'assetto.

Sono stati corretti tre errori del primo prototipo:

1. Scafo e orientamento della camera applicavano il rollio al contrario
   rispetto alla posizione della camera. Ora condividono la base Euler nativa.
2. La base ricavata dall'alto del mondo cambiava verso oltre ±90 gradi di
   inclinazione visiva, provocando un ribaltamento improvviso dello scafo.
   L'asse laterale dipende ora dallo yaw, come nella rotazione originale.
3. Una curva dello stick al 12% sostituiva la deadzone e il fallback D-pad
   originali, rendendo molto più forti gli input vicini al centro. Ora si usa
   l'adattatore del pad nativo. Nessun coefficiente della fisica è stato ritoccato.

La fixture `qa-space-controls-native-02` ha eseguito 4.700 frame nel runtime
nativo Vulkan: imbarco reale a Kerwan tramite report Triangolo, decollo,
stick destro/sinistro con rilascio, picchiata, risalita, turbo, pausa e ripresa.
Le sequenze raw sono riproducibili dal suo `fixture.json`. Le immagini ai frame
2.520, 2.700, 2.940 e 3.300 mostrano assetto e recupero; 4.320 mostra la pausa.
Il frame 3.300 conferma visivamente l'eliminazione del ribaltamento al polo.
L'uscita al limite di frame è avvenuta normalmente. Nessuna scheda di memoria
dell'utente è stata copiata o modificata.

## Percorsi già eseguiti prima della correzione dei controlli

- `qa-space-natural-probe-03`: imbarco reale Kerwan, decollo, volo, ingresso e
  caricamento originale di Novalis. A fine prova era ancora attiva una scena
  narrativa di Novalis: non equivale a completare quella scena.
- `qa-space-styled-route-01`: Novalis → Kerwan → Novalis con Weapon XP 0.6.1
  e Cinematic Journeys 0.1.0 caricati; entrambi gli atterraggi conclusi. La
  preparazione della partenza era esplicitamente scriptata.
- `qa-space-unknown-route-01`: avvicinamento a Eudora bloccata e ripetuto
  tentativo Triangolo senza avviare l'arrivo.

I primi due percorsi confrontano coordinate, visite, bolts, munizioni, oggetti,
livello nativo e livello caricato fra inizio volo, avvicinamento e consegna al
loader: tutte e sette le categorie invariate. Queste prove precedono la DLL
sopra indicata e non sono presentate come una nuova prova dell'intero percorso
con la build corretta. L'autopilota diagnostico genera comandi normalizzati:
non dimostra il comportamento del raw pad, coperto dalla fixture specifica.

## Runtime

Runtime isolato: OpenRAC 0.10.0 con estensione SDK 1.11, SHA-256
`04d160891488e913fe5d4e3f4f7059ba24b978a4a2bebd09f277106e8059e592`.
Il bridge ha prove proprie Vulkan/OpenGL: scena temporanea, rilascio risorse,
rifiuto di acquisizioni annidate, gestione di eccezioni e RAM del gioco
completa di 32 MiB invariata durante la sospensione.

Restano da completare le prove di accettazione dell'intera matrice concordata:
tutte le varianti narrative delle navicelle, la campagna completa, le modalità
video con ogni messaggio e gli errori di caricamento nel percorso completo.
La partita interattiva corretta serve prima a validare il pilotaggio con
l'utente. Il catalogo completo segue l'approvazione del prototipo.
