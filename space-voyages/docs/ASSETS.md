# Asset locali della prima galassia

Il generatore legge una copia NTSC-U o PAL di R&C1 estratta dall'utente. Non modifica
ISO, livelli, mod o salvataggi installati. L'output e privato: contiene artwork
derivato dal gioco e non deve essere incluso nel repository sorgente.

```powershell
python tools/build_assets.py --iso "<install>/iso_data/rac1/disc.iso" `
  --levels "<install>/port/ntsc" --editor "<OpenRAC>/editor" --out "<nuova-cartella>"
python tools/test_asset_formats.py
python tools/verify_assets.py "<nuova-cartella>"
```

La modalita opzionale `--full-galaxy` conserva il prototipo a tre pianeti come
comportamento predefinito e costruisce tutte le destinazioni in una **nuova**
cartella. I dati precedenti non vengono riscritti. Eseguire tutti i test con
`python -m unittest discover -s tools -p "test_*.py"`.

Servono Python 3.10+, Pillow e l'editor OpenRAC installato, il cui decoder WAD
viene riutilizzato. Un output esistente viene rifiutato. `inventory.json`
registra dimensioni e SHA-256 di ogni file prodotto. `provenance.json`
descrive origini, classi, posa, trasformazioni e ricostruzioni.

## Regione del disco

`--region auto` e il valore predefinito. L'identificazione usa l'eseguibile
effettivo del disco e il suo SHA-1 conosciuto, tramite il lettore dell'editor.
Sono supportati NTSC-U v1.00 (`SCUS_971.99`) e PAL v2.00 (`SCES_509.16`).
Si puo richiedere esplicitamente `--region ntsc` o `--region pal`: una regione
diversa viene rifiutata prima della creazione dell'output. Anche i manifest
dei livelli esportati devono appartenere alla stessa regione, per evitare
un miscuglio silenzioso fra ISO PAL e GLB NTSC.

I gruppi TOC usati per immagini dei pianeti, font, radar e nave sono condivisi
dalle due versioni. Le tabelle metriche del font vengono trovate nel codice
dell'overlay sorgente, senza indirizzi RAM fissi. Nelle copie verificate i
pixel del font, tutti i 232 record e il radar FX50 sono identici; il font piccolo
si trova a 0x1c3970 in NTSC-U e a 0x1c3c70 in PAL. Le risorse possono avere lo
stesso aspetto mentre routine, indirizzi e tempi del runtime richiedono un
adattatore regionale proprio.

Per l'export PAL l'editor seleziona la copia gameplay_ntsc presente sul disco
PAL: e quella usata dal port PAL in modalita 60 Hz, non il disco americano.
La provenienza registra questa scelta, la tabella delle normali della versione
e l'identita dell'eseguibile. L'hash del boot e verificato; non si presenta
questa verifica come un nuovo hash integrale di tutta l'ISO.

```powershell
python tools/build_assets.py --region pal --full-galaxy `
  --iso "<R&C1 PAL v2.00.iso>" --levels "<export-port-PAL>" `
  --editor "<OpenRAC>/editor" --out "<nuova-cartella-PAL-full>"
python tools/batch_assets.py --source "<cartella-PAL-full>" `
  --out "<nuova-cartella-PAL-batched>"
```

La generazione e i test degli asset non costituiscono una prova della mod
nel gioco PAL. Quella richiede runtime, hook, pilotaggio e transizioni PAL
verificati separatamente; le cartelle NTSC esistenti rimangono intatte.

## Pianeti: immagini originali, corpi tridimensionali

La prima galassia usa gli ID nativi **Novalis 1, Kerwan 3, Eudora 4**. Aridia e
il livello 2. Le immagini corrette dei globi non sono i panorami del gruppo
TOC `planets`: sono le PIF 128x128 della scena di viaggio nel lump `transition`
(TOC 0x13b8), `planet_picture(level)`. Il generatore decodifica WAD/PIF e
palette CSM1, convertendo l'alpha PS2 0..128 in 0..255.

L'immagine originale descrive una sola vista illuminata, non una superficie
equirettangolare completa. Viene proiettata su una sfera vera da 3.968 triangoli
e 2.145 vertici; il lato non documentato e ricostruito per campionamento
specchiato dello stesso disco. Non viene presentato come una mappa 360 originale.
Le texture risultanti sono opache: la trasparenza circolare della preview non
diventa un buco nel modello. Atmosfera e illuminazione appartengono al renderer.

## Navicelle

Le classi native 531/532/533 provengono da `level_98`, con texture, UV, normali e
topologia originali. Viene campionata la **sequenza 1, frame 0**, selezionata
anche da `0028e8a0.c` per il volo interplanetario. Lo skinning GLB applica le
matrici inverse di bind e rispetta gli accessor interi normalizzati: trattare le
rotazioni int16 come float grezzi produce scafi enormi e viene coperto da test.
La posa risultante viene centrata e normalizzata a 2,5 metri lungo +X, con Z in
alto. Nessuno scheletro viene modificato durante il volo.

`--ship-overrides <pack-attivo>` cerca i modelli sostitutivi in level_98,
level_01 e level_00, in quest'ordine, e legge le loro texture relative. Serve
per rispettare, per esempio, la sostituzione Veldin Original Ship. Ogni
materiale genera una parte distinta; il renderer deve disegnare tutte le parti
con lo stesso prefisso `ship_<classe>`. La prima parte conserva il nome senza
suffisso; le successive usano `_part1`, `_part2`, ecc.
Questo percorso riguarda sostituzioni delle classi canoniche. La nave Original
del garage, identità0 e modello 2047 indipendente, usa invece le estensioni06/07
documentata alla fine di questo file; non sostituisce `ship_531`.

## Font e sfondo

`font.rgba.bin` e il font piccolo originale, FX2 del lump del titolo (TOC
0x14e8), 256x128. I 232 record di `font.tsv` sono localizzati nell'overlay
originale di Novalis mediante le stesse sequenze MIPS lui/addiu usate dai
wrapper del testo: nessun indirizzo RAM rilocato viene indovinato.

`font.tsv` inizia con `256 128`, poi una riga `codice u v top advance` per byte.
Le celle sono 16x16, tranne i simboli del pad 0x10..0x1f che sono **24x16**.
Triangolo e il byte **0x12**, u48 v96, advance18. Il font conserva i colori
originali RGB128; il renderer nativo li moltiplica per il colore del testo e
il fattore GS. L'alpha esportato e gia 0..255.

`space_sky.rgba.bin` conserva il panorama 2048x1024 gia estratto dalla scena
spaziale originale. Non e un campo stellare generato o un'immagine nuova.

`radar.rgba.bin` conserva il radar **FX50 di Gemlik**, 128x128: cornice metallica,
reticolo e freccia centrale nativi. Il catalogo della galassia aggiunge solo i
contatti delle destinazioni. Il builder base esclude FX49, l'arco della salute
del combattimento; l'estensione `flightfx` descritta sotto lo aggiunge per i
danni alla nave. La coppia radar/contatti va verificata visivamente con il renderer.

