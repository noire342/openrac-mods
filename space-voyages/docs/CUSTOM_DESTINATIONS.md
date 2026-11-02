# Destinazioni custom e separazione delle responsabilità

Una destinazione custom è identificata dal proprio manifest ID. Il programma
nativo è soltanto il modello di esecuzione: due mondi che usano `level: 18`
non diventano lo stesso pianeta. Posizione nel menu, handle del registro e
indirizzi degli attori non sono identità persistenti.

`RegisteredDestination` conserva questa identità in uno spazio fisso, senza
allocazioni o puntatori a stringhe di un altro plugin. Distingue campagna e
destinazione aggiunta. `DestinationGateway` usa esclusivamente l'SDK 1.14:
lookup, contesto corrente, ticket di partenza, preparazione, commit e rilascio.
Non conosce Veldin, flotta, missioni, modelli, coordinate sbloccate o salvataggi.
Il registro dell'host rimane autorevole; un ID costruito localmente non concede
accesso a un mondo non registrato.

```cpp
const auto alpha = RegisteredDestination::added("custom-alpha", 18);
const auto beta = RegisteredDestination::added("custom-beta", 18);
// alpha != beta, anche se entrambi usano lo stesso programma nativo.
```

Non serve modificare il gateway per ciascun nuovo pianeta. Gli ID arrivano al
lookup dell'host; ogni risposta viene confrontata con l'identità richiesta,
l'handle e le generazioni correnti. Non viene imposto un numero di mondi dal
codice del gateway. Il test dedicato attraversa 257 identità diverse, incluse
destinazioni con lo stesso programma nativo e template 0; non attribuisce loro
la progressione del tutorial della campagna.

## Il garage come primo adattatore

Le regole attuali di Veldin appartengono a `garage_destination_adapter.hpp` e
alla policy della galassia: una sola rappresentazione del corpo, garage prima
dello sblocco finale, campagna finale dopo il vero sblocco. `DestinationClient`
è la facciata di compatibilità usata da questo catalogo; delega tutta la
gestione dei ticket al gateway generico. Un mondo aggiunto sconosciuto al
catalogo attuale viene rifiutato, senza essere scambiato per Veldin.

Il provider del garage conserva le proprie responsabilità: possesso e scelta
delle navi, profilo, contenuti del livello e politica di ritorno alla campagna.
Queste regole non entrano nel gateway condiviso. Un altro pianeta può avere
regole diverse senza ereditare le regole della flotta.

## Cosa definisce un futuro pianeta

Un'integrazione completa fornisce tre elementi separati:

- il mondo registrato nell'host, con manifest ID stabile, risorse e programma;
- i dati di presentazione nella galassia: nome, modello, posizione, dimensioni;
- la policy di disponibilità e progressione del contenuto.

Il viaggio, il controllo dei token e la continuazione nativa restano condivisi.
La scoperta automatica di nuovi modelli/marker della galassia non viene
spacciata per già presente: il catalogo attuale contiene i corpi originali e
l'adattatore del garage. Questa separazione consente di estendere il catalogo
senza introdurre un'altra implementazione del volo o copiare il garage.

## Compatibilità e verifiche

L'API osservativa pubblica Space Voyages v1 rimane byte-identica. I suoi ID
nativi non distinguono due mondi custom che usano lo stesso programma; non
vanno usati come chiavi per nuovi contenuti. Una futura API di catalogo può
esporre gli ID stabili con una versione propria, conservando il prefisso v1.

I test del gateway verificano identità, template condivisi, registrazioni
sostituite/scadute, fallimento del commit e consumo una sola volta. I test
precedenti della facciata e della Session mantengono il confronto sulle rotte
di campagna e garage. Sono prove CPU, distinte dalle prove native del contenuto.
