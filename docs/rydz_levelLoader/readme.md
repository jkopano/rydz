# rydz_levelLoader — Dokumentacja

# Spis treści

1. [Integracja](#Integracja)
2. [Tworzenie map - przygotowanie](#Tworzenie-map--przygotowanie)
3. [Tworzenie map - eksport do silnika](#Tworzenie-map--eksport-do-silnika)
3. [ENTITY OVERWIEW](#ENTITY-OVERWIEW)
---

# Integracja

dołącz system do projektu:
```cpp
#include "rydz_levelLoader/rydz_levelLoader.hpp"
```
Dla wczytywania statycznego brusha mapy dołącz:
```cpp
  app.add_systems(ScheduleLabel::Update,
      group(spawn_model).run_if(run_once()));
```
Dla wczytywania Entities typu prop_static dołącz:
```cpp
  app.add_systems(ScheduleLabel::Update,
      group(spawn_model).run_if(run_once()));
```

---

# Tworzenie map - przygotowanie

Najpierw należy pobrać narzędzia pomocnicze:
## 1. Pobierz i zainstaluj [NodeJS](https://nodejs.org/en)
## 2. Zainstaluj narzędzie do konwertowania .obj na .gltf
```npm install -g obj2gltf```
## 3. Pobierz i zainstaluj [TrenchBroom](https://trenchbroom.github.io)
## 4. Klonuj repo [trenchbroom_rydz](https://github.com/pan7nikt/trenchbroom_rydz) do katalogu
```TrenchBroom\games\Rydz```
## 5. Ustaw dobre ścieżki do silnika w katalogu
```TrenchBroom\games\Rydz\assets\paths.txt```
## 6. Uruchom TrenchBroom.exe, wybierz "New map" i "Rydz (Experimental)", potem OK
## 7. (ten krok jest opcjonalny, ale zalecany)
W view > preferences > view > Layout  ustaw na "Four panes" i wyłacz "Sync 2D views" 
## 8. Twórz level

---

# Tworzenie map - eksport do silnika
## 1. Zapisz mapę (plik .map) do silnika w katalogu
rydz/res/levels
## 2. File > Export > Wavefront OBJ i jako ścieżkę dajesz
```TrenchBroom\games\Rydz\assets```
## 3. Uruchom
```TrenchBroom\games\Rydz\assets\mapConvert.bat```
<br/>to automatycznie skopiuje wszystkie modele i mapę oraz przekonwertuje mapę do właściwego formatu
## 4. Uruchom grę

---

# ENTITY OVERWIEW
## prop_static
#### Parametry
modelpath - ścieżka do modelu (np. models/sun.glb)
<br/>origin - pozycja encji w świecie
<br/>modelscale - skala modelu
---
