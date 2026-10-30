# QA della combinazione finale 0.2

La matrice degli esiti già osservati è in [NATIVE_VALIDATION.md](NATIVE_VALIDATION.md).
Questa lista serve a chiudere la consegna concreta: ogni prova deve identificare
runtime, DLL, companion e asset usati. Il superamento dei test host non approva
la resa nel gioco.

## Client ufficiale e provider

- [ ] EXE installato invariato, SHA256 stock noto; nessun EXE nello ZIP.
- [ ] Solo la card Revo, XP opzionale: utility caricate privatamente; 30 binding e commit completo.
- [ ] Asset compresi, 19 manifest copiati in cache propria; originali immutati.
- [ ] Build non supportata, firma errata o binding mancante rifiutati senza hook attivi.
- [ ] Teardown possiede solo i propri detour; failure conserva moduli e diagnostica.
- [ ] Save_Restore/NewGame verificati al confine completo dei wrapper stock.

## Inventario e menu

- [ ] Partita nuova: nessuna delle due armi assegnata dalla sola inizializzazione.
- [ ] Vecchio save con Walloper: possesso originale conservato, Revo ancora separato.
- [ ] Entrambe le armi nel menu nativo,4×5 celle, preview/nome/munizioni coerenti.
- [ ] Selezione e scorrimento corretti anche con più di 20 voci registrate.
- [ ] Gli otto preferiti possono contenere entrambe le identità; salvataggio,
      ricaricamento e anello rapido non le scambiano.
- [ ] Nessun ID logico 65554 finisce negli array nativi o nel file di salvataggio.

## Combattimento, motore e cariche

- [ ] Tap e rilascio prima della soglia non catturano accidentalmente un nemico.
- [ ] Presa, rotazione, lancio e impatto funzionano con nemici ammessi diversi.
- [ ] Boss e bersagli esclusi mantengono il colpo ravvicinato, senza presa.
- [ ] Casse e bersagli colpiti reagiscono attraverso il danno nativo.
- [ ] Una sola carica per attivazione; venti usi portano 20→0.
- [ ] Pickup ordinario soltanto se posseduto:2 cariche, massimo 20.
- [ ] Punta lenta a riposo, veloce e sincronizzata durante la presa, ferma a vuoto.
- [ ] Tentativo a vuoto: scatto breve e ritorno allo stesso angolo.
- [ ] Pausa, cambio arma, morte, cutscene e viaggio non lasciano collisioni,
      AI, posa o riferimenti a entità liberate in uno stato alterato.

## Venditore e salvataggi

- [ ] Hoven visitato è il requisito; sola disponibilità sulla mappa non basta.
- [ ] Acquisto 15.000 bolt e carica 25 bolt, senza sostituire possesso/prezzo Walloper.
- [ ] Pickup e anteprima mostrano una singola cartuccia, con scala corretta.
- [ ] Native save valido con Walloper originale; preferiti custom sanitizzati.
- [ ] Sidecar atomico per slot: possesso, cariche, XP e preferiti custom ripristinati.
- [ ] Autosave già pending non sostituisce il relativo snapshot con stato più nuovo.
- [ ] Sidecar mancante, corrotto o con fingerprint diverso non applica dati di un altro save.
- [ ] New Game azzera il record custom; NG+ conserva il progresso previsto e i preferiti.
- [ ] Errori IO, cambio slot e caricamento durante contesti di menu non scambiano record.

## XP opzionali

- [ ] Standalone senza Weapon XP e con provider v4 compatibile provati separatamente.
- [ ] Provider precedente rifiutato chiaramente, senza credito silenzioso al Walloper.
- [ ] Uccisione guadagnata: Revo cresce e Walloper resta invariato; prova inversa col Walloper.
- [ ] Nessun XP per colpi soppressi, cattura sola, casse, morte duplicata o token vecchio.
- [ ] Rate, cap per kill e moltiplicatore per arma rispettano i bolt nativi e le frazioni.
- [ ] Limite livello 10, nomi vN e animazione nativa per ogni livello attraversato.
- [ ] Bonus danni/munizioni disattivati di default; melee non scalato due volte,
      lancio origin 2017 non moltiplicato nuovamente dal consumer.

## Resa grafica e consegna

- [ ] Modello Revo distinto dal Walloper in gioco, venditore, menu e scena di acquisizione.
- [ ] Braccio fuori dalla testa, impugnatura stabile e contatto del nemico sul punto preso.
- [ ] Trivella sempre visibile; ruota solo la punta, nessuna aura Walloper sul Revo.
- [ ] HUD in uso con icona cartuccia; trivella `59901:13` soltanto in inventario/anello.
- [ ] HD e fallback mantengono il footprint nativo e la corretta identità.
- [ ] Carica e XP nel riquadro esistente, senza coprire contatore o altri elementi.
- [ ] Verificare 4:3, 16:9, 16:10, lingue, menu, viaggio e livelli con poca memoria.
- [ ] ZIP nativo importato dal launcher, senza script o card utility esterne.
- [ ] Nessuna ISO, RAM, campagna, sidecar o asset estratto nel pacchetto sorgente.
- [ ] Ricevuta finale con hash e soli esiti osservati; nessuna pubblicazione automatica.
