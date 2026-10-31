# Revolverator 0.3.1 — Rotatore

La versione di sviluppo 0.3.1 usa **OpenRAC 0.10 su Windows e PAL Europe v2.00
(`SCES_509.16`) come riferimento**, mantenendo l'adattatore NTSC-U. Le prove
specifiche della migrazione sono descritte in [PAL](docs/PAL.md). Aggiorna anche
il caricamento degli asset al formato 4: dopo l'aggiornamento di OpenRAC usa
**Check and play** nel launcher per rileggere i livelli, se richiesto. Il
pacchetto non contiene un client modificato e non altera l'eseguibile.

La ripresa dopo il lancio richiede al massimo **150 ms**, con Cerchio memorizzato
durante il recupero e fino a otto nemici in volo indipendenti. Ratchet accompagna
il lancio con un gesto di 250 ms, interrotto dalla pressione successiva.

Su entrambe le versioni, un tap di Cerchio produce un impulso di **160 ms**:
la sola testa della trivella raggiunge **2× in larghezza, 1,2× lungo l'asse e
55 rad/s**, con un breve rientro. L'impulso rimane visibile anche con un tap di
un solo frame. Appena il nemico inizia a roteare, la testa torna subito alla
scala normale e segue la sua rotazione. Il tentativo senza munizioni conserva
il breve scatto a vuoto.

La vetrina Gold è ingrandita, centrata e ferma, con l'illuminazione delle
cupole originali. Acquisto Gold e aumenti di livello usano l'animazione nativa
del Colpitore con nome e modello Rotatore, anche dorato; il presenter non
richiede Weapon XP per l'acquisto. L'acquisto base conserva la scena del negozio.

Prove della 0.3.1: [compatibilita e verifica nativa](docs/OPENRAC_0_10.md).

La 0.3 aggiunge il **Rotatore d'oro**: una cupola in piu nel negozio Gold di
Novalis, **50.000 bolt normali e zero bolt d'oro**, stesso modello e finitura
dorata ispirata alle armi Gold originali. L'upgrade conserva munizioni, XP e
Scelta Rapida. Il nemico lanciato esplode all'impatto: il raggio cresce con la
rotazione fino al lancio automatico dopo tre secondi. Ogni lancio ha una breve
pausa di150ms nella locomozione di Ratchet per dare peso al gesto.

Le precedenti revisioni sono state verificate sul client ufficiale Windows NTSC-U.
Le prove della 0.3 su OpenRAC 0.9 restano separate dalle nuove prove su 0.10.
Dettagli: [upgrade e salvataggi](docs/GOLDEN_UPGRADE.md),
[esplosione e controlli](docs/GOLDEN_EFFECTS.md).

Ricostruzione del Revolverator rimosso da Ratchet & Clank: colpo ravvicinato,
presa, sollevamento, rotazione e lancio del nemico. Il trapano originale viene
ricostruito dagli asset della propria copia PAL o NTSC-U del gioco.

**Sorgenti di sviluppo per OpenRAC su Windows; anteprima, non versione definitiva.** La 0.2 aggiunge
il Revolverator mantenendo il Walloper originale. Combattimento, XP separati,
commercio e salvataggi hanno prove native; modello, anteprime e HUD sono stati
verificati su Novalis nella combinazione riportata in NATIVE_VALIDATION.md.
Non è una dichiarazione di compatibilità PAL o di campagna completa.

## Installazione della preview

