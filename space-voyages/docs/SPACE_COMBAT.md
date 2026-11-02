# Navi, danni, laser e audio della scena spaziale

La simulazione è locale alla mod. Non usa la salute di Ratchet, munizioni,
missioni, ricompense, checkpoint o oggetti del mondo sospeso. Le battaglie
originali di Gemlik e della flotta conservano il proprio comportamento.

## Scafo e ripartenza

Lo scafo parte da 100. Un impatto misura la componente della velocità contro
la normale della roccia, prima della frenata geometrica. Sotto 4 unità/s non
infligge danni; un impatto frontale a 24 ne infligge 22, a 30 ne infligge 29.
Il contatto resta agganciato finché la nave non si stacca per almeno 0,25 s;
il cooldown di 0,45 s impedisce che sfioramenti ripetuti consumino lo scafo
a ogni frame. I contatti non cambiano sterzata, inclinazione o comandi.

L'indicatore è l'arco FX49 originale di Gemlik, sullo stesso quad del radar
FX50. La palette cambia prima del filtraggio bilineare e mantiene l'opacità
nativa 112/128. Il riempimento segue il guadagno 0,2 e il limite di un punto
per tick del dominio nativo a 256 unità, riportato sullo scafo a 100.

A zero scafo si bloccano comandi e ingresso nei pianeti. La scena presenta
una sfumatura al nero; solo dopo il frame nero confermato può ripartire.
La nave torna alla posa sicura registrata alla partenza, ripete l'ingresso
spaziale di 1,75 s e riceve un secondo di protezione giocabile. Il mondo
rimane sospeso nella stessa acquisizione SDK; non viene ricaricato il pianeta.
La pausa ferma scafo, HUD, animazioni, proiettili e distruzione.

## Propulsori

I punti dei propulsori provengono dalle facce delle aperture nella posa
skinned realmente usata dal modello preparato: 8 per la nave verde, 2 per
quella arancione e 2 per quella viola. Quelli viola hanno normali inclinate,
che non vengono appiattite lungo l'asse X. Il medesimo modello interpolato
trasforma scafo ed effetti; non esiste un'integrazione separata che accumuli
ritardo o sposti la fiamma durante le curve.

Texture FX5 e tinte derivano dalle risorse originali; la forma volumetrica
è un adattamento della mod. La lunghezza cresce con il turbo. Geometria,
texture e punti vengono caricati una volta; tempo, intensità e turbo sono
uniformi GPU. La pausa usa il tempo di scena fermo.

## Armi e asteroidi

La nave verde 531 è sempre disarmata, anche se una configurazione esterna
provasse a dichiararla armata. Per 532 e 533 si adatta il laser del caccia
Gemlik: Quadrato/L1, cadenza di quattro tick a 60 Hz, velocità 400 più quella
della nave e gittata 200. Il primario non introduce un contatore munizioni.
Le due navi cinematiche non possiedono questa funzione nel gioco originale:
le loro aperture frontali diventano emettitori della mod, senza sostituire
il modello. Non vengono scambiati i fari blu per cannoni.
Le navi armate riusano il mirino originale a tre componenti FX17/18/8,
proiettato a 23 unità lungo la direzione reale dei cannoni; non è un nuovo
pannello né un sistema di aggancio automatico dei bersagli.

La geometria e la texture del proiettile sono quelle originali della classe
1009. La scala è adattata alla nave e all'apertura; sulla viola è limitata
dal diametro reale della bocca. Il punto più avanzato del modello coincide
con la posizione fisica del colpo. Non si spara dal centro della nave.
Per renderlo leggibile dalla camera posteriore, il plasma si espande fuori
dalla bocca nei primi sei metri: tre volte sulla nave arancione e sei sulla
viola. È un adattamento visuale dichiarato, guidato dai dati. Il diametro
al piano di nascita rimane quello del cannone e la scia non si allunga
dietro quel piano; collisione, cadenza e gittata non cambiano.

Le rocce piccole richiedono tre colpi, quelle grandi sei. Le collisioni
continue impediscono ai laser di attraversare una roccia fra due tick;
i pianeti fermano i colpi e rimangono indistruttibili. Le rocce distrutte
non partecipano più a collisioni, camera o colpi. Rimangono distrutte anche
al respawn della nave e vengono ripristinate soltanto nel nuovo viaggio.
Non assegnano bolts, esperienza o ricompense.

Una maschera GPU conserva anche il tempo esatto dell'impatto: la roccia
non scompare prima che il laser interpolato la raggiunga. Due frammenti
visivi del modello originale 1412 si separano per mezzo secondo. È un
adattamento del comportamento di `spawn_asteroid_fragment` e
`process_asteroid_hit`, non la riproduzione completa dell'esplosione nativa.
Il pool di 128 proiettili e i 32 eventi persistenti hanno capacità fisse;
gli aggiornamenti del disegno avvengono alle revisioni dei dati.

## Audio originale di Gemlik nella 0.6

Musica, motore continuo, turbo, laser, impatti e distruzioni usano campioni
originali della battaglia di Gemlik. I binding audio delle classi 531, 532 e
533 sono separati nei dati; il pack corrente adotta il caccia originale 69
come fallback di volo per queste navi cinematiche. Il turbo usa il proprio
campione originale, con un raccordo di gain e pitch; il motore non viene
risintetizzato. La nave verde mantiene motore e turbo, restando disarmata.

`scene_audio` consuma gli ID dei nuovi colpi, gli eventi di impatto persistenti
e i contatori del danno durante i tick di simulazione. Un fotogramma disegnato
più volte non ripete il suono. Le voci hanno capacità fissa; il controller
non cambia traiettorie, danni, salute delle rocce o ricompense.

La pausa congela musica ed effetti. Alla distruzione si fermano i motori;
la musica continua e il respawn locale ripristina il minimo. Il pack originale
non contiene un cue aggiuntivo di respawn. I volumi sono quelli ordinari
Musica/Effetti del gioco, seguiti dal volume globale e dal dispositivo di
uscita già gestiti da OpenRAC.

## Confini e compatibilità

`ship_damage`, `space_combat`, contatti e componenti GPU hanno responsabilità
separate. I file versionati `flight-effects.tsv` e `weapons.tsv` contengono
risorse, montaggi e parametri. Modelli esterni con aperture diverse richiedono
montaggi verificati; le texture compatibili conservano i punti del modello.
I vecchi pack continuano a funzionare con le capacità disponibili. Senza
arco salute la mod non abilita danni invisibili; senza modello laser non
abilita gli spari. Dati presenti ma malformati vengono rifiutati.

La 0.6 richiede il runtime isolato con mixer generico SDK 1.13. Le build
precedenti SDK 1.12 mantengono la propria identità e le relative ricevute.
Non vengono chiamate routine di un overlay Gemlik assente. L'API pubblica
v1 e il formato dei salvataggi rimangono invariati.

I controlli host dell'audio verificano mixer, caricamento, cue e ciclo di vita.
Tour nativi PAL/NTSC, fixture PAL delle tre navi e cattura WASAPI hanno ricevute
separate della build 0.6; vedere [AUDIO.md](AUDIO.md) per esiti e limiti.

Fonti e trasformazioni delle risorse sono in [ASSETS.md](ASSETS.md).
Prove host, prove del driver e catture native sono distinte in
[VALIDATION.md](VALIDATION.md).