## Contratto renderer

`assets.tsv` ha una riga per parte:

```
id verticesfile indicesfile rgbafile width height
```

I vertici sono float32 little-endian interlacciati `position3 normal3 uv2`
(32 byte), gli indici triangolari uint32 little-endian, i pixel RGBA8 top-down.
Le sfere sono unitarie; scala, posizione e stato di sblocco restano nel catalogo
della mod. `assets.json` contiene anche conteggi e metadati, senza obbligare il
renderer a dipendere da un parser JSON. Il test di integrita non certifica la
resa in gioco, che va verificata dal prototipo nativo.

## Galassia completa e strutture artificiali

Modello, marker, dimensioni e posizione appartengono ai dati di presentazione
del contenuto. La registrazione di un mondo non genera automaticamente un
nuovo modello o contatto della galassia. Identità stabile, registrazione host
e policy restano separate dal caricamento degli asset; il contratto del
gateway condiviso è in [CUSTOM_DESTINATIONS.md](CUSTOM_DESTINATIONS.md).

`--full-galaxy` produce 15 globi: Veldin, Novalis, Aridia, Kerwan, Eudora,
Rilgar, Umbris, Batalia, Gaspar, Orxon, Pokitaru, Hoven, Oltanis, Quartu e
Kalebo III. Veldin 0/18 condivide un corpo; le due visite di Orxon utilizzano
entrambe il livello 10. Le regole narrative non appartengono agli asset.

Blarg Station (6), Gemlik Base (13) e Drek's Fleet (17) utilizzano la geometria
3D originale dei rispettivi livelli, **mai una sfera con la loro immagine**.
Il generatore unisce il terrain e **tutte** le istanze TIE collocate mediante
le matrici originali. Non carica Moby, nemici, script o collisioni. Gli shrubs
e il cielo del livello vengono esclusi; rocce scenografiche e moduli statici
disconnessi appartenenti a terrain/TIE sono invece presenti. Non viene
aggiunto un ulteriore corpo celeste dallo sfondo del livello.

Ogni TIE conserva i suoi 64 colori di illuminazione originali: `_LIGHT_SLOT`
seleziona il colore della singola istanza, con RGBA nativo little-endian e
fattore 1/128. Il terrain conserva `COLOR_0`; i valori RGB possono superare
1.0 e non vengono troncati. Le normali usano l'inversa trasposta della matrice
lineare, anche in presenza di scala non uniforme. Una riflessione corregge
anche l'orientamento dei triangoli.

Il batching unisce esclusivamente materiali con pixel RGBA identici, medesime
dimensioni, sampler per asse, alphaMode/alphaCutoff e doubleSided. Non ci sono
atlas: gli UV originali, anche negativi o maggiori di 1, rimangono intatti.
REPEAT e CLAMP_TO_EDGE vengono conservati per asse. I fattori baseColorFactor
vengono applicati al colore dei vertici.

Tutte le parti di un corpo condividono un'origine: il centro della sua AABB
completa. Le posizioni vengono divise per la massima distanza di un vertice
da quel centro: **bounding sphere di raggio 1, Z in alto**. Il renderer usa
quindi un'unica scala e trasformazione per tutte le parti. I nomi sono
`station_blarg`, `station_gemlik`, `fleet_drek`, seguiti da `_part1`, `_part2`,
ecc. Le rocce periferiche partecipano al raggio: la scala visiva del nucleo
va controllata nel gioco prima di cambiare il criterio.

La versione completa estende il contratto TSV in modo append-only:

```
id verticesfile indicesfile rgbafile width height stride wrapS wrapT alphaMode
```

Le prime sei colonne restano immutate. Lo stride e 32 per globi/navicelle e
**48 per strutture**: `position3 normal3 uv2 color4`, tutti float32
little-endian. I sampler usano i valori GL 10497 (REPEAT) e 33071
(CLAMP_TO_EDGE); alphaMode e OPAQUE, MASK o BLEND. In questa estrazione tutti
i MASK usano alphaCutoff 0.5. Un lettore legacy continua a poter usare i file
a sei colonne del prototipo. Il JSON mantiene format=1, aggiunge
`full_galaxy=true` e lo stride alla singola entry.

### Misure della prima build completa

`space-voyages-assets-full-01` contiene 792 parti: 15 globi, 3 navicelle,
323 parti Blarg, 273 Gemlik e 178 Fleet. Le strutture mantengono tutti i TIE
originali LOD0, senza semplificazione o rimozione dei dettagli piccoli:

| Struttura | TIE | Classi | Vertici | Triangoli |
|---|---:|---:|---:|---:|
| Blarg Station | 1290 | 97 | 1.387.267 | 1.165.872 |
| Gemlik Base | 2040 | 120 | 1.204.153 | 999.129 |
| Drek's Fleet | 1157 | 85 | 525.540 | 435.620 |

Geometria e texture del catalogo occupano 234.414.348 byte (circa 223,6 MiB),
oltre ai piccoli asset UI e al panorama condiviso. Il limite concordato e
1024 parti e 512 MiB aggregati. Questi conteggi descrivono il costo dati;
prestazioni e resa in gioco richiedono misurazione nel renderer nativo.

`provenance.json` registra bounds e trasformazioni, texture di origine per
batch, conteggi degli attori esclusi e hash dei 1017 file letti per le scene
statiche. Un secondo passaggio verifica che quei file siano rimasti immutati
durante la generazione. L'ISO viene aperta esclusivamente in lettura.
`verify_assets.py` controlla entrambi i formati, l'inventario SHA-256 completo,
UV finiti, colori, normali unitarie, indici e bounds aggregati delle parti.

## Batching senza perdita della geometria

`batch_assets.py` produce una cartella separata dal catalogo completo:

```powershell
python tools/batch_assets.py --source "<full-01>" --out "<nuova-cartella-batched>"
python tools/batch_assets.py --verify "<cartella-batched>"
```

Non applica LOD e non rimuove vertici, triangoli o dettagli. Raggruppa i
materiali per corpo, wrapS, wrapT, alphaMode, alphaCutoff e doubleSided,
conservando separati OPAQUE e MASK. Gli originali restano immutati. Pianeti,
navicelle, font, radar e sfondo vengono copiati senza modificare i loro pixel
o buffer. L'inventario originale viene verificato prima e dopo la generazione.

Le strutture usano **stride64**:

```
position3 normal3 uv2 color4 atlas_rect4
atlas_rect = (scaleU, scaleV, offsetU, offsetV)
```

I primi 48 byte di ciascun vertice sono identici all'originale. Gli UV restano
quelli nativi, anche quando attraversano una cucitura o sono negativi. Il rect
e costante su tutti i vertici di uno stesso materiale. Il fragment shader
deve prima interpolare gli UV originali, poi applicare il sampler nativo:

