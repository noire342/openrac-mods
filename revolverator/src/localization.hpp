// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 noire342
#pragma once
#include <array>
#include <cstdint>

namespace revolverator::localization {

// This is the game's column number, not sceScfGetLanguage's console setting.
// Native sources: port/assets/disc/messages.h; host/options.c;
// NTSC draw_subtitles.c. Native column1 is unused. Global address is canonical
// NTSC-U: callers must resolve it through Memory::global before reading.
inline constexpr std::uint32_t native_language_address=0x15ed88u;
enum class Language { English, French, German, Spanish, Italian };

constexpr Language language_from_native(int value) noexcept {
 switch(value){
 case 2:return Language::French;
 case 3:return Language::German;
 case 4:return Language::Spanish;
 case 5:return Language::Italian;
 default:return Language::English;
 }
}

struct Strings {
 const char* weapon_name;
 const char* description;
 const char* pickup_singular;
 const char* pickup_plural;
};

// Printable ASCII only until the native font's extended character encoding is
// established. Each pickup format accepts exactly one integer (%d), matching
// the existing native pickup-message formatter. Descriptions fit its 256-byte
// allocation slot, including NUL. Hold/release reflects the capture gesture;
// the controller's 0.18s minimum hold is deliberately not a UI tuning number.
inline constexpr std::array<Strings,5> translations{{
 {"Revolverator",
  "Hold Circle to catch an enemy with the drill and spin it. Release to throw. Some enemies cannot be caught.",
  "Collected %d Revolverator cartridge",
  "Collected %d Revolverator cartridges"},
 {"Revolverator",
  "Maintenez Rond pour saisir un ennemi avec la foreuse et le faire tourner. Relachez pour le lancer. Certains ennemis ne peuvent pas etre saisis.",
  "Vous avez obtenu %d cartouche pour le Revolverator",
  "Vous avez obtenu %d cartouches pour le Revolverator"},
 {"Revolverator",
  "Halte Kreis, um einen Gegner mit dem Bohrer zu packen und zu drehen. Lass los, um ihn zu werfen. Manche Gegner lassen sich nicht greifen.",
  "%d Revolverator-Patrone erhalten",
  "%d Revolverator-Patronen erhalten"},
 {"Revolverator",
  "Manten Circulo para atrapar a un enemigo con el taladro y hacerlo girar. Suelta para lanzarlo. Algunos enemigos no se pueden atrapar.",
  "Has recogido %d cartucho para el Revolverator",
  "Has recogido %d cartuchos para el Revolverator"},
 {"Revolverator",
  "Tieni premuto Cerchio per afferrare un nemico con la trivella e farlo ruotare. Rilascia per lanciarlo. Alcuni nemici non possono essere afferrati.",
  "Hai raccolto %d cartuccia per il Revolverator",
  "Hai raccolto %d cartucce per il Revolverator"}
}};

constexpr const Strings& strings_for_language(int native_value) noexcept {
 return translations[static_cast<unsigned>(language_from_native(native_value))];
}
}
