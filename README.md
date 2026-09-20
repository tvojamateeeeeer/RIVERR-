# DarkArp – one-shot arp sampler (VST3)

Hodíš dovnútra one-shot, plugin ho automaticky vyladí na C5 a zahrá z neho arp
z akordu, ktorý mu pošleš z piano rollu. Robený na temné ambient arpy.

## Ako z toho dostať .vst3 (Windows)

1. Vytvor si účet na github.com a nový (klidne private) repozitár, napr. `DarkArp`.
2. Nahraj do neho **celý obsah tohto priečinka** (vrátane priečinka `.github`).
   Cez terminál v tomto priečinku:

       git init
       git add .
       git commit -m "DarkArp"
       git branch -M main
       git remote add origin https://github.com/TVOJE_MENO/DarkArp.git
       git push -u origin main

3. V repozitári otvor záložku **Actions** -> beh "Build VST3 (Windows)".
   Build trvá cca 10 minút. Keď je zelený, dole v **Artifacts** stiahni
   `DarkArp-VST3-Windows` (zip).
4. Rozbaľ a skopíruj priečinok `DarkArp.vst3` do
   `C:\Program Files\Common Files\VST3\`
5. FL Studio: *Options -> Manage plugins -> Find installed plugins*.
   DarkArp sa objaví v Plugin database (Generators). Pridaj ho do projektu.

## Použitie

- Zvuk: potiahni .wav/.aif/.flac/.mp3 do okna pluginu, alebo klikni **LOAD SOUND**.
  (Drag priamo z FL browsera skús – ak ho FL nepustí, použi Explorer alebo LOAD SOUND.)
- **AUTO C5**: plugin zistí výšku tónu sampla a posunie ho o najmenej poltónov na
  najbližšie C. Klávesa C5 (MIDI nota 60) potom zahrá sampel v jeho pôvodnej polohe.
  Ak detekcia netrafí (šumové / bicie zvuky), vypni AUTO C5 a doladíš cez **TUNE**.
- Arp: zahraj akord z piano rollu. **STEP SEQUENCER**: on/off, pitch (poltóny),
  velocity a pravdepodobnosť pre každý z 16 krokov. Dvojklik = reset hodnoty.
- **START / END** markery na waveforme skracujú prehrávaný úsek.
- Presety: **SAVE** uloží do `Dokumenty\DarkArp\Presets`, výber v menu **Presets**.
  Celý stav sa ukladá aj do FL projektu (sampel sa ukladá cestou, nie obsahom).

## Vývoj

Voliteľný headless test: `cmake -B build -DDARKARP_BUILD_TESTS=ON` a spusti `DarkArpTest`.
Kód: `Source/PluginProcessor.*` (DSP, arp, presety), `Source/PluginEditor.*` (GUI),
`Source/PitchDetect.h` (auto-tune).