```glsl
vec2 wrappedUV = vec2(
    wrapS == 10497 ? fract(originalUV.x) : clamp(originalUV.x, 0.0, 1.0),
    wrapT == 10497 ? fract(originalUV.y) : clamp(originalUV.y, 0.0, 1.0));
vec2 atlasUV = atlasRect.zw + wrappedUV * atlasRect.xy;
```

Applicare `fract` ai vertici prima dell'interpolazione sarebbe errato. Il rect
puo essere una varying `flat`; gli UV mantengono l'interpolazione prospettica
originale. Le prime dieci colonne del TSV restano invariate; lo stride64
identifica questa variante. I file atlas possono essere condivisi fra parti.

Ogni tile conserva i suoi texel RGBA e due pixel di gutter per lato: REPEAT
copia il bordo opposto, CLAMP_TO_EDGE ripete il bordo vicino. Anche gli angoli
rispettano entrambi gli assi. La texture usa **GL_LINEAR senza mipmap o
anisotropia**. Le dimensioni dell'atlas sono potenze di due, cosi tutti i rect
sono rappresentabili esattamente in float32. Eventuali mipmap futuri richiedono
padding e generazione per tile propri; non possono essere aggiunti all'atlas
attuale senza una verifica nuova.

I chunk si fermano fra materiali originali, a 262.144 vertici (16 MiB).
Nessun materiale viene diviso; ogni indice conserva il valore originale piu
l'offset del suo blocco di vertici. Il verificatore confronta **ogni** prefisso
di 48 byte, ogni indice e ogni tile con il catalogo sorgente, oltre all'inventario.
Il test CPU del bilineare confronta i sampler originali e il mapping del
fragment su UV negativi, 0, 1, frazioni e punti vicini alle cuciture.

La build privata `space-voyages-assets-batched-01` riduce le strutture da
**774 a 27 draw**, per 45 mesh totali e 21 atlas. Conserva 3.116.960 vertici e
2.600.621 triangoli delle strutture. Gli atlas unici occupano 76.873.728 byte;
il reader occuperebbe 368.925.452 byte (351,8 MiB) anche caricando nuovamente
le immagini condivise per ciascun chunk, entro il limite di 512 MiB.
Il massimo errore dell'oracolo bilineare CPU e circa 1,39e-11 in unita byte.
Queste prove certificano i dati e il mapping matematico; prestazioni e resa
GPU richiedono la prova nel renderer nativo.

## Estensione privata: asteroidi originali di Gemlik

`tools/asteroid_assets.py` aggiunge due varianti originali a una **nuova**
cartella, lasciando intatti tutti gli output 0.3.3 e gli estratti del gioco:

```powershell
python tools/asteroid_assets.py --base "<base-batched-0.3.3>" --levels "<estratti-regionali>" --region pal --out "<nuovo-pack>"
python tools/asteroid_assets.py --verify "<nuovo-pack>"
```

Gli estratti devono appartenere alla stessa regione del pack di base. La
fonte è `level_13/manifest.json`, `placements.json` e i GLB/PNG originali:

| Gruppo stabile | Classe originale | Vertici | Triangoli | Materiali |
|---|---:|---:|---:|---:|
| `asteroid_0` | 212 (`0xD4`) | 3222 | 3584 | 6 |
| `asteroid_1` | 1412 (`0x584`) | 695 | 522 | 4 |

212 è l'asteroide collocato 130 volte nella Gemlik originale. 1412 è il modello
creato dalla routine nativa `spawn_asteroid_fragment`, senza collocazioni
iniziali. Sono **due varianti permanenti, non LOD intercambiabili**. Entrambi
sono attori statici non skinnati e senza animazioni di vertici: la rotazione
e il movimento spettano al controller. Non vengono importati aggiornamenti
nativi, danni, collisioni, script o logiche di respawn.

Entrambi hanno un corpo centrale e 33 patch di crateri/escrescenze con vertici
separati. Le 34 isole topologiche appartengono alla superficie di un solo
asteroide, non sono 34 rocce separate da introdurre insieme nella galassia.
L'export conserva **tutti** i componenti, senza eliminare i crateri. La preview
offline in tre angoli rende verificabile questa distinzione.

Ogni variante usa un unico centro, il centro AABB di tutti i materiali, e un
unico divisore pari al massimo raggio originale moltiplicato per 1,000001.
Il margine mantiene anche i vertici float32 entro la sfera di raggio 1:
`asteroid_0` arriva a 0,999998992 e `asteroid_1` a 0,999998974. Asse Z in alto,
origine locale `(0,0,0)`, stesso transform per tutti i materiali della variante.
Il renderer può moltiplicare direttamente per il raggio fisico del core.

Sette texture native 128×128 sono condivise in un atlas 512×512, con gutter 2,
OPAQUE, `doubleSided=true`, REPEAT su entrambi gli assi e stride 64 già
documentato sopra. Ogni gruppo diventa una mesh e richiede una draw instanziata.
Le UV originali rimangono inalterate, anche da −5 a quasi 8: il wrap avviene
nel fragment dopo l'interpolazione. L'oracolo GL_LINEAR/no-mipmap registra
errore massimo 1,32e-12 in unità byte. Non sono richieste immagini AI, nuove
texture, semplificazione o cambi di shader/runtime per leggere il formato.

La versione 1 dell'estensione mantiene il prefisso del TSV esistente
byte-per-byte e aggiunge soltanto `asteroid_0`/`asteroid_1`. Le 45 entry vecchie
del JSON e tutti i loro buffer/PNG/font/radar sono copiati identici; i quattro
metadati originari sono anche archiviati come `base-0.3.3.*` nel nuovo pack.
Inventario e provenienza vengono aggiornati esclusivamente nel derivato.
`asteroid-extension.json` registra fonti e SHA256, normalizzazione, componenti,
classi, materiali e rettangoli atlas; gli input sono ricontrollati dopo la build.

I derivati privati `space-voyages-assets-pal-asteroids-01` e
`space-voyages-assets-ntsc-asteroids-01` contengono 47 mesh. I nuovi buffer e
l'atlas delle due regioni coincidono; la provenienza mantiene i riferimenti
regionali distinti. L'allocazione reader aggregata è 371.322.564 byte, compresa
la copia logica della texture condivisa per ogni mesh, entro 512 MiB.

`test_asteroid_assets.py` verifica immutabilità/append-only, UV/colore/indici,
bounds, rilevamento delle isole senza rimozioni, rigetto di regioni miste,
trasparenza inattesa, corruzione e sovrascritture. La suite asset estesa passa
25 test. Queste prove riguardano dati e contratti, non la resa in gioco.

`tools/preview_asteroids.py --assets "<nuovo-pack>" --out "<nuova-preview.png>"`
produce una preview software testurizzata dei triangoli esportati. Richiede
NumPy soltanto per questa anteprima opzionale; il builder richiede Pillow.
È una vista ortografica offline con luce di ispezione dichiarata, non una
cattura di gioco o una prova del comportamento degli asteroidi.

