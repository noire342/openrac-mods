# API pubblica Space Voyages v1

Per l'identità dei mondi custom e il confine fra registro, viaggio e regole
del contenuto vedere [CUSTOM_DESTINATIONS.md](CUSTOM_DESTINATIONS.md).

L'header `include/space_voyages_api.h` è indipendente dall'SDK del motore e
disponibile con licenza MIT. La DLL caricata esporta:

```cpp
extern "C" const space_voyages_api_v1*
openrac_space_voyages_query(uint32_t version);
```

Risolvere il simbolo dalla libreria già caricata, senza caricarne una seconda
copia. Richiedere la versione `1`; un puntatore nullo indica versione non
supportata o servizio non disponibile. Controllare `version` e `size` della
tabella prima dell'uso. Tutte le chiamate appartengono al thread del gioco.
La tabella e i token cessano di essere validi quando la mod viene scaricata.

## Lettura

```cpp
space_voyages_state_v1 state{};
state.size = sizeof(state);
if (service->get_state(&state)) {
    // Leggere phase, origin, destination, candidate e position.
}
```

`phase` segue `0 Ground → 1 Departure → 2 FreeFlight → 3 Approach → 4 Arrival`.
L'arrivo concluso ritorna a `Ground`.
La fase `Departure` comprende decollo originale e ingresso animato nello spazio;
`FreeFlight` viene emesso quando termina il raccordo e si ricevono i comandi.
Il respawn locale riusa `Departure → FreeFlight` dentro la stessa scena,
con provenienza invariata: non equivale a un nuovo decollo del mondo nativo
o a una visita assegnata. Durante la distruzione la fase resta `FreeFlight`
e il candidato viene azzerato. Valori e struttura dell'API v1 non cambiano.
Provenienza, destinazione e candidato
sono ID nativi di livello, oppure `-1` se assenti. `candidate_unlocked` indica
se il candidato può essere scelto; la sola vicinanza non basta a entrare.
`position` usa gli assi della galassia (+X avanti, Z alto), `speed` unità/s.
`paused` si riferisce alla scena di volo. I dati sono copie, senza puntatori
alla memoria del gioco; non attribuiscono visite o progressi.

## Identità delle destinazioni nella 0.8

L'API pubblica v1 rimane **osservativa e invariata**. `origin`, `destination`
e `candidate` conservano gli ID nativi del catalogo; non contengono un handle
del registro host né la variante interna della rotta. Perciò il valore 18
non distingue **Veldin Casa** dal **Veldin finale della campagna**: entrambi
usano il programma 18. Anche `candidate_unlocked == 1` esprime soltanto
l'idoneità corrente alla scelta, non l'identità logica del mondo.

Un consumer non deve dedurre Casa dal livello 18, dalla nave visibile, da una
riga del menu o da una visita iniziale al tutorial 0. La selezione fra
`Campaign` e `VeldinGarage` appartiene alla mod; l'identità e il caricamento
della destinazione appartengono al registro host SDK 1.14. L'API v1 di Space
Voyages non espone controlli per montare mondi, cambiare rotta, ottenere
ticket o scrivere i progressi. Il contratto del provider Garage è separato.

I voli ordinari richiedono il bridge scene/audio SDK 1.13. La possibilità
opzionale di entrare a Casa richiede anche SDK 1.14, un provider PAL `READY`
verificato per la generazione corrente e il lookup valido di `veldin-casa`.
Questa disponibilità non assegna la coordinata finale 18; una volta ottenuta
quella coordinata, il corpo Veldin risolve la destinazione di campagna.
Nessuna rotta usa il tutorial 0 come sostituto di Casa.

Vedere [VELDIN_GARAGE.md](VELDIN_GARAGE.md) per separazione fra provider,
registro host, tre navi supportate e stato delle prove. La nuova integrazione
ha controlli CPU, ma non ancora una prova nativa congiunta con il garage reale;
gli eventi v1 non costituiscono da soli quella prova.

## Eventi

```cpp
uint32_t token = 0;
if (service->subscribe(on_phase_changed, user, &token)) {
    // Conservare token e annullare la sottoscrizione prima dello scaricamento.
}
service->unsubscribe(token);
```

La callback riceve `(void* user, const space_voyages_state_v1* state)` quando
cambia la fase. Il puntatore allo stato è valido soltanto durante la callback:
copiarlo per conservarlo. Non è un evento di posizione a ogni frame. Durante
il volo i frame ordinari del mondo sono sospesi; questi eventi continuano a
segnalare ingresso, avvicinamento e uscita della scena.

Sono disponibili 16 sottoscrizioni. `subscribe` e `unsubscribe` restituiscono
`1` in caso di successo, `0` altrimenti. Si può annullare una sottoscrizione
anche durante una callback. Una nuova sottoscrizione riceverà gli eventi
successivi; leggere lo stato corrente esplicitamente se necessario.

Le callback devono terminare rapidamente e non cambiare livello, rientrare
nella gestione del viaggio o scaricare mod. Questa API è di osservazione:
non concede coordinate, non teletrasporta la nave e non cambia i salvataggi.