Nel launcher ufficiale OpenRAC, pagina Mods, aggiungi
[questa repository](https://github.com/noire342/openrac-mods), seleziona
Revolverator e installalo. In alternativa importa con **File** lo ZIP
`revolverator-0.3.1-windows-x86_64.zip`. Abilita la mod e avvia
la regione supportata della tua copia del gioco.
Per provarla subito attiva **Rotatore sbloccato da subito** nelle impostazioni
(**Revolverator sbloccato da subito** nella precedente0.2).

Asset e utility sono nello stesso pacchetto: non servono
card di supporto separate, script esterni o un client sostitutivo. L'indirizzo
esplicito di una release seleziona quella versione. La 0.3.1 è pubblicata sul
canale normale, quindi è disponibile anche dall'URL generico della repository.

## Armi e menu

| Identità | Chiave per le API | Ruolo |
| --- | ---: | --- |
| Walloper originale | `18` | Arma nativa, con possesso e funzionamento propri |
| Revolverator | `65554` (`0x10012`) | Arma aggiuntiva, con munizioni e XP indipendenti |

Il Revolverator usa temporaneamente il trasporto nativo 18 nelle operazioni del
gioco che lo richiedono. La sua chiave logica non viene usata come indice negli
array nativi. Possedere il Walloper non assegna automaticamente il Revolverator.

Il menu Armi e l'editor Scelta Rapida usano i pannelli nativi con una griglia di
**4 righe × 5 colonne**, fino a 20 celle visibili e scorrimento oltre 20. Gli otto
preferiti rimangono quelli originali: Walloper e Revolverator possono occupare
due posizioni distinte anche se condividono il trasporto interno. Il registro
prepara l'aggiunta di altre armi; l'API v2 attuale non espone una registrazione
arbitraria da parte di altre mod.

## Bilanciamento e controlli

| Voce | Valore base |
| --- | ---: |
| Prima disponibilità | Visita di Hoven |
| Prezzo standard | 15.000 bolt |
| Capacità | 20 cariche |
| Consumo | 1 carica per attivazione |
| Pickup normale, se posseduto | 2 cariche, fino a 20 |
| Prezzo per carica dal venditore | 25 bolt |

Il toggle **Rotatore sbloccato da subito** (`start_unlocked`) è disattivato
di default. Se attivo, assegna ed equipaggia l'arma con 20 cariche alla prima
fase giocabile soltanto se non è già posseduta. Cambiare livello, ricaricare
il salvataggio o riavviare non rifornisce un'arma già posseduta; disattivare il
toggle conserva arma e progressi acquisiti.

Premi brevemente Cerchio per il colpo ravvicinato. Per la presa serve una
pressione continua di almeno 0,18 secondi; rilascia per lanciare. I boss e i
bersagli non ammessi alla presa conservano la risposta ravvicinata nativa.
La punta gira lentamente a riposo, segue la rotazione del nemico durante la
presa e si ferma senza cariche. Il tentativo a vuoto produce un breve scatto.

La cartuccia è una singola cella energetica industriale. Modello e texture sono
stati creati con assistenza AI; la mesh viene generata dalla ricetta del
progetto. Nell'HUD durante l'uso compare la cartuccia delle munizioni. La trivella resta
l'icona di inventario e scelta rapida, dal simbolo nativo `59901:13` ingrandito con
[Real-ESRGAN ncnn Vulkan](https://github.com/xinntao/Real-ESRGAN-ncnn-vulkan).
La combinazione grafica verificata e le prove ancora aperte sono elencate in
[NATIVE_VALIDATION.md](docs/NATIVE_VALIDATION.md).

## Salvataggi e Weapon XP

Il file nativo conserva Walloper e ID originali. Le associazioni rapide custom
vengono rimosse dalla rappresentazione nativa durante la serializzazione e
ripristinate subito in RAM. Il Revolverator viene salvato in un sidecar
`saveN.bin.arsenal`, scritto atomicamente e associato all'impronta del preciso
salvataggio nativo di quello slot. Un sidecar assente, danneggiato o riferito a
un'altra versione del salvataggio non viene applicato.

La mod funziona anche senza Weapon XP. Per la progressione opzionale occorre
un provider con **API v4**; il companion corrispondente è
`weapon-xp 0.6.3-arsenal`. I provider precedenti che non possono escludere i
colpi custom vengono rifiutati chiaramente. Gli XP seguono i bolt nativi del
nemico e le regole configurate del trasporto 18, mantenendo un totale separato.
Il limite è livello 10; danni e munizioni aggiuntivi restano disattivati di default.

## Requisiti della 0.3.1

- OpenRAC ufficiale **0.10**, Windows x64, R&C1 NTSC-U, con livelli nel
  **formato 4**. Usa **Check and play** per aggiornare un'estrazione precedente.
- EXE verificato: SHA256
  `f70a9f2bc6de39ed42b9f9f64fc5cea98ffad8b8bd7795c7adfb965a9875c95b`.
- ABI richiesta dal plugin **1.1**. La tabella SDK **1.10** conserva il prefisso
  usato dalla mod e le firme dei suoi 27 hook pubblici.
- Utility compatibilità e file alias incluse privatamente nel pacchetto:
  caricamento automatico dalla cartella DLL della mod; niente card aggiuntive.
  I 30 hook privati restano necessari e vengono attivati come transazione
  tipizzata sul client riconosciuto, senza sostituire l'EXE.
- Asset del trapano e della cartuccia inclusi nello ZIP nativo; la ricostruzione
  locale serve soltanto per una build dai sorgenti.
- Weapon XP API v4 soltanto se si vuole la progressione opzionale.

La mod legge i manifest originali, crea copie temporanee proprie e carica gli
asset inclusi tramite utility private. Conserva il formato 3 o 4 del manifest;
su un runtime con API 1.10 richiede il formato 4 prima di montare gli asset.
L'hash del precedente client 0.9 `b7f3762a…df94081` rimane riconosciuto;
le nuove prove della DLL 0.3.1 riguardano la 0.10. Le ricevute della 0.2 e 0.3
su 0.9 restano storiche e non sostituiscono queste prove.
ISO, salvataggi e RAM non fanno parte dei sorgenti pubblici.

## Documentazione

- [Architettura del menu e identità](docs/WEAPON_MENU_AUDIT.md)
- [API v3 per altre mod](docs/API.md)
- [OpenRAC 0.10, requisiti e prove della 0.3.1](docs/OPENRAC_0_10.md)
- [Animazione nativa di ottenimento](docs/NATIVE_ACQUISITION.md)
- [Verifiche Golden 0.3](docs/GOLDEN_VALIDATION.md)
- [Build e asset locali](docs/BUILD_AND_ASSETS.md)
- [Prove native e limiti](docs/NATIVE_VALIDATION.md)
- [Verifiche ancora aperte](docs/QA_CHECKLIST.md)
- [Packaging e provenienza](docs/PACKAGING.md)
- [Icona HD](docs/HD_ICON_INTEGRATION.md)
- [Localizzazione](docs/LOCALIZATION.md)

## Licenza

Il codice della mod scritto per questo progetto è distribuito con licenza
[GPL-3.0-or-later](LICENSE), Copyright (c) 2026 noire342. I notice di terze parti restano
validi. La licenza del codice non copre modelli, texture, icone o altri asset
derivati dal gioco; il runtime OpenRAC e la relativa patch mantengono le loro
licenze GPL e gli altri notice upstream.

La variante di texture Golden e stata preparata con AI partendo dall'atlante
originale e dalla palette del Blaster Gold; geometria e coordinate UV del
modello sono invariate. Il modello della munizione e stato generato con AI,
come riportato nella sua documentazione di provenienza.