## Estensione privata: HUD salute e propulsori originali

`tools/flight_effects_assets.py` crea un altro derivato, senza modificare il
pack 0.4, gli estratti o il disco. Le due regioni sono verificate dalla firma
del boot originale tramite il reader OpenRAC, prima di leggere le risorse:

```powershell
python tools/flight_effects_assets.py --base "<pack-0.4>" --levels "<estratti-regionali>" --iso "<disco-originale>" --editor "<OpenRAC/editor>" --region pal --out "<nuovo-pack>"
python tools/flight_effects_assets.py --verify "<nuovo-pack>"
```

Il pack mantiene tutti i buffer, le texture, i font, gli asteroidi, il catalogo
`assets.json` e l'intero `assets.tsv` precedenti byte-per-byte. Solo la
provenienza e l'inventario del derivato vengono aggiornati, con copie dei
metadati precedenti in `base-0.4.*`. `flightfx-extension.json` registra le
fonti regionali, i boot/table address verificati tramite PT_LOAD dell'ELF,
le superfici degli ugelli, la normalizzazione e ogni hash. I file originali
commerciali rimangono locali; nessuna immagine viene inclusa nel repository.

Il file opzionale `flight-effects.tsv` ha schema v1:

```text
format 1
health health.indices.bin health.palette.rgba.bin 128 128
sprite flame flame.rgba.bin 32 32 1 1 1 0
mount 531 px py pz dx dy dz radius length
color 531 1.375 0.6875 0.25 1
```

Le righe `mount` si ripetono per ogni ugello; `color` per ogni classe. Le
classi 531/532/533 hanno rispettivamente 8/2/2 ugelli originali. `sprite`
contiene nome, file RGBA, larghezza, altezza, colonne, righe, frame e FPS:
`hit` e `explosion` sono opzionali e non vengono inventati in questo pack.
Un vecchio pack privo del file resta valido; un file presente ma malformato
deve essere rigettato dal reader. Dimensioni e byte count sono espliciti.

La salute usa Gemlik FX49, 128×128, conservando i pixel **R8 indexed** e il
CLUT logico di 256 RGBA. Viene applicata soltanto la permutazione CSM1 della
palette e la conversione dell'alpha PS2; gli RGB e gli indici sono originali.
La fonte `DrawGemlikTurretHud` disegna salute e radar FX50 sullo stesso quad,
prima salute poi radar, con colore GS `0x70808080`: RGB128 neutro e alpha
112/128. Disegnare il radar con alpha1 coprirebbe la salute sottostante.
L'indice base9 e verificato anche nel pvar del fighter class69 di Gemlik
PAL, offset0x104. Il renderer deve fare il lookup CLUT **prima** del filtro
bilineare; filtrare direttamente l'indice produrrebbe colori e soglie errati.
La soglia originale e `clamp(int(hp_shown*251/max_hp)+2,2,253)`: indici0/1
sono preservati; gli altri con `j>=soglia` diventano nero opaco. L'immagine
originale usa indici0..252, nessun pixel254/255. Il CLUT sorgente resta intatto.

La fiamma usa il glow bianco **Gemlik FX5**, 32×32 statico, senza frame AI,
upscale o ricolorazioni nei texel. Il callback originale
`DrawResidentEffectQuads` chiama `GetEffectTex(5)`. Questa fonte e dichiarata
esplicitamente: il WAD di transizione registra solo FX0..3 e il FX5 del
titolo e un'immagine diversa. Le tinte vengono conservate come coefficienti
GS RGB/128: 531/532 `(176,88,32)/128`, 533 `(0,128,48)/128`. L'alpha dinamico
di luminosita non fa parte degli RGB e resta separato.

Le tabelle native degli offset cinematici sono registrate come riferimento,
**non applicate direttamente** ai modelli statici: la loro posa/origine non
coincide con la posa baked. Le ancore sono invece ricavate dagli anelli
originali degli ugelli, con vertici interamente pesati sui joint dedicati:
531 joint6/5/7/4/25/24/26/23, 532 joint19/20, 533 joint30/31. Vengono campionati
e skinnati con la stessa sequenza1/frame0 e lo stesso centro/scala del body.
Il centro delle facce rivolte all'esterno, pesato per area, determina il
centro dell'apertura; il punto di montaggio viene portato sul piano esterno
massimo, evitando offset aggiunti a mano. Il raggio e la distanza dal centro
al bordo interno proiettato. Le normali sono -X per531/532, mentre533 ha
ugelli inclinati: circa `(-.773692,+/-.381150,-.506088)`. Il cono volumetrico
e un adattamento dichiarato del quad nativo; la lunghezza base e4×raggio.

Le sostituzioni esclusivamente di texture conservano la geometria delle
ancore. Un modello con ugelli diversi richiede metadati espliciti associati
allo SHA256 del modello baked, tramite `--mount-overrides` (oggetto per
classe con `model_sha256` e `mounts`). Il builder rigetta modelli differenti
senza tale corrispondenza: non garantisce ancore per sostituzioni arbitrarie.

I pack privati PAL/NTSC `space-voyages-assets-*-flightfx-01` aggiungono solo
21.504 byte raw, oltre ai metadati, e hanno TSV coincidenti. Gli 8 test in
`test_flight_effects_assets.py` verificano CLUT/alpha, soglie, bounds delle
banche, mapping ELF, apertura e normale di un ugello inclinato, tinte GS,
regioni miste e rifiuto delle sovrascritture. Il verificatore confronta ogni
payload del pack precedente e gli SHA delle sorgenti. Sono prove dei dati,
non una prova delle fiamme, del danno o del respawn in gioco.

`tools/preview_flight_effects.py --assets "<pack>" --out "<nuova-cartella>"`
produce viste software dei modelli originali con anelli e normali diagnostici,
oltre al compositing indicizzato dell'HUD. Richiede NumPy opzionale. Queste
immagini sono etichettate come offline e non sono screenshot del gioco.

## Estensione privata: laser originale e armi spaziali della mod

`tools/weapon_assets.py` deriva `flightfx-02` da una copia nuova di
`flightfx-01`. Il primo pack rimane immutato. Le 47 entry precedenti, i loro
buffer/texture, `flight-effects.tsv` e salute/fiamme restano byte-identici;
il catalogo aggiunge solo `projectile_laser` come quarantottesima mesh.
I quattro metadati precedenti sono archiviati come `base-flightfx-01.*`.

```powershell
python tools/weapon_assets.py --base "<flightfx-01>" --levels "<estratti-regionali>" --region pal --out "<flightfx-02-nuovo>"
python tools/weapon_assets.py --verify "<flightfx-02>"
```

