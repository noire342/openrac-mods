// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once

namespace weaponxp {
// Conservative combat whitelist from difficulty-mod/classes.tsv. Crates, NPCs and helpers earn no XP.
inline int enemy_reward(int oclass) {
    switch (oclass) {
        case 28:
            return 10;  // plasmabot
        case 29:
            return 10;  // blarg_turret
        case 30:
            return 10;  // machinegun_turret
        case 31:
            return 10;  // minibomber
        case 35:
            return 10;  // sharkigator
        case 36:
            return 10;  // blarg_gunner
        case 44:
            return 10;  // sentrybot
        case 52:
            return 10;  // seeker
        case 63:
            return 10;  // robosquawker
        case 123:
            return 10;  // spiderbot
        case 133:
            return 10;  // amoeboid
        case 211:
            return 10;  // plasmabot
        case 217:
            return 10;  // big_axebot
        case 252:
            return 10;  // scoutbot
        case 253:
            return 10;  // bomber_tank
        case 294:
            return 10;  // blarg_trooper
        case 326:
            return 10;  // blarg_helicopter
        case 333:
            return 10;  // blarg_bombthrower
        case 340:
            return 10;  // circular_saw_enemy
        case 344:
            return 10;  // blarg_gun
        case 347:
            return 10;  // dreks_fleet_turret
        case 427:
            return 10;  // blarg_trooper
        case 459:
            return 10;  // birdbot
        case 563:
            return 10;  // woodcut_bot
        case 572:
            return 10;  // large_amoeboid
        case 573:
            return 10;  // robot_dog
        case 577:
            return 10;  // peckbot
        case 578:
            return 10;  // mine_layer
        case 580:
            return 10;  // sand_shark
        case 623:
            return 10;  // extermibot
        case 625:
            return 10;  // extermitank
        case 631:
            return 10;  // blarg_helicopter
        case 638:
            return 10;  // blarg_elite_commando
        case 749:
            return 10;  // horny_toad
        case 865:
            return 10;  // medium_amoeboid
        case 866:
            return 10;  // small_amoeboid
        case 1023:
            return 10;  // blarg_paratrooper
        case 1048:
            return 10;  // alien_snapper
        case 1051:
            return 50;  // alien_queen
        case 1059:
            return 10;  // sharkigator
        case 1068:
            return 10;  // blarg_space_commando
        case 1106:
            return 50;  // snagglebeast
        case 1126:
            return 10;  // machinegun_turret
        case 1130:
            return 50;  // commando
        case 1196:
            return 10;  // screamer
        case 1201:
            return 10;  // turret
        case 1202:
            return 10;  // toxic_crab
        case 1231:
            return 10;  // psytcopus
        case 1246:
            return 10;  // puffoid
        case 1262:
            return 10;  // blarg_space_commando
        case 1267:
            return 10;  // hoven_turret
        case 1346:
            return 10;  // gun
        case 1371:
            return 10;  // blarg_commander
        case 1382:
            return 10;  // robot_dog
        case 1422:
            return 50;  // drek_final_boss_robot
        case 1445:
            return 10;  // chomper
        case 1454:
            return 50;  // rocket_tank
        case 1564:
            return 10;  // pterodactyl
        default:
            return 0;
    }
}
}  // namespace weaponxp
