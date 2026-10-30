# Revolverator 0.2.0

Ricostruzione del Revolverator rimosso da Ratchet & Clank: colpo ravvicinato,
presa, sollevamento, rotazione e lancio del nemico. Il trapano originale viene
ricostruito dagli asset della propria copia NTSC-U del gioco.

**Sorgenti di sviluppo per OpenRAC su Windows, R&C1 NTSC-U; anteprima, non versione definitiva.** La 0.2 aggiunge
il Revolverator mantenendo il Walloper originale. Combattimento, XP separati,
commercio e salvataggi hanno prove native; modello, anteprime e HUD sono stati
verificati su Novalis nella combinazione riportata in NATIVE_VALIDATION.md.
Non è una dichiarazione di compatibilità PAL o di campagna completa.

## Installazione della preview

Nel launcher ufficiale OpenRAC, pagina Mods, aggiungi questo indirizzo:
[revolverator-v0.2.0-preview.1](https://github.com/noire342/openrac-mods/releases/tag/revolverator-v0.2.0-preview.1).
Seleziona Revolverator, installa, abilita e avvia normalmente il gioco NTSC-U.
Per provarla subito attiva **Revolverator sbloccato da subito** nelle impostazioni.

L'archivio nativo è `revolverator-0.2.0-windows-x86_64.zip`, importabile anche
come ZIP locale. Asset e utility sono nello stesso pacchetto: non servono
card di supporto separate, script esterni o un client sostitutivo. L'indirizzo
esplicito della release seleziona la preview; l'URL generico della repository
segue le release stabili. L'indice espone solo Weapon Experience e Revolverator.

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

Il toggle **Revolverator sbloccato da subito** (`start_unlocked`) è disattivato
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

## Requisiti della 0.2

- Plugin nativo compilato con SDK OpenRAC 1.3, ABI richiesta 1.1.
- Client ufficiale OpenRAC Windows x64 NTSC-U invariato: commit `77a2f052`,
  SHA256 `b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081`.
- Utility compatibilità e file alias incluse privatamente nel pacchetto:
  caricamento automatico dalla cartella DLL della mod; niente card aggiuntive.
  I 30 hook restano una transazione tipizzata completa, mentre l'SDK è invariato.
- Asset del trapano e della cartuccia inclusi nello ZIP nativo; la ricostruzione
  locale serve soltanto per una build dai sorgenti.
- Weapon XP API v4 soltanto se si vuole la progressione opzionale.

La consegna 0.2 è un normale pacchetto nativo del launcher: legge i manifest
originali, crea copie temporanee proprie e carica gli asset inclusi tramite
utility private. La ricevuta corrente copre sei scenari con questa installazione;
le prove sul vecchio runtime o sulle card di supporto restano storiche.
ISO, salvataggi e RAM non fanno parte dei sorgenti pubblici.

## Documentazione

- [Architettura del menu e identità](docs/WEAPON_MENU_AUDIT.md)
- [API v2 per altre mod](docs/API.md)
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