La fonte del laser e il modello Gemlik **1009/0x3F1**, 456 vertici e 384
triangoli, con texture originale `moby_0184.png` 32×32. Si conservano ordine
e valori di normali, UV, colori e indici; alle posizioni viene applicata solo
la scala di classe originale `0.3971484303474426`. Il nuovo buffer ha stride 48.
La texture originale e RGB opaca, convertita in RGBA senza cambiare i colori;
non e una fiamma blu, una sfera o una nuova geometria disegnata per imitazione.
La trasparenza/additivita deriva dall'oggetto nativo: mode0x200 e alpha24/128,
come interpretato da `frontend/frame.cpp`, non dall'alphaMode del GLB statico.
La draw dedicata usa depth test e nessuna scrittura depth. Il modello punta
lungo +X; il renderer sottrae maxX prima della scala per allineare la punta
visibile alla posizione fisica del proiettile.

`weapons.tsv` opzionale e separato da quello dei propulsori:

```text
format 1
ship 531 0
ship 532 1
ship 533 1
tuning 400 200 4
bolt projectile_laser
muzzle 532 px py pz 1 0 0
bolt_scale 532 0.2573812719
```

`muzzle` contiene classe e sei float (posizione e direzione normalizzata nel
medesimo spazio baked del body); puo ripetersi per ogni apertura. `tuning`
contiene velocita, portata e cadenza in tick60Hz. `bolt_scale` riguarda solo
la resa: non cambia velocita, portata o collisioni. 531 rimane disarmata.
La policy di fuoco viene dal diverso caccia originale class69: Quadrato/L1
tenuti, alternanza dei due emettitori, cooldown4tick, velocita400 piu la
velocita della nave, portata200. Il parametro200 dello spawn e **portata**,
non velocita. La routine nativa dei missili e distinta e non viene importata.

L'abilita delle navi cinematiche532/533 e una **regola nuova della mod**,
non il ripristino di una loro logica nativa di combattimento. Le superfici
frontali delle nacelle sono reinterpretate come emettitori integrati,
conservando il body. Non vengono usati i faretti blu sul naso o il centro
della nave. Su532 i centri provengono dai due dischi planari di12triangoli,
individuati sull'asse geometrico delle nacelle originali. Su533 provengono
dalle due bocche ottagonali anteriori dei gruppi skinned joint5/21, sul piano
di X massimo del rispettivo elemento. `weapons-extension.json` conserva
gli indici dei vertici sorgenti, gli SHA dei modelli e il criterio geometrico.
Una sostituzione del modello non viene interpretata automaticamente: il
builder rigetta i modelli non corrispondenti, richiedendo un adattamento
esplicito dei metadati per quelle nuove bocche.

Il raggio del laser scalato deve entrare nell'apertura. La scala visuale e
`min(scala_body/scala_classe_body, raggio_apertura/raggio_laser_nativo)`.
532 conserva il rapporto originale normalizzato `0.2573812718629368`;
533 usa `0.11293803728050934`, limitato dalla bocca ottagonale: usare
semplicemente il rapporto body `0.22023399` renderebbe il fascio circa due
volte piu largo dell'apertura. I quattro emettitori hanno direzione +X.

La distruzione originale degli asteroidi compone frammenti e piu tipi di
particelle tramite `process_asteroid_hit`/`spawn_explosion`. Non viene
falsamente attribuito un singolo sprite generico a quella sequenza:
`impact`/`explosion` restano opzionali finche la fonte esatta non e verificata.
Non vengono importati bolt, ricompense, missioni o codice di salvataggio.
Comportamento, collisioni e assenza di ricompense spettano al core della mod.

I due pack regionali verificati hanno mesh/TSV delle armi coincidenti. I test
aggiunti controllano il raggio inscritto dell'ottagono, le due nacelle con
esclusione di un faretto centrale, l'immutabilita/append-only del pack e il
rigetto di corruzione e sovrascritture. Il verificatore confronta ogni vertice,
indice e texel del laser con la fonte. La suite completa comprende37test.
Le preview di `tools/preview_weapon_assets.py` sono ispezioni software
etichettate, non prove della resa in gioco o del combattimento.

## Estensione privata: mirino originale e leggibilita del plasma

Il derivato opzionale `flightfx-03` conserva tutte le48mesh e i rispettivi
payload di02. Aggiunge soltanto36.864byte RGBA originali e metadati visuali:

```powershell
python tools/weapon_assets.py --visibility --base "<flightfx-02>" --iso "<disco-verificato>" --editor "<OpenRAC/editor>" --region pal --out "<flightfx-03-nuovo>"
python tools/weapon_assets.py --verify "<flightfx-03>"
```

Il vecchio prefisso `weapons.tsv` resta intatto e vengono aggiunte queste righe:

```text
reticle ring reticle-ring.rgba.bin 64 64 1 1 1 0
reticle marks reticle-marks.rgba.bin 64 64 1 1 1 0
reticle center reticle-center.rgba.bin 32 32 1 1 1 0
bolt_growth 532 3 6
bolt_growth 533 6 6
```

Il mirino e quello originale di `DrawGemlikTurretHud`: GemlikFX17 (tre archi),
FX18 (tre marcatori) e FX8 (cerchio interno), senza ridisegno o upscaling.
Il decode conserva RGB e alphaPS2×2, con riordino CLUT CSM1. L'originale
compone17/18 a40×40 e8 a10×10 coordinate native, con pivotcentrale(.5,.5),
UVpixel1..size-1 e colore GS `0xFF20FF20`. Quest'ultimo va applicato come
RGBA(32,255,32,255)/128 al texel e poi clamp finale: impostare alpha1 prima
del prodotto indebolirebbe gli edge intermedi rispetto all'originale.
La posizione originale e la proiezione di un punto23unita avanti lungo
la direzione di mira con la velocita della nave. Viene proposto soltanto per
le navi armate della mod, non per531.

La crescita del fascio e invece una **scelta visuale della mod**, dichiarata:
nasce con la scala che entra nella bocca, poi raggiunge3×su532 o6×su533 dopo
6unita percorse. Non e presentata come aspetto1:1del gioco originale. Non
cambia geometria/texture originali del proiettile, alpha24/128, posizioni dei
cannoni, velocita400, portata200, cadenza4tick, collisioni o ricompense.

Il builder crea sempre una nuova cartella, salva i metadati02 precedenti in
`base-flightfx-02.*` e registra fonti regionali, hash e adattamento in
`visibility-extension.json`. Il verificatore controlla ogni vecchio payload,
il prefisso delle regole fisiche, e confronta i tre RGBA con la banca del
disco originale verificato. Tre test aggiuntivi verificano slot/alpha/UV,
immutabilita delle regole e delle mesh, corruzione e regioni miste. La suite
Python completa passa40test. Questa prova dei dati non certifica leggibilita,
mirino, suoni o prestazioni in gioco: il confronto nativo della nuova resa e
separato dalle prime catture della candidate d164.

## Estensione privata: audio originale della battaglia di Gemlik

