# Archivio NTSC — prima anteprima ritirata

La prima anteprima aveva i difetti di animazione e crash successivamente segnalati. Questa pagina è storica e non certifica la versione PAL corrente. Vedere `validation-pal-0.10.0.md`.

# Verifica del 31 ottobre 2026

## Ambiente

- Windows x64, OpenRAC ufficiale `launcher-v0.10.0`, commit `d015ea1c6925de90b4be91ddb349af83361d9d4b`.
- SHA-256 dell'eseguibile NTSC-U: `f70a9f2bc6de39ed42b9f9f64fc5cea98ffad8b8bd7795c7adfb965a9875c95b`.
- SDK 1.10.1, SHA-256 archivio: `5d7455a9c0e7bb6067ee514565cbce9abf12e132f9eb5053d8d3618c16084470`.
- Risorsa Clank 87 letta dall'ISO personale: 493440 byte, SHA-256 `5a9c508cff1221e6fccd407d0f210c8ee3bba98a1518186056f1665e55e9e354`.
- Profilo di prova isolato, schede separate vuote, `--no-card`, rendering Vulkan. Nessun salvataggio personale usato.

## Test automatici

Tre gruppi CTest: regole di controllo e input; equipaggiamento del Ratchet fermo; adattatore nativo con API simulate. L'ultimo ripete tre cicli per ognuno dei 19 overlay e verifica rifiuto dei servizi mancanti, errore di creazione, rimozione degli attori e mancato accesso a indirizzi scaduti. Il generatore verifica gli indirizzi contro la tabella del commit ufficiale.

La regressione della schiena riguarda l'intero slot 3, comprese entrambe le parti, indipendentemente dalla classe dell'Helipack o dei suoi aggiornamenti. Gli elementi degli altri slot e la copia originale restano identici. Una parte di Clank nascosta o assente non autorizza il distacco.

## Prova nativa

Novalis, eseguibile ufficiale non modificato:

1. Distacco con L3 + Triangolo; controllo del modello giocabile nativo di Clank.
2. Ispezione visiva: Ratchet fermo ha la schiena libera; Clank è separato.
3. Movimento, salto e ritorno vicino a Ratchet.
4. Triangolo a circa 3.46 unità lascia il controllo a Clank; entro 2 unità riaggancia.
5. Il rientro riporta Ratchet al punto iniziale e ripristina Clank sulla schiena.
6. Secondo ciclo di distacco/rientro; chiusura del processo senza crash.

Traccia locale `qa-backpack-final`: posizione iniziale di Ratchet `(162.5303,136.3935,60)`, Clank arriva a `(166.3484,136.9709,60)`, salto fino a `z=61.0495` al frame relativo 245, Ratchet torna alla posizione esatta iniziale dopo il rientro. Frame 690: Clank in movimento e schiena di Ratchet libera. Frame 960: riaggancio completato. La prova precedente `qa-backpack-fixed` verifica anche la chiusura mentre Clank è ancora staccato, con ripristino registrato nel log.

I file di prova, gli screenshot e l'ISO restano fuori dal pacchetto installabile.

## Copertura ancora aperta

La campagna completa e gli altri pianeti non sono certificati da questa prova su Novalis. Restano da verificare salvataggi durante il distacco, morte, transizioni della storia e combinazioni con altre mod. Per questo il pacchetto rimane un'anteprima e non viene aggiunto al catalogo pubblico stabile.
