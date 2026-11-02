# Clank libero — PAL 0.1.0-preview.5

Mod separata per Ratchet & Clank (2002), **PAL SCES_509.16 v2.00**, Windows x64 e OpenRAC **0.10.0 ufficiale**.

- **L3 + Triangolo**: stacca Clank quando Ratchet è vivo, a terra e lo porta sulla schiena.
- **Triangolo tenuto premuto, lontano da Ratchet**: ruota originale dei Gadgebot; seleziona con stick o direzioni e rilascia per impartire il comando. I comandi agiscono sui Gadgebot effettivamente presenti nel livello.
- **Quadrato**: attacco corpo a corpo originale di Clank.
- **Croce**: salto; una seconda pressione in aria avvia la planata nativa.
- Durante la planata compare e gira l'elica originale; al termine torna ripiegata.
- **Triangolo entro 2 unità da Ratchet**: riaggancio e ritorno al controllo di Ratchet.

Ratchet resta nel punto del distacco, con la schiena libera. Camera, animazioni e componenti del personaggio sono quelli del gioco. L'HUD mantiene posizione, cornice e animazioni native; la riserva opzionale di Clank mostra elettricità, senza bolle e senza pannelli aggiuntivi. Alla morte di Clank il gioco esegue il normale ritorno al checkpoint con Ratchet.

## Installazione

Importa lo ZIP nel launcher, abilita **Clank libero** per la versione PAL e riavvia il gioco. Servono l'ISO personale della revisione supportata e tutti i livelli PAL estratti in formato 4. Il pacchetto verifica l'eseguibile e le risorse prima di attivare il controllo.

Questa anteprima sostituisce la precedente prova NTSC. Le prove native riguardano Aridia e Novalis; campagna completa, salvataggi durante il distacco e combinazioni con altre mod restano da verificare. Le verifiche automatiche dei 19 adattatori non certificano tutta la campagna.

## Distanza ed energia (opzionale)

Nelle impostazioni della mod, **Energia lontano da Ratchet** è disattivato di default. Quando lo abiliti, oltre la distanza scelta (25 unità di default) compare la riserva elettrica. A zero il gioco torna al checkpoint. Rientrando nel raggio si ricarica in 3 secondi; in pausa non si consuma. Una piccola isteresi evita oscillazioni sul confine del raggio.

**Autonomia = 0** mantiene esattamente il consumo nativo di Ratchet. Un valore positivo sceglie una durata personalizzata. Vetro, riflessi, cornice, posizionamento, movimento delle particelle e animazioni di ingresso/uscita sono originali. Due immagini elettriche 64x64, con nucleo luminoso e alone sfumato, sostituiscono solo le bolle: ciano durante il consumo normale, rosse durante l'allarme. La cella usa un identificativo privato e un proprio contatore; non modifica l'ossigeno di Ratchet. Il disegno resta elettrico anche durante l'uscita a carica piena, quando il gioco cambia il seriale del widget.

L'allarme usa l'audio originale di Ratchet sott'acqua. Alcuni pianeti, tra cui Aridia, non ne caricano il campione: la mod legge il banco originale di Novalis dal disco personale e ne usa la voce 31, con una definizione privata e riproduzione da HUD, senza attenuazione per distanza o ostacoli. Il banco è caricato solo quando serve ed è rilasciato al cambio di livello o all'uscita. Nessun audio originale viene incluso nello ZIP.

Le chiavi di configurazione `distance_oxygen` e `oxygen_seconds` mantengono i nomi precedenti per conservare le impostazioni salvate. Il nome mostrato nel launcher è **Energia**.

La ruota include le otto immagini originali del comando 0x7553 anche sui pianeti che non le caricano normalmente. Vengono lette dall'ISO personale e aggiunte alle tabelle della sessione: nessuna immagine di gioco è distribuita nello ZIP, e le altre icone restano invariate.

## Correzioni rispetto alla prima anteprima

Il cambio personaggio segue l'ordine nativo: riposizionamento, poi attivazione di Clank. Il suo controller completo gestisce movimento, transizioni, attacchi, danni, collisioni e componenti visivi. Le richieste di nuoto/affondamento incompatibili vengono adattate prima della lettura delle animazioni, e la morte segue la procedura nativa del checkpoint.

L'elica usa il componente originale 1146, la sua animazione di planata e l'aggancio allo scheletro di Clank. Elica e luce vengono eliminati dall'uscita nativa completa quando si torna a Ratchet. Il caricamento dopo una morte conserva la memoria riservata alla classe aggiunta, senza registrarla ripetutamente.

## Architettura e compatibilità

`Controller` dipende da `IWorld` e contiene le regole di distacco/rientro. `Input` riconosce i fronti dei pulsanti. `NativeWorld` possiede figura, attore e prenotazioni della memoria. `NativeControl` adatta esclusivamente il Clank posseduto dalla mod alle routine native complete. `Tether` contiene soltanto la regola di distanza/consumo. `NativeHud` gestisce i widget e adatta i loro callback originali; `AlarmSound` possiede il banco e la definizione privata dell'allarme. `EnergyArtwork` genera le immagini delle scariche; `HudAssets` aggiunge solo queste e le icone mancanti, con ripristino dei puntatori e prenotazioni controllate. `Resources` conserva i dati immutabili e gli alias di lettura dei manifest per la sessione. Il profilo PAL è raccolto in `compatibility.json`; CMake genera i parametri condivisi, e il generatore verifica gli indirizzi contro il commit ufficiale.

Gli interventi sulle routine in memoria sono limitati al processo del gioco e al personaggio posseduto. I file originali e l'ISO restano invariati. La mod non distribuisce dati del disco: legge le risorse originali dall'installazione personale. Le immagini elettriche sono generate dal codice della mod e non sostituiscono le icone esistenti nelle tabelle. I componenti dell'elica già presenti nei singoli livelli mantengono i rispettivi materiali.

## Compilazione

Visual Studio con C++, CMake e SDK OpenRAC **1.10.1**, bloccato in `sdk.lock`:

```powershell
python tools/fetch_sdk.py
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

La cartella installabile è `build/package/clank_unleashed`. Le dipendenze private in `third_party` sono congelate e inventariate in `third_party/SNAPSHOT.json`, così aggiornamenti alle altre mod non cambiano questa compilazione. Le fixture native si compilano soltanto con `-DCLANK_BUILD_QA=ON` e il relativo plugin non viene distribuito nello ZIP installabile.

```powershell
python tools/generate_native_addresses.py C:/path/OpenRAC --check
python tools/verify_pal_tables.py "C:/path/disc.iso" --json pal-audit.json
python tools/verify_pal_companions.py --help
```

Prove e limiti: `docs/validation-pal-0.10.0.md`. Attribuzioni: `NOTICE.md` nei sorgenti e `COPYING-ThirdParty.md`, `LICENSE`, `LICENSE-MinHook.txt` nel pacchetto binario.
