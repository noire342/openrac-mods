# Disposizione della galassia

Le coordinate X/Y provengono dalla tabella originale `level_map_markers`:
NTSC-U `0x001DDE28`, offset nell'ELF retail `0x000DEDA8`, 20 record di quattro
interi da32bit. Tutti i record sono stati confrontati con la copia locale
del gioco e con `draw_level_selection_map.c`. SHA-256 dei320byte originali:
`f340142500ed756e435a76b050c422b4d900581f3db0ddfdffc11101cb5cac04`.

È una mappa UI bidimensionale: non contiene quote Z, orbite o distanze
astronomiche. La mod conserva la proiezione originale e ricostruisce una
profondità stabile per distribuire i corpi nello spazio. Scala e profondità
sono scelte di presentazione e navigazione, non nuovi dati canonici.

## Trasformazione attuale

```text
world.x = (map.x -157.5) ×8.75
world.y = (95-map.y) ×8.75
world.z = authored_depth ×2.5
```

La scala del primo catalogo completo era3.5, con quote `authored_depth` già
distribuite in3D. Tutte le distanze sono state poi ampliate di2.5 volte su
richiesta dell'utente. Velocità24/30unità/s, accelerazione, sterzata, camera,
raggio dei corpi e margini di avvicinamento restano invariati. La distanza
minima fra corpi è circa235unità; fra due pianeti è circa306. La massima è
2.166,84unità (Veldin–Pokitaru), circa72,23secondi a turbo costante, esclusi
sterzate, decollo, avvicinamento e atterraggio.

| Corpo | Livello | Mappa X | Mappa Y | authored_depth |
|---|---:|---:|---:|---:|
| Veldin | 18 | 35 | 80 | -130 |
| Novalis | 1 | 80 | 95 | 75 |
| Aridia | 2 | 85 | 130 | -95 |
| Kerwan | 3 | 90 | 65 | 145 |
| Eudora | 4 | 108 | 110 | -35 |
| Rilgar | 5 | 160 | 110 | 120 |
| Stazione Blarg | 6 | 120 | 85 | 15 |
| Umbris | 7 | 150 | 150 | -165 |
| Batalia | 8 | 180 | 130 | 35 |
| Gaspar | 9 | 200 | 110 | -100 |
| Orxon | 10 | 220 | 140 | 135 |
| Pokitaru | 11 | 280 | 100 | -25 |
| Hoven | 12 | 230 | 80 | -150 |
| Base Gemlik | 13 | 210 | 40 | 155 |
| Oltanis | 14 | 200 | 50 | 70 |
| Quartu | 15 | 160 | 40 | -40 |
| Kalebo III | 16 | 130 | 50 | -125 |
| Flotta di Drek | 17 | 45 | 90 | -50 |

Il marker iniziale0 di Veldin è vuoto; il gioco presenta soltanto il marker18.
La mod usa un solo corpo e rispetta quello sblocco per l'arrivo finale. Orxon10
e Oltanis14 restano due pianeti distinti. Gli sblocchi non alterano posizione,
scala, identità o profondità dei corpi.

Il catalogo C++ è la fonte usata da navigazione e renderer. I test controllano
la trasformazione della mappa, gli alias narrativi, le distanze,378 combinazioni
di origine/sblocchi e i corridoi degli ingressi, anche nei gruppi più stretti.
Il limite morbido della galassia è1.300unità; il guardiano finale è1.450.
Entrambi provengono da `galaxy_bounds`, condiviso con la validazione dei dati.

## Fonti e risorse

- Sorgente OpenRAC: `games/rac1/ntsc/src/ui/map/draw_level_selection_map.c`
  e `include/rnc/ui/map/level_map.h`.
- Controprova locale e record grezzi: `work/space-voyages-tools/native-galaxy-map.json`.
- Le preview native provengono dal gruppo `transition` della propria ISO;
  le strutture3D riusano terrain e TIE collocati originali. Dettagli in
  [ASSETS.md](ASSETS.md).

Le versioni PAL richiedono prove proprie: la conversione Y della UI originale
non costituisce una verifica del runtime PAL della mod.