`flightfx-05` conserva ogni payload visuale e le regole di03 byte per byte.
Aggiunge soltanto `audio.tsv`, PCM16 little-endian e ricevute di provenienza.
Tutti i campioni provengono dal banco originale del livello13 o dalla sua
musica richiesta dal caccia. Non vengono aggiunti droni ambientali, suoni
sintetici o cue della cinematica di viaggio. Il respawn non ha un suono
dedicato: non e stato verificato un cue originale di Gemlik appropriato.
I PCM commerciali restano nelle cartelle private `work`, fuori dalla repo
e dagli archivi pubblici della mod, come mesh e texture originali.

Il renderer offline e separato dal gioco e dal dispositivo audio:

```powershell
cmake -S tools/audio_bake -B "<build-offline>" -DOPENRAC_PORT="<OpenRAC>/port"
cmake --build "<build-offline>" --config Release
python tools/audio_assets.py --base "<flightfx-03>" --iso "<disco-verificato>" --editor "<OpenRAC>/editor" --region pal --tool "<build-offline>/Release/space_native_audio_bake.exe" --stage "<stage-nuovo>" --out "<flightfx-05-nuovo>"
python tools/audio_assets.py --verify "<flightfx-05>"
```

Il builder riconosce la regione tramite SHA-1 dell'eseguibile originale,
legge l'ISO in sola lettura e rifiuta regioni miste e cartelle gia esistenti.
Il tool C++ collega i decoder VAG/ADPCM e il `SoundPlayer` originali del port,
inclusi grain VM a240Hz, ADSR, pitch e interpolazione Gaussian dello SPU.
Gli SFX sono renderizzati a48kHz stereo con volume della definizione nativa,
volume del banco e dei tone e inviluppo originali; non si normalizzano i picchi.
Il pitch-bend casuale del chiamante viene fissato al punto medio del range
originale per ottenere un asset riproducibile. La programmazione interna
del turbo conserva invece la propria rampa di pitch originale.
Il render e dry: non pretende di ricostruire riverbero e mix finale del
livello in esecuzione. Tutti i13gain e pitch del manifest sono1: non si
aggiungono attenuazioni della musica, compensazioni, normalizzazione dei
picchi o nuove opzioni di mix. Il volume di uscita rimane quello dell'utente.

| Ruolo | Fonte originale Gemlik | Uso nella mod |
| --- | --- | --- |
| Motore531/532/533 | Caccia69, slot locale0, banco84 | Stesso motore di combattimento per i tre aspetti cinematici |
| Turbo531/532/533 | Caccia69, slot1, banco85 | Attivazione e loop turbo; rampa nativa preservata |
| Laser532/533 | Caccia69, slot3, banco87 | Emissione del laser originale1009 |
| Impatto asteroide | Asteroide212, slot0, banco186 | Colpo/urto sulla roccia |
| Distruzione asteroide | Frammento1412, slot0, banco280 | Rottura della roccia |
| Danno nave | Riutilizzo byte identico dell'impatto212 | Adattamento per l'urto introdotto dalla mod; non un inesistente slot urto del caccia |
| Distruzione nave | Caccia69, slot6, banco90 | Cue originale dell'esplosione fatale |
| Musica | Traccia richiesta2, intro2 + corpo3 | Musica di Gemlik, senza attribuzione di un titolo non verificato |

Gli ID del banco vengono letti dal remap classe/slot dell'ISO regionale,
non ricavati dal nome del modello. Non sono stati trovati banchi motore
separati per i modelli cinematici531/532/533: il riuso del caccia69 e esplicito.
531 resta disarmata e non ha un ruolo laser.

Le fonti sono `turrets_and_fighter_jet.c`: sparo in
`update_fighter_jet_gun_and_aim`, motore in `JetFighterUpdateL13` e turbo
in `PlayerFighterFlight`. La chiamata fatale a `spawn_explosion` e stata
controllata anche nel MIPS originale PAL: `0x2bc570` imposta `t2=6` prima
del `jal` a `0x26b450` in `0x2bc5c4`; le due collisioni fatali fanno lo
stesso a `0x2bc160/0x2bc1f0`. Il parametro intero viene inoltrato a
`Moby_PlaySound` dalla routine condivisa. Questo evita di attribuire il cue
sulla sola base di una dichiarazione C locale con ordine float/int errato.
`process_asteroid_hit` riproduce esplicitamente lo slot0 della roccia.
`JetFighterUpdateL13` chiama `MusicRequestTrack(2,4)`; `music_StartTrackBody`
carica l'indice successivo, quindi il corpo3 segue l'intro2.

La musica conserva il mono44100Hz originale:423416frame di intro seguiti
da5503512frame di corpo,5926928frame totali (134.3975secondi). Il loop
inizia dopo l'intro. Per motore e turbo non viene tagliata la partenza:
il tool segue la fase del campione originale e attende la stabilizzazione
della programmazione pitch prima di marcare due successivi confini repeat
ADPCM. Le coordinate del loop48k sono quantizzate a un frame di uscita.
Il motore conserva il loop53291..104456, il turbo85225..120627; la rampa
SPU del turbo va da1123 a2109. Nessun crossfade o modifica del waveform.

Formato opzionale, presente soltanto nei nuovi pack:

```text
format 1
clip music audio/music.pcm.bin 44100 1 5926928 423416 5926928
gain music 1
pitch music 1
clip engine_531 audio/engine.pcm.bin 48000 2 104456 53291 104456
gain engine_531 1
pitch engine_531 1
```

Una riga `clip` contiene ruolo, percorso relativo, rate, canali, frame,
inizio loop e fine loop esclusiva. `LOOP_END=0` significa fine del clip,
non imposta da solo la riproduzione ripetuta. Il controller decide quando
loopare musica/motori/turbo; gli SFX restano one-shot. I13ruoli di05 sono
obbligatori, con `gain` e `pitch` espliciti e neutri; `respawn` e assente. L'assenza
dell'intero manifest permette ancora di leggere i vecchi pack silenziosi.
Tre ruoli motore e tre turbo condividono ciascuno lo stesso file PCM;
i due laser condividono un file. Non esiste `laser_531`.

I pack 05 PAL/NTSC verificati hanno PCM coincidenti e manifest SHA-256
`adf935c932d89862724e50b448e9f405602775945e83182e675b09302b84a000`.
Occupano14027808byte di payload PCM e15881276byte logici per i 13 ruoli.
Tutti i campioni sono non silenziosi, nessun campione raggiunge la
saturazione16bit e non ci sono grain non supportati durante i bake.
Le differenze misurate ai confini dei loop sono18LSB per musica/motore
e15LSB per turbo: sono misure dei dati, non una dichiarazione di
inaudibilita percettiva nel mix finale.

