# Verifica NTSC-U del 30 ottobre 2026

Build privata OpenRAC 0.9.0, commit runtime `77a2f052`, mod `0.1.0`.
DLL Windows x64 SHA-256:
`ff3b5b109a65ef9ad786f4f6bb819b130c7e77887589cf4ea55d3296db40cb29`.
Asset: `model-02` (sola spirale sul giunto 4), `ammo-levels-03`
(cartuccia singola, classi 32000/2016), icona privata Real-ESRGAN 128×128.
Le fixture sono isolate, senza campagne utente; RAM e asset originali non fanno
parte dei sorgenti pubblici.

| Prova | Esito osservato |
|---|---|
| Build Windows | 10/10 gruppi CTest superati |
| Tap di cinque frame | 0 catture, 0 lanci, una carica consumata |
| Pressione minima | 0,18 s nel controller; test ai bordi prima/dopo la soglia |
| Presa/lancio reali su Novalis | 1 cattura, 1 lancio, 1 impatto, 10→9 munizioni |
| Posa durante la presa | Asse verticale misurato 0,99999–1,00000; arma visibile |
| Punto di presa | Contatto ricostruito da RAM entro 1,04 mm dalla punta, con coordinate arrotondate |
| Rotore | Minimo 1 rad/s; durante la presa sincronizzato con il nemico, fino a 18 rad/s |
| Arma scarica | Ferma a 0,93333 rad; scatto fino a 1,13240, ritorno a 0,93333 in 0,18 s |
| Annullamento API durante la presa | A 260 frame annullata; a 270 Idle, nessun lancio o impatto |
| Cassa munizioni nativa | Cassa distrutta, 4→3 per il colpo, poi raccolta ordinaria 3→5 |
| Raccolta al limite | Pickup nativo 9→10, senza superare la capacità |
| Venditore prima di Hoven | Slot 18 assente dalla lista di una partita nuova |
| Venditore Hoven | Prezzo 15.000; 20.000→5.000 bolts, possesso e 10 munizioni |
| Rifornimento dal venditore | 2→3 munizioni, 5.000→4.975 bolts; modello cartuccia presente |
| HUD e impostazione carica disabilitata | Icona HD nel riquadro nativo 32×32, contatore e pannello originali |
| Weapon XP installato 0.6.2 | Profilo isolato: 39 XP guadagnati dall'impatto reale |
| Nomi con Weapon XP | Prova distinta, 3.500 XP assegnati tramite API: stringa nativa `Revolverator v4` |
| Caricamento pianeti | Livelli 0–18 caricati; arma e rotore operativi nei contesti Ratchet ammessi |

Il test API dei nomi non dimostra progressione guadagnata: quella è verificata
separatamente con i 39 XP. Su Orxon la prima visita controlla Clank; forzare
uno slot arma nel probe non abilita armi che il gioco vieta a Clank.

## Memoria e limiti

Braccio e rotore hanno un'arena essenziale di 1 KiB. Il livello riserva fino a
128 KiB aggiuntivi quando disponibili, senza superare il limite dell'heap. Nei
livelli pieni, l'arena essenziale usa un'allocazione nativa HUD eseguita per prima.
La capacità delle munizioni e il combattimento non dipendono dall'allocazione
dei testi personalizzati o dell'icona HD. Rilgar, Blarg Station e Quartu sono
stati riprovati esplicitamente dopo questa correzione; raccolta e cap sono stati
verificati anche a Rilgar.

A Rilgar non c'è spazio per la pagina HD e resta il simbolo nativo 32×32.
I messaggi di raccolta aggiunti possono mancare quando il clone del banco testi
non entra in memoria; le munizioni vengono comunque raccolte normalmente.
L'upscale e la traduzione personalizzata restano attivi quando le relative
risorse sono disponibili.

Lo smoke dei diciannove livelli non è una campagna completa: non certifica ogni
nemico, boss, combinazione di mod, salvataggio, formato video o interruzione.
La verifica PAL e una prova completa della campagna rimangono fuori da questi
esiti. I boss sono esclusi dalla presa e conservano il colpo ravvicinato nativo;
non è stata eseguita una boss fight completa in questa sessione.
