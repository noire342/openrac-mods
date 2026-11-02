# Verifica PAL — anteprima 0.1.0-preview.5, 2 novembre 2026

## Identità

- OpenRAC ufficiale `launcher-v0.10.0`, commit `d015ea1c6925de90b4be91ddb349af83361d9d4b`.
- Eseguibile PAL Windows x64: SHA-256 `e720a59d130b4b2289a4eb15cad3e33f739a72a385992db171ef43490205fb0e`.
- SDK 1.10.1: archivio SHA-256 `5d7455a9c0e7bb6067ee514565cbce9abf12e132f9eb5053d8d3618c16084470`.
- Disco personale PAL SCES_509.16 v2.00: SHA-256 `0f18a6c84cd8d727ec8c21000a236ed5ce2f279cbb8d0682fa9747199ef73260`, verificato con il profilo ufficiale del gioco.
- Classe 87: 493440 byte, 19 sequenze, SHA-256 `5a9c508cff1221e6fccd407d0f210c8ee3bba98a1518186056f1665e55e9e354`.

## Sorgenti e verifiche automatiche

Sei gruppi CTest: regole/input, equipaggiamento, adattatore nativo, traduzione degli stati, distanza/energia/allarme e gestione delle tabelle/disegno delle icone. L'adattatore ripete distacco/rientro sui 19 set di indirizzi e verifica anche il percorso di morte, la rimozione dei componenti, gli errori di creazione e la prenotazione delle risorse nei caricamenti successivi.

Il test del riposizionamento riproduce l'effetto reale della funzione nativa sullo stato del personaggio: il test corretto falliva con la sequenza della prima anteprima. La morte richiede il caricamento nativo del checkpoint, perché lo stato morto rifiuta un semplice reset allo stato idle.

La tabella di dieci funzioni per ciascuno dei 19 overlay viene generata dai sorgenti PAL del commit fissato. Gli audit read-only del disco verificano sette tabelle statiche in tutti i 19 overlay, la risorsa giocabile e i componenti 1146/1204 già presenti nei 19 livelli. Sono distribuiti soltanto indirizzi, dimensioni e digest.

Il driver completo dell'elica usa la routine di superficie del pianeta corrente. Chiamare direttamente quella di Veldin su Aridia produceva una chiamata a uno stato non registrato nell'overlay; il ponte mantiene le regole di superficie native e adatta le richieste di stato prima delle animazioni.

Il blocco mutabile HeroEase è composto da 31 record da 0xB0 byte. Il campo scale del record 25 è a base+0x11DC. Il runtime originale non rimappa sempre questo indirizzo interno: il resolver della mod usa la base verificata del livello mentre controlla il proprio Clank. Non si applica un checksum ai dati live mutabili.

## Esecuzioni native isolate

Tutte le prove usano il programma PAL ufficiale, Vulkan, cartelle e schede vuote separate, `--no-card`, ISO personale in sola lettura. I salvataggi e l'installazione attiva dell'utente non sono stati usati. Il plugin di telemetria non altera gli input; le eventuali fixture di posizione sono attivate esplicitamente e annotate nel log.

- Aridia: distacco, movimento, salto, attacco Quadrato e planata; stati nativi Clank e indici di sequenza sempre inferiori a 19 nei campioni registrati.
- Attacco reale: cassa bulloni classe 500 presente prima del colpo e distrutta dopo il colpo; contatore bulloni da 1 a 7. L'osservatore SDK dei colpi non riceve eventi per questo percorso, quindi la verifica usa il risultato nativo e le immagini.
- Fango reale: fixture di posizione nel punto della segnalazione, poi collisione/terreno nativi. Richiesta 0x68 adattata a 0x7D prima della lettura dell'animazione; ritorno al checkpoint e successivi distacchi/rientri. Stato finale Ratchet idle, salute 4, controllo 0.
- Elica: durante 100 frame consecutivi di planata, componente 1146 con sequenza 1 e avanzamento dei frame 0..29. Dopo la planata torna alla sequenza ripiegata; dopo il riaggancio entrambi i puntatori elica/luce sono zero. Ispezione dello screenshot durante la planata conferma posizione e aspetto dell'elica.
- Novalis: prova separata del controllo e del riaggancio; verifica del caricamento della classe aggiunta.

## Copertura

Le fixture di posizione isolano l'azione da verificare e non rappresentano una percorrenza manuale della campagna. Non sono certificati tutti i pianeti, tutte le missioni, i salvataggi durante il distacco o tutte le combinazioni di mod. Il pacchetto rimane un'anteprima locale e non viene inserito nel catalogo pubblico stabile.

La ricevuta esterna al pacchetto contiene i digest degli archivi, della DLL esatta e dei log finali utilizzati.

