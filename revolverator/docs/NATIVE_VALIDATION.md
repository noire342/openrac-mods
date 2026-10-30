# Preview nativa installabile dal launcher

La combinazione corrente è un singolo pacchetto Revolverator con asset e DLL
utility private. Parte nel client ufficiale NTSC-U invariato usando i livelli
originali; niente script di preparazione, asset pack esterno o card utility.
La ricevuta pubblica `docs/validation/revolverator-0.2.0-native-package.json`
contiene i sei scenari nuovi e gli hash esatti, separati dalle prove precedenti.

| Scenario corrente | Evidenza |
| --- | --- |
| Smoke 900 frame | Solo mod di produzione Revo/XP, nessun helper; 20 cariche e controllo al frame 521 |
| Menu 2.400 frame | 16 armi, griglia nativa 4×5, preferiti 18/65554 e preview/HUD reali; fase 18 terminata |
| XP 1.800 frame | Presa/lancio/impatto/morte: Revo 39 indipendenti, poi Walloper 39; nessun XP imposto |
| Save 2.200 frame | Card isolata reale: 7 snapshot→3 pending→7 reload, CRC/fingerprint validi, Walloper non posseduto |
| Venditore/pickup 2×2.400 frame | 24 controlli nativi superati, capacità 20 e inventari indipendenti |

Le fixture mirate includono probe QA dichiarati nella ricevuta; lo smoke non
li usa. Gli helper non sono dipendenze installabili del pacchetto. Novalis
carica 345 modelli e 61 input originali, incluso l'EXE, restano immutati.

DLL Revo: `ebe140838d24ebb4707b74fc06ad94a935c9ab135c885ad87a78d9f2081fdef5`.
Utility file: `aec87cbfd6eb3133dbb51e62edbce7fae46081355dcd4b5b5aa458876860ca14`.
Utility hook: `077a7306b71ac5f35776b74c261cceb1ff0c1144961a847696eabc961c71d55a`.
Client ufficiale: `b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081`.

I 34 gruppi host Windows/Linux passano, ma restano distinti dalle osservazioni
native. Campagna completa, tutte le boss fight/mod, NG+, ogni slot e sistemi
non supportati non sono certificati. La preview rimane distinta dalla finale.

---

# Verifiche storiche con utility/card configurate manualmente

Il percorso corrente usa il client Windows x64 NTSC-U ufficiale invariato,
commit `77a2f052`, SHA256
`b7f3762aa7076f0d93cad4033b49b4e966975a17885a05d70f61c9fe8df94081`,
con Arsenal Compatibility 0.1.0. La DLL provider è
`077a7306b71ac5f35776b74c261cceb1ff0c1144961a847696eabc961c71d55a`.

## Prime verifiche native sul client invariato

La fixture `qa-vendor-arsenal-stock-pass-01` ha completato 1.800 frame senza
errori nativi, con provider e consumer di pass-through. Sono stati verificati
hash del client, risoluzione/eseguibilità dei 30 target e commit dei 30 detour.
Le chiamate osservate includono NewGame 1, Restore 2, RestoreData 42 e
UpdateState 1.056. Menu, venditore e pickup non sono stati esercitati da questa
fixture: la registrazione di 30 hook non equivale a 30 percorsi eseguiti.
Questa è una prova del confine provider; non certifica gameplay Revolverator,
XP guadagnati, salvataggi completi o resa grafica della nuova combinazione.

## Ricevuta finale nativa sul client stock

La ricevuta pubblica `docs/validation/revolverator-0.2.0-stock-native.json`
registra sei scenari con la DLL Revolverator
`ca5d978e86bcd06e0d253b5c5f01a324bda7192871f72e489626f55e8a88f28c`,
provider `077a7306...c71d55a`, client ufficiale B7F3762A…94081 e, dove attivo,
Weapon XP `68205f76fea997336d4ee01fcb7c2812a2fdd32b6d79d3b67cc51a8b162cb67c`.

| Scenario | Esito osservato |
| --- | --- |
| Trial 720 frame | Avvio incluso, nessun helper; Novalis isolato e 20 cariche |
| Venditore/pickup 2×2.400 frame | 24 controlli, acquisto/rifornimento e Walloper indipendente, cap 20 |
| XP guadagnati 1.800 frame | Presa/lancio/impatto e morte reali: Revo 39 con Walloper invariato, poi Walloper 39 separati; nessun XP assegnato dal probe |
| Salvataggio 2.200 frame | Card isolata reale: snapshot 7, RAM 3 mentre pending, reload 7; CRC/fingerprint validi e Walloper originale ancora non posseduto |
| Menu 2.400 frame | 16 armi, griglia nativa 4×5, preferiti 18/65554; schermate 1080/1560 con trivella assemblata e cartuccia |

I file originali verificati immutati sono 61, compreso il client installato.
I 32 gruppi host Windows/Linux passano; il provider comprende 5.158 controlli
con backend fittizio. Questi test restano distinti dalle prove native sopra.
Non sono certificati campagna completa, tutte le boss fight/mod, NG+, ogni slot,
PAL, altri sistemi operativi o hash del client diversi. La preview pubblica
resta separata dalla versione definitiva.

---

# Verifiche storiche della 0.2 con runtime modificato

