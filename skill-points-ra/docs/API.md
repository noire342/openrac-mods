# API C v1

`openrac_skill_points_query(1)` restituisce una tabella `sp_api` con dimensione e
versione. Una versione sconosciuta, o un produttore non attivo, restituisce `NULL`.
Tutti i simboli di implementazione sono privati; i soli export aggiuntivi sono questa
funzione e i normali entry point del loader.

Su Windows usa `GetModuleHandleW(L"skill_points_ra.dll")` e `GetProcAddress`.
Non usare `LoadLibrary` per ottenere il servizio: non deve nascere una seconda istanza.
Controlla `size` e `abi`, e inizializza il campo `size` di ogni struttura di uscita.
Le chiamate avvengono esclusivamente sul thread di gioco OpenRAC.

| Metodo | Risultato |
| --- | --- |
| `totals` | Numero di obiettivi, sbloccati, punti disponibili e ottenuti |
| `at` | Voce all'indice del catalogo ufficiale; l'ordine del menu può essere diverso |
| `get` | Voce individuata tramite ID RetroAchievements stabile |
| `subscribe` | Ascoltatore con token di proprietà del consumatore; massimo 16 |
| `unsubscribe` | Rimuove il token; ammesso anche durante un evento |
| `is_enabled` | 1 attivo, 0 disattivato o errore negativo; coda opzionale dalla 0.1.2 |
| `active_totals` | Totali che escludono gli obiettivi disattivati; coda opzionale dalla 0.1.2 |

`totals`, `at` e `get` conservano il catalogo completo di 178 voci e gli sblocchi
storici, anche quando una sfida è esclusa. Le strutture esistenti e il prefisso
della tabella non cambiano. Prima di leggere la coda opzionale verifica
`api->size >= offsetof(sp_api, active_totals) + sizeof(api->active_totals)`
(includi `<stddef.h>`); un produttore precedente può avere una tabella più corta.
Le regole disattivate espongono misura 0/0, senza perdere il precedente sblocco.

`native_index` è 0–29 per i punti originali, -1 per le nuove sfide. `measured_target`
è zero quando la definizione originale non espone una misura. Non si inventano
percentuali per le condizioni non misurabili. `unlocked_at == 0` identifica un punto
originale riconosciuto da un salvataggio del gioco.

I puntatori a titoli, descrizioni e autori restano validi fino all'evento `STOPPING`.
Non conservarli oltre la vita del produttore; copia il testo se serve più a lungo.

Eventi: `SP_EVENT_UNLOCKED` (con ID), `SP_EVENT_STOPPING` (ID 0). Le regole vengono
valutate prima di notificare gli ascoltatori. I getter e `unsubscribe` sono ammessi
nei callback; `subscribe` restituisce `SP_BUSY`. Un callback che propaga un'eccezione
viene disconnesso. Disiscriviti all'unload del consumatore; dopo `STOPPING` elimina i
puntatori al servizio. L'esempio C implementa entrambi i percorsi.

Le API non scrivono memoria di gioco, non modificano i flag originali, non inviano
risultati a RetroAchievements e non attribuiscono punti o XP alle armi.
