# Verifiche prima della consegna

Questa è la lista di regressione completa per DLL, pack del trapano ed estensione
delle munizioni. Non è una ricevuta di passaggio di ogni scenario: gli esiti
verificati della preview sono documentati in [Validation](../../docs/VALIDATION.md#revolverator-source-preview).
I casi non coperti non vanno dedotti da un test host o da una singola immagine.

## Build e asset

- [ ] Compilare la DLL finale e passare tutti i gruppi CTest della build principale.
- [ ] Registrare hash DLL, manifest del plugin, `rotor.json` e inventario degli asset.
- [ ] Passare `model_rotor_test.py`: 19 modelli, corpo 1338 vertici sul giunto 0,
      punta 208 sul giunto 4, nessun triangolo misto o istanza mesh duplicata.
- [ ] Passare `ammunition_model_test.py`: una sola cartuccia energetica, 19 modelli,
      21 manifest e alias 2016/32000 con la scala nativa 225.
- [ ] Eseguire check e stacking standard del pack del trapano senza modelli saltati.
- [ ] Confrontare gli hash dei manifest base prima e dopo la generazione.
- [ ] Estrarre il bundle privato e provare `Start-Revolverator.ps1 -PrepareOnly`:
      `started:false`, nessuna modifica ai dati installati, nessun salvataggio copiato.

## Caricamento nativo e inventario

- [ ] Il log della build installata mostra `revolverator` caricato, senza richieste
      a hook opzionali assenti e senza altri plugin falliti come effetto collaterale.
- [ ] Prima della visita a Hoven una nuova partita non vede l'arma nello stock.
- [ ] La disponibilità della mappa da sola non equivale alla visita di Hoven.
- [ ] Dopo la visita, acquistare l'arma a 15.000 bolt e verificare il possesso nativo.
- [ ] Un vecchio salvataggio con Walloper conserva il possesso dello slot 18.
- [ ] L'acquisto iniziale, quando nativamente applicabile, dà capacità10; cambiare
      livello o aprire un menu non ricarica, duplica o azzera munizioni.
- [ ] Salvataggio e caricamento conservano acquisto e quantità. Usare una copia di
      prova per questa verifica e registrare il percorso della copia.

## Combattimento e interruzioni

- [ ] Un tap produce il colpo previsto e consuma una sola carica.
- [ ] Un hold continuo prende un bersaglio ammesso; un tap o il rilascio anticipato
      non cattura accidentalmente un altro nemico.
- [ ] Colpire casse bolt, nanotech, munizioni ed esplosive produce il loro esito
      nativo. Le casse rinforzate mantengono la resistenza prevista dal gioco.
- [ ] Un nemico lanciato può colpire altri bersagli e casse tramite il danno nativo.
- [ ] Nemici non ammessi, NPC e boss non vengono presi; un ostacolo interrompe la presa.
- [ ] Nessun arco o aura elettrica del Walloper appare sul colpo del Revolverator.
- [ ] Il nemico resta agganciato nel punto colpito, con inclinazione conservata
      durante sollevamento e rotazione; non è sempre appoggiato in piedi sulla punta.
- [ ] Rilascio e impatto non lasciano Ratchet in attesa del ritorno della chiave.
- [ ] Pausa congela presa, rotore e animazione a vuoto; riprendere non duplica il lancio.
- [ ] Cambio arma, morte, cutscene, cambio livello e ricaricamento dello stesso
      livello ripristinano AI/collisioni e rimuovono solo i modificatori posseduti.
- [ ] Una seconda cattura non usa un vecchio token, uno scheletro liberato o un
      riferimento al bersaglio della precedente cattura.

## Posa, contatto e rotore

- [ ] Da fermo e camminando, gomito e mano passano all'esterno della testa.
- [ ] Il sollevamento precede la rotazione naturale del polso; l'impugnatura resta
      nella mano e la trivella tende verso l'alto senza rotazioni dell'intero corpo.
- [ ] Verificare il contatto effettivo fra punta e punto di presa da davanti,
      dietro e di lato, anche durante il passaggio di angolo attraverso ±pi.
- [ ] La trivella resta visibile mentre il nemico ruota e mentre si sposta la camera.
- [ ] Solo la punta a spirale gira: motore, collare e impugnatura non seguono il rotore.
- [ ] Con munizioni il minimo è lento; senza munizioni la punta è ferma.
- [ ] Durante la presa, velocità e verso angolare della punta corrispondono al nemico.
- [ ] Con zero cariche Cerchio dà un breve scatto avanti e ritorna allo stesso
      angolo, senza rotazione residua o consumo negativo.
- [ ] Ripetere prese e cambi arma senza esaurire l'heap del livello. Controllare
      l'effettiva installazione dei modificatori, non soltanto lo stato del controller.

## Munizioni, venditore e HUD

- [ ] Partendo da 10 cariche, dieci attivazioni valide portano a 0, senza doppi consumi.
- [ ] Raccogliere una cartuccia dà 2 cariche, limitate a 10; a pieno carico la raccolta
      mantiene il comportamento nativo e non altera munizioni di altre armi.
- [ ] Il pickup visibile è la singola cartuccia energetica, con fisica e raccolta native.
- [ ] Comprare una carica costa 25 bolt; ricariche multiple addebitano il totale corretto.
- [ ] Nel menu Gadgetron l'anteprima delle munizioni usa la cartuccia alias 2016,
      senza modificare l'anteprima dell'arma o le munizioni del Guanto Mina.
- [ ] Il simbolo scelto `59901:13`, contatore, cornice, colori, comparsa e dissolvenza
      restano nel riquadro nativo, senza pannelli aggiuntivi.
- [ ] La barra di carica compare solo durante la presa e non copre il contatore o
      l'eventuale riga Weapon XP. Provare HUD acceso/spento dalla configurazione.
- [ ] Con header HD, la texture 128×128 mantiene footprint logico 32×32 in HUD,
      inventario e quick select; senza header funziona il fallback nativo.
- [ ] Provare 4:3, 16:9 e 16:10, pause/menu e cambio livello. Non basta un solo fermo immagine.

## Compatibilità e consegna

- [ ] Provare separatamente la mod da sola e con Weapon XP caricato prima; registrare
      le impostazioni effettive e controllare progressione, nomi e danni nativi.
- [ ] Disabilitando entrambi i componenti nella sessione di prova, lo slot torna
      al Walloper senza record di menu, alias o puntatori residui.
- [ ] Il bundle privato contiene solo libreria, pack previsto e asset propri;
      nessuna ISO, RAM, copia di campagna o manifest base viene distribuita.
- [ ] Non dichiarare PAL, copertura della campagna completa o release pubblica
      sulla base delle sole prove NTSC-U di Novalis e Hoven.
- [ ] Allegare all'eventuale consegna finale i soli esiti verificati e le limitazioni
      rimaste, usando hash e percorsi degli artefatti effettivamente provati.

Gli helper locali `work/revolverator-tools/start_qa.ps1` e il probe non sono parte
della mod destinata alla partita normale. Per le prove usare esplicitamente
`-Graphics .../revolverator-model-02 -Levels .../revolverator-ammo-levels-03/levels`
per rendere gli asset provati espliciti nel comando e nel registro della sessione.
