# Recupero componenti

**Sorgenti in sviluppo. La release installabile non è ancora disponibile.**
Il riferimento è Windows x64, OpenRAC 0.10.0 ufficiale e R&C1 PAL Europe v2.00
(SCES_509.16). Le [verifiche e limitazioni](docs/VALIDATION.md) distinguono le
prove automatiche dai percorsi effettivamente provati nel gioco.

I veri frammenti meccanici dei robot diventano componenti fisici: cadono,
rimbalzano, rimangono durante la visita al pianeta e si raccolgono con una
pressione di **Triangolo**, entro 1,8 unità dalla superficie del pezzo e senza
ostacoli. Negozi, personaggi e altre interazioni native hanno precedenza.

La voce **Componenti** nel menu di pausa mostra miniatura, nome, provenienza
e quantità. I nomi descrivono i modelli originali, per esempio **Testa di Birdbot**
e **Cannone di Birdbot**. La guida usa il simbolo nativo di Triangolo; il tasto
torna alla pagina precedente senza riutilizzare la stessa pressione.

Il cannone a risucchio aspira e lancia lo stesso componente, che esplode alla
fine del colpo: nessuna trasformazione in nemico o seconda ricompensa.
L'[adattatore del cannone](docs/NATIVE_SUCK_CANNON.md) documenta i passaggi di
possesso esclusivo e il loro stato di verifica.

L'inventario non ha un limite di peso o spazio. File complementari `.salvage`
associano catalogo e quantità allo specifico salvataggio PAL, senza cambiare
il formato della memory card. Caricare un vecchio slot ripristina il relativo
inventario. I pezzi a terra appartengono alla visita corrente; non vengono
serializzati. Vedi [salvataggi e gestione degli errori](docs/SAVES.md).

Il nucleo C++ non dipende dal personaggio, dagli indirizzi PAL o dall'SDK.
L'[API C v1](docs/API.md) espone dati copiati del catalogo e dell'inventario,
transazioni, generazione di componenti per livelli personalizzati ed eventi.
La futura protagonista, il livello introduttivo e il crafting sono sviluppi
successivi serviti dagli stessi contratti.

L'[utility condivisa](../arsenal-compat/README.md#shared-consumers-abi-v2)
mantiene l'ABI precedente del Rotatore e aggiunge un registro di pagine di pausa:
una singola pagina appare direttamente, più pagine usano **Altri menu**.
Le mod registrano le voci senza duplicare i punti di controllo della navigazione.
Per convivere con Rotatore entrambe le cartelle devono contenere la stessa
utility v2. Le vecchie DLL delle Releases non vengono aggiornate dal solo
aggiornamento dei sorgenti: serve una build coordinata. La matrice finale
delle combinazioni è ancora in corso.

## Build

Dal progetto principale, con CMake, compilatore C++20 e SDK 1.3.0:

```sh
python tools/fetch_sdk.py
cmake -S . -B build-salvage -DBUILD_ROBOT_SALVAGE=ON -DBUILD_ARSENAL_COMPAT=ON -DBUILD_REVOLVERATOR=OFF -DBUILD_WEAPON_XP=OFF -DBUILD_SKILL_POINTS_RA=OFF -DBUILD_CINEMATIC_JOURNEYS=OFF -DBUILD_TESTING=ON
cmake --build build-salvage --config Release --parallel
ctest --test-dir build-salvage -C Release --output-on-failure
```

Windows genera `build-salvage/package/robot_salvage`, inclusa la utility.
Linux esegue soltanto i test portabili e non produce un plugin nativo supportato.
Il codice è pubblicato in `noire342/openrac-mods`; non è ancora presente un
pacchetto di Recupero componenti nelle Releases o nella ricerca installabile
del launcher. Il client ufficiale rimane invariato. Non sono inclusi dati del
disco, memory card personali, dump di memoria o un client modificato.

## Documentazione e licenze

- [Catalogo](docs/CATALOG.md) e [nomi dei componenti](docs/COMPONENT_LABELS.md).
- [Generazione nativa](docs/NATIVE_FRAGMENT_SOURCES.md), [fisica e UI](docs/UI.md).
- [API](docs/API.md), [salvataggi](docs/SAVES.md) e [miniature](docs/THUMBNAILS.md).
- [Stato delle verifiche e lavoro rimanente](docs/VALIDATION.md).

Implementazione GPL-3.0-or-later; header di interoperabilità MIT. Le dipendenze
conservano i propri avvisi. Copyright (c) 2026 noire342.
