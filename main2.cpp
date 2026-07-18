// main2.cpp оставлен как точка сборки единого translation unit.
// Реализация разделена на тематические файлы в source_parts/, чтобы код было легче
// сопровождать и расширять будущими модами без поиска по одному огромному файлу.
//
// Сборка через g++ из корня проекта:
// g++ -std=c++17 main2.cpp glad.c -Iinclude -I. -lglfw -lGL -ldl -pthread -lsfml-audio -lsfml-system -o minecraft

#include "source_parts/01_core_globals.hpp"
#include "source_parts/02_physics_collision.inc"
#include "source_parts/03_configs_audio_saves.inc"
#include "source_parts/04_lighting_worldgen_workers.inc"
#include "source_parts/05_ui_inventory_menus.inc"
#include "source_parts/06_chunks_blocks_resources.inc"
#include "source_parts/07_gameplay_rendering.inc"
#include "source_parts/08_input_shaders_main.inc"