## Ruota Gadgebot e riserva

La prima prova di Triangolo su Aridia produceva tre errori di callback non registrata (0x2394A8, 0x239510, 0x239918). Il resolver ora collega queste richieste ai tre callback originali esclusivamente mentre la mod possiede Clank. La ruota entra nella fase selezionabile e il rilascio scrive il comando nel registro nativo. Il riaggancio conserva la precedenza entro il raggio di 2 unità. Non vengono creati Gadgebot su pianeti che non li contengono.

L'icona originale 0x7553 non è residente su tutti i pianeti. Il lettore estrae dal disco personale otto immagini 32x32 e la palette (9216 byte complessivi, SHA-256 d88546c80e4676073b7f23fc6311a906f3f8aca7b6333b823a76a1907351a407). HudAssets aggiunge un record alle definizioni e otto riferimenti, conservando tutte le voci esistenti. I puntatori vengono ripristinati prima del caricamento; i test verificano confini, ordine palette/immagini, caricamenti ripetuti e ripristino. I pixel non sono distribuiti. L'audit verify_pal_hud.py verifica i 19 overlay, le due tabelle dei comandi e il bundle originale.

La riserva usa lo slot nativo del contatore, con identificativo privato 0x7FFD e un contatore separato nella prenotazione della mod. L'ossigeno di Ratchet non viene modificato. Le prime due immagini del widget riferiscono la cornice originale 0x753F; la terza riferisce sempre la particella elettrica, anche senza l'intercettazione della fase rossa. La rimozione nativa cambia il seriale prima della fine dell'animazione: la proprietà del disegno dipende quindi dal tipo privato e dall'epoca, non dal seriale. Il ponte di risoluzione rimane attivo per il solo widget in uscita anche dopo il riaggancio a Ratchet.

Autonomia 0 usa la divisione intera nativa 10000 / (scale_ticks(60) * 15). Le durate di 8 secondi di alcune fixture accelerano soltanto la verifica dell'allarme; i relativi parametri sono nella ricevuta. Il valore pubblico è 0 e la meccanica è disabilitata di default.

## Correzioni preview.5

La precedente preview.4 ricostruiva l'interno con un fondo piatto e una scarica a bassa risoluzione. Ora il disegno originale conserva vetro, riflessi, animazione e distribuzione delle particelle; soltanto le immagini delle particelle vengono sostituite con due scariche 64x64 generate analiticamente. Palette e immagini richiedono 9216 byte, senza pixel del disco distribuiti. La cornice è un alias delle immagini originali, non una copia. La prenotazione include un contatore e 32 byte per la definizione audio privata e rispetta anche la memoria ridotta di Novalis.

Su Aridia la definizione originale della classe Ratchet per la voce 0xB contiene indice -1: una prenotazione della voce non prova quindi che il suono venga riprodotto. Il lettore verifica sul disco personale il banco di Novalis (SHA-256 2330fb88665d0ba255cf7359f7208be0cd433a86926a90320b90b98c671bc8dd) e la definizione originale (SHA-256 970fd8af9e75a5718c72c55de36626729c30d49458a85d295475752dc68cdcc2), risolve la voce 31 e carica il banco direttamente tramite snd_BankLoadByLoc. Il banco occupa memoria dell'engine audio sul PC, senza copiare 1,7 MB nella RAM retail. La definizione privata usa il banco restituito e il percorso non posizionale 0x11, volume 0x400. Le voci appartenenti alla definizione vengono fermate prima del rilascio del banco.

I test riproducono il cambio di seriale alla rimozione mantenendo la texture privata, verificano l'assenza di modifiche al contatore originale, la riserva piena, il renderer normale dei widget del gioco, il primo allarme a metà della fase rossa, la deduplicazione e il ciclo di caricamento/rilascio del banco. Le prove native distinguono gli handle reali del suono riprodotto dalla sola prenotazione in stato 7. La verifica WASAPI usa il loopback dell'uscita, senza microfono, senza cambiare volume/mute e con una baseline separata; non prova l'ascolto acustico da casse o cuffie.

Le prove restano isolate, con schede vuote e nessun salvataggio personale usato. L'esito finale e i digest della DLL effettivamente provata sono nella ricevuta preview.5; i risultati delle candidate precedenti non sono sostitutivi della verifica finale.

La chiusura del processo può chiamare l'unload delle mod quando i servizi grafici sono già stati distrutti. Il riaggancio nativo rimane riservato alle transizioni durante il gioco; nell'unload terminale si abbandonano soltanto le lease e si rimuovono gli hook. Non vengono richiamati servizi di attori, figure, HUD o audio già terminati. Il test di teardown rende indisponibili tali servizi prima della distruzione; una prova nativa separata chiude il processo mentre Clank è ancora controllato.