Nove test verificano remap classe/slot, header VAG e LBA, intro/loop,
immutabilita del pack, regione, corruzione, campi mancanti e percorsi
non validi. Due esercitano direttamente il tool originale con un VAG
sintetico dal decode noto e un banco/grain/ADSR/SPU con repeat ADPCM;
verificano anche il rifiuto di sovrascrivere output e metadati.
Con `SPACE_AUDIO_BAKE_TOOL` impostato la suite Python completa passa49test.
Le prove offline non certificano ancora riproduzione, bilanciamento del
mix, cambi scena, pausa o reset nel gioco: servono i corrispondenti test
nativi con SDK audio e modulo finali.

La revisione05 corregge l'attenuazione aggiunta arbitrariamente alla musica
in04 (`gain music 0.4`), che la rendeva troppo bassa rispetto agli effetti.
04 e le sue ricevute restano immutabili. Per derivare05 senza riestrarre
o renderizzare di nuovo i campioni:

```powershell
python tools/audio_assets.py --neutral-from "<flightfx-04>" --out "<flightfx-05-nuovo>"
```

Il comando crea sempre una nuova cartella. Cambiano esclusivamente i file
`audio.tsv`, `audio-extension.json`, `provenance.json` e `inventory.json`.
Ogni PCM, mesh, texture, loop, impostazione delle armi e altro payload04
resta byte identico; `mix_parent` registra SHA e cartella di provenienza.
Il verificatore dei pack neutri rifiuta qualsiasi gain o pitch diverso da1.
Il test della revisione controlla anche che il pack 04 di partenza non sia
stato modificato e che il comando non sovrascriva una cartella esistente.

## Estensione privata: nave Original del garage

Il primo stadio `flightfx-06` deriva da 05 in una **nuova cartella**. Aggiunge il gruppo
`ship_original` e il suo vetro `ship_original_part1`, il profilo opzionale
`original-ship.tsv` e le ricevute di provenienza. Non riscrive i pack 03–05,
le fonti PAL o l'installazione dell'utente. Tutti i payload stock, i 13 ruoli
audio, i PCM, i loop, `audio.tsv`, `weapons.tsv` e `flight-effects.tsv`
restano byte identici a 05. Non vengono aggiunti campioni sonori.

```powershell
python tools/original_ship_assets.py --base "<flightfx-05>" `
  --source "<generated-initial-ship-04>" --out "<flightfx-06-nuovo>"
python tools/original_ship_assets.py --verify "<flightfx-06>"
python -m unittest discover -s tools -p "test_original_ship_assets.py"
```

Il builder verifica il GLB contro `initial-ship-provenance.json`, l'envelope
CPU `ORSHIP` con CRC e SHA, e il rig indipendente a 31 joint e 3 sequenze della
classe 2047, derivata dalla 530 PAL v2.00. Non applica il rig a 27 joint della
Green 531. Le due primitive conservano 4.784 vertici e 4.250 triangoli, di cui
148 del vetro. Il vecchio converter delle tre navi campiona seq1/frame0;
nell'Original quella sequenza apre la cupola, quindi non viene riutilizzata.

La posa `initial_cruise_v1` riproduce il riferimento seq00 e la politica
cruise di `InitialShipV1::rig` / `mechanics(state4)`: cupola 0, carrello 0,
cardini 9/12/25 ripiegati di -1,3 radianti e tiranti 16/20, 17/18, 21/22 mossi
attorno al rispettivo cardine. Scafo, telaio e vetro vengono skinnati con
la stessa palette. Il vetro non subisce una seconda chiusura separata.

La normalizzazione a lunghezza 2,5 della conversione stock produrrebbe un
raggio 2,01143, troppo grande per il volume di contatto 1,5 e la distanza
minima camera 1,85. Il profilo Original usa invece **una sola scala uniforme**
misurata dal proprio bounding sphere: target 1,45 e guardia float32 di0,000001.
Il pack verificato ha raggio effettivo 1,449998975 e lunghezza 1,802199320;
conserva proporzioni, +X avanti e +Z alto, senza modificare fisica o camera
globali. Lo stesso centro e la stessa scala vengono applicati ai propulsori.

I due ugelli sono misurati dagli anelli originali dei joint 15/24, 48 triangoli
ciascuno, confrontati con gli attachment 29/30. La direzione (-1,0,0) deriva
dalla geometria, senza copiare gli otto mount di Green. La lunghezza delle
fiamme è 4 volte il raggio dell'apertura, adattamento visuale già dichiarato
per gli altri profili. La tinta RGB (176,88,32)/128 segue il callback glow
del provider Original, che usa temporaneamente l'indice controller 531;
questo non cambia l'identità della nave.

Nello stadio 06 entrambi i materiali sono double-sided; il corpo rimane OPAQUE e il vetro
BLEND. I vertici sono esportati con stride 48 (position3, normal3, uv2,
color4), preservando il COLOR_0(.5,.5,.5,1) del vetro. Le sue UV costanti
(.5,.5) sono un placeholder per la mappatura metal nativa: il record `glass`
abilita soltanto su quella parte `MeshAsset::sphere_uv`. Il renderer usa
la formula da `port/viewer/shaders/mesh.vert`: normale nello spazio camera,
UV `(e.x * .5 + .5, .5 - e.y * .5)`, modulazione 112/128 e gate shine
`clamp((24 - centre_depth) / 16, 0, 1)`, con alpha blend e depth write spento.
La riproduzione di questa semantica non costituisce una prova visuale in gioco
del nuovo materiale.

Il manifest completo del pack 06 è:

```text
format 1
ship original 0 ship_original 2047 530 31
pose initial_cruise_v1
bounds 1.45 1.80219932
glass ship_original_part1
mount -0.7505190969 0.3290107587 -0.388474855 -1 0 0 0.2272184881 0.9088739523
mount -0.7505190969 -0.3290107587 -0.388474855 -1 0 0 0.2272184881 0.9088739523
color 1.375 0.6875 0.25 1
```

La riga `ship` dichiara nome, ID stabile Garage 0, gruppo mesh, classe
visuale 2047, classe fonte 530 e 31 joint. Il controller nativo 531 e la sua
variante 0 restano ruoli separati. L'apparenza usa lo slot interno 3; i tre
slot stock 0–2 restano invariati. Original è disarmata: niente cannoncini,
mirino o cue laser. Il profilo audio riusa i clip motore/turbo Gemlik 69
già caricati per il ruolo legacy 531, senza nuovi PCM o variazioni dei gain.

Il reader del profilo v1 pubblica `SceneAssets::original_ship.available` solo dopo aver
verificato identità, posa, corpo/vetro, semantica del materiale, bounds
misurati e due mount del profilo 3. La Session attuale richiede anche il gate
`cockpit_available` del profilo v2 descritto sotto, insieme allo stato valido
del provider e dell'origine al vero handoff. Un profilo assente nei pack 03–05
è compatibile per le navi stock; un profilo presente
ma incompleto o malformato viene rifiutato. Non viene mai mostrata Green
come sostituto silenzioso di Original.

## Cockpit Original: pose native e profilo v2

`flightfx-07-final` compone i due attori separati 0/10 senza riscrivere lo scafo
del pack 06. Il GLB 2047 continua a contenere soltanto la nave: le mesh di Ratchet
e Clank arrivano dagli export PAL di `level_03/mobys/moby_0.glb` e `moby_10.glb`.
Non viene importata una crew aggregata della nave Green.

La fonte delle pose è la cattura nativa read-only
`work/veldin-ship/qa-menu-cockpit-11402-41/cockpit-source.json`, SHA-256
`2c967c12d6de31e29ba111b1848393961278f01ef2e7d4cdfd1bd5cb595e6163`.
Al tick canonico 464, banca nativa 4/frame 80, sono ancora vivi i due attori
cinematici e il modello Original 2047 ha il rig a 31 correttamente collegato.
La cattura precede il cleanup: all'apertura di menu 14 gli attori della scena
sono già ritirati. L'Hero di gameplay nascosto non viene usato al loro posto.

`jointsRelativeToOriginalBody` contiene già basi, rapporti di scala e modificatori
dei sedili. Il bake applica una sola volta gli inverse bind dei GLB. Ratchet ha
111 joint nativi; il GLB ne contiene 113 perché aggiunge i due helper post-scale
71/91. Le matrici catturate includono già tale post-scale: i due parent semplici
non devono avere vertici pesati, condizione verificata dall'importer. Clank ha
75 joint. La sola chiave authored 40 non sostituisce questa cattura: il confronto
mostra una differenza nella posa reale di Ratchet.

I sette materiali dei due personaggi aggiungono 10.096 vertici e 11.994 triangoli.
La posa viene adattata alla canopy 530 con la sola traslazione comune **+0,055X
Space**, senza nuova scala né variazioni di articolazione, fisica o camera.
Il controllo sui 69 piani di supporto del vetro mantiene un margine di 0,004
unità per la parte superiore dei corpi. È un fit geometrico dichiarato,
non un attachment nativo 530. Le mesh risultanti restano nel raggio invariato 1,45.

I colori opachi conservano il moltiplicatore del formato moby: l'exporter usa
byte/128, con 1,0 equivalente al GS 0x80. Non si applica il `colour_scale=2` del
terreno. Il chrome di Clank usa la semantica SphereUV nativa; 445/445 posizioni
uniche coincidono con la sua superficie opaca. Solo questo materiale riceve
`metal_overlay`, con pass di profondità `LEQUAL` e ripristino immediato di
`LESS`; il vetro della canopy conserva il normale alpha blend. L'ordine è
opachi → overlay metal → vetro, evitando che la lucentezza sia respinta dalla
propria superficie o disegnata sopra il guscio trasparente.

```powershell
python tools/original_crew_assets.py --base "<flightfx-06>" `
  --iso "<R&C1 PAL v2.00.iso>" --editor "<OpenRAC>/editor" `
  --levels "<export-PAL>" --capture "<cockpit-source.json-verificato>" `
  --out "<flightfx-07-final-nuovo>"
python tools/original_crew_assets.py --verify "<flightfx-07-final>"
python -m unittest discover -s tools -p "test_original_crew_assets.py"
```