R&C1 NTSC-U, Windows x64, runtime locale basato su OpenRAC `77a2f052` con 30 hook
aggiuntivi. Weapon XP opzionale: build privata `0.6.3-arsenal`. Le prove sotto
riguardano fixture isolate; non modificano l'installazione ufficiale. La nuova
capacità è 20. Le prove XP storiche usavano ancora il limite10; acquisti,
pickup e UI sono stati riprovati con20.

| Prova | Evidenza osservata |
| --- | --- |
| Bridge | 30 firme compilate corrispondenti; registrazione di tutti gli hook; 2400 frame fra gioco, Armi e Scelta Rapida |
| Menu aggiuntivo | Walloper 18 e Revolverator 65554 distinti; griglia nativa 4×5 e preferiti logici separati |
| XP guadagnati | Primo Birdbot: cattura 1, lancio 1, impatto 1, morte nativa; Revo 0→39, Walloper fermo a 0 |
| Walloper normale dopo il Revo | Secondo Birdbot: morte nativa; Walloper 0→39, Revo rimasto 39 |
| Munizioni e impostazioni della fixture XP | 10→9 cariche, capacità 10; moltiplicatori danni/munizioni 1 |
| Commercio/pickup nativi con20 | 24 controlli: Hoven15.000→20; refill2→20 costa450; acquisto Walloper7.500 indipendente; pickup4→6, 19→20 e pieno20 invariato |
| HUD e menu finali Novalis | 2400 frame,16 armi nel pannello nativo4×5, icone Walloper/Revo distinte nei preferiti; cartuccia32px nell'HUD20/20+LV1 |
| HUD nei livelli con poca memoria, senza XP | Rilgar (5), Nebula G34 (6) e Quartu (15): 2.400 frame ciascuno, arma impugnata completa a riposo e cartuccia 32px con 19/20; texel e CLUT del frame3 verificati esatti in RAM. Glifo arma 32px nei livelli5/6, 128px nel15; heap HUD residuo rispettivamente 3.456/624/12.304 byte, senza crash o errori nativi di allocazione. DLL `62cbd609ec86b2088888eef4c55b901e17b3107fa90f89e 615b1816047b312cd`; ricevuta `work/arsenal-commerce-tools/packed-hud-native-62cbd-03.json`. Menu e anteprime non esercitati da questi tre smoke test |
| Anteprima rotante | Punta e corpo assemblati a più angoli, sia Armi sia piccolo pannello Scelta Rapida; nessun taglio superiore |
| Autosave duplicato | Richiesta con 7 cariche, RAM portata a 3 e seconda richiesta mentre pending; ricaricamento conserva 7 |
| Consumer XP host | 658 controlli Windows e Linux, anche con ASan/UBSan: attribuzione, expiry, duplicati, frazioni, configurazione e danni opzionali |

## Prova XP riproducibile

La fixture locale `work/qa-vendor-arsenal-earned-xp-01` ha completato 1800 frame con
Revolverator 0.2.0 e Weapon XP 0.6.3-arsenal caricati. Il probe assegna le due armi
tramite API e porta due Birdbot originali a distanza di combattimento. Non
modifica XP, salute dei nemici o valori delle ricompense.

Il primo GetHit accettato porta origine 2017; il secondo combattimento usa il
Walloper nativo con origine 180. Entrambi arrivano a RecordDeath. La ricevuta
`work/arsenal-xp-tools/receipt.json` registra i passaggi 0→39 separati, lo stato
dei plugin e gli hash dei file effettivamente provati. I contatori di impatto da
soli non sarebbero una prova di danno o esperienza guadagnata.

La DLL Revo di quella fixture ha SHA-256
`dd3ae8723a7eb5d0071e7bf0c1321a9a8b29aee0e714c952ead7074a442d537e`;
il runtime ha SHA-256
`fc717cd0526ca1977895f3baf8e86186adcc5bfd41d0d75b5622148f0e689f9b`.
Le ricompilazioni successive richiedono una ricevuta propria per essere
identificate come artefatto finale.

## Combinazione grafica provata

La fixture `work/qa-vendor-arsenal-menu-additive-06` ha concluso2400 frame con
Weapon XP0.6.3-arsenal e Revolverator0.2.0. Gli screenshot960/990/1020 e
1500/1560/1620 mostrano le due anteprime assemblate;2340 mostra l'HUD della
cartuccia dopo la chiusura della ruota. La DLL della fixture ha SHA-256
`402c5e92c215130529a9d3f80c08a232857c33b58f593f258fd671e85814a054`.

La correzione conserva separati rotore impugnato e posa neutra dell'anteprima.
Il renderer HUD usa un frame3 dedicato alla cartuccia, lasciando la trivella
nei frame di inventario e selezione rapida. La regressione host relativa al
contratto nativo60018 comprende63 controlli; la suite Windows completa passa17/17.

## Verifiche aperte

La prova grafica descritta riguarda Novalis nella configurazione della fixture.
Carica, livelli, animazione di acquisizione, tutti i pianeti e formati video
richiedono verifiche della combinazione finale; gli esiti sopra non le sostituiscono.

I test della precedente 0.1 non certificano automaticamente la 0.2. Non sono
coperti una campagna completa, tutte le boss fight, ogni combinazione di mod,
PAL o il comportamento nativo macOS. I test host Linux non sono una prova di
esecuzione del gioco su Linux. La lista operativa è in [QA_CHECKLIST.md](QA_CHECKLIST.md).