Il builder controlla il pin della cattura, contesto, classi, palette, scale,
helper post-scale, geometrie, ruoli e integrità degli input. L'output deve
essere una cartella nuova. I prototipi locali `flightfx-07` e `07-native`
rimangono ricevute intermedie; il candidato completo è `07-final`.

Nel manifest v2, oltre ai record dello scafo, sono obbligatori:

```text
format 2
sample 1 464 4 40
capture 2c967c12d6de31e29ba111b1848393961278f01ef2e7d4cdfd1bd5cb595e6163
part ship_original body 0 0
part ship_original_part1 glass 1 0
actor 0 111 1 4 8a2dfd41e8970b3c4c1297f29a19a9a239796c7d9b1a5bd8c04eab7ba35065ae bc5e0177e37d5beb083e395fe9b6cb11df5c4a7715fa2edb0b906163a5cca92a
actor 10 75 2 3 17e6c0784d6b8a0d4e1356740108fb3b76f5ab67b2879944e0f3cbcb61dfabc4 54fcf3430e6673e6d18426c5d1b2abc89b0082196aee79c52b17799721861cfd
part ship_original_part2 actor0 0 0
part ship_original_part3 actor0 0 0
part ship_original_part4 actor0 0 0
part ship_original_part5 actor0 0 0
part ship_original_part6 actor10 0 0
part ship_original_part7 actor10 0 0
part ship_original_part8 actor10 1 1
```

`sample` distingue banca di archivio 1, tick 464, chunk 4 e chiave nominale 40;
non dichiara che la posa sia ricostruita dalla sola chiave 40. `actor` dichiara
classe, joint nativi, bacino, numero di parti e hash delle due fonti. `part`
dichiara ID, ruolo, SphereUV e overlay metal. Il reader rifiuta ruoli duplicati,
attori o materiali mancanti, overlay senza superficie opaca coincidente e
vertici fuori dall'envelope. Soltanto dopo la verifica completa pubblica
**`original_ship.cockpit_available`**. Con il profilo v1 del pack 06 questo campo
rimane falso; `available` non implica da solo un cockpit completo.

Il profilo del modello nel mondo distingue inoltre l'attore **2047** dal
controller nativo temporaneo **531**. La geometria non prova la proprietà:
la Session richiede la selezione Original del provider verificato. La classe
2047 osservata senza tale contesto non autorizza il volo Original.

I pack 07-final PAL e NTSC importano la stessa geometria Original di provenienza
PAL. Questo non rende il provider Garage compatibile con NTSC: attualmente
il provider rimane PAL-only. ISO, risorse CPU installate e file PAL non sono
modificati dall'estensione. Mesh, texture e ricevute commerciali restano
private, fuori dagli archivi pubblici.

Gli otto test dell'importer dello scafo e gli otto test dei propulsori esistenti
passano 16/16: verificano riferimento chiuso, cardini e tiranti, integrità,
materiali, scala, anelli, immutabilità e rifiuto delle sovrascritture.
Il cockpit ha una ricevuta propria: nove test Python e due suite C++ del
reader, con 295 controlli sintetici e verifica del pack reale, senza warning
di compilazione. La suite integrata del controllo attore del mondo passa 39/39.
Le ricevute sono `work/space-original-crew-audit-081/final-verification.json`
e `final-world-actor-ctest.log`; il confronto geometrico indipendente è
`work/space-original-occupants-qa-081/pack07-native-independent-geometry.json`.
Queste prove non certificano un giro Original completo con il garage. I
tentativi nativi `o2`/`o3` e i risultati parziali sono distinti in
[VELDIN_GARAGE.md](VELDIN_GARAGE.md). Le ricevute precedenti di campagna e audio
mantengono i propri moduli e hash.
