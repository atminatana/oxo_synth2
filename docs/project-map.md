# Карта проекта Oxo Synth2

Навигация по репозиторию: что где лежит, куда идти с типовой задачей и какие артефакты
получаются из исходников. **Карта — не источник фактов**: измеренные законы движка, правила
чистой комнаты и открытые вопросы живут в [AGENTS.md](../AGENTS.md) и
[architecture.md](architecture.md). Здесь только «где это лежит».

## Дерево репозитория

```text
Identity.xcconfig          ИДЕНТИЧНОСТЬ И ВЕРСИЯ продукта: имена, bundle ID, коды AU, FUID VST3,
                           папки, MARKETING_VERSION / CURRENT_PROJECT_VERSION. Форк меняет его
                           (Identity.local.xcconfig — локальный override).
build_vst3.sh              mac: сборка VST3 + AUv2 и установка в ~/Library/Audio/Plug-Ins.
build_windows.bat          Windows: сборка VST3 (clang-cl по умолчанию, --msvc, --bench).
OxoSynth2.xcodeproj/       iPad: приложение-оболочка + AUv3 appex (синхронизованные папки).
EngineCore/                header-only C++20 движок oxo::s2: параметры, .sy1, синтез, эффекты,
                           патч-библиотека, MIDI learn, program change. Без платформенных
                           зависимостей — собирается и в тестах, и в s2render.
EditorCore/                переносимые сервисы редактора: C-хуки, состояние, файлы, темы,
                           настройки, проверка обновлений.
Shared/                    SwiftUI-панель для mac VST3/AUv2 и iPad AUv3 + встроенные банки.
OxoSynth2/                 iPad-приложение: только экран «откройте в хосте» + версия.
OxoSynth2AudioUnit/        AUv3 appex: AUAudioUnit-оболочка, вью-контроллер, DSP-инстанс,
                           C-мост с render loop.
OxoSynth2VST3/             mac VST3: processor / controller / editor view, CMake, AUv2-обёртка.
Windows_Build/             Windows x64 VST3: Win32/GDI+ редактор, CMake, тесты хранилища.
EngineCoreTests/           C++-тесты движка и переносимых сервисов (`make test`).
tools/                     чёрные ящики и измерения: s1ref (оригинал), s2render (наш движок),
                           bench, au_harness, ui_snapshot, сборка инсталляторов, win_check.
reference/meta/            ЗАКОММИЧЕННЫЕ измерения (params.json, nativemap.txt, таблицы).
                           Сгенерированные из них EngineCore/*Tables.hpp руками не правим.
third_party/               сабмодули: VST3 SDK 3.8.1, AudioUnitSDK 1.4.0. НЕ РЕДАКТИРУЕМ.
docs/                      эта документация + images/ для README.
scripts/doc_system.py      runtime самообновляемой документации.
```

`reference/renders/`, `reference/derived/`, `reference/presets/`, `dist/`, `build/` и
`Windows_Build/build-*` — генерируемые/локальные, в git не входят.

## Задача → файлы

| Задача | Начинать с | Проверять |
| --- | --- | --- |
| Звук: осцилляторы, фильтр, огибающие, LFO, эффекты | `EngineCore/Synth.hpp`, `Voice.hpp`, `Filter.hpp`, `Envelope.hpp`, `Lfo.hpp`, `Effects.hpp`, `WaveTables.hpp`, `Simd.hpp` | `EngineCoreTests`, `compare.py` / `compare_sy1.py` |
| Список/диапазоны/умолчания 99 параметров | `EngineCore/Params.hpp` | `EngineCoreTests/ParamsTests.cpp` |
| Формат `.sy1`, загрузка/сохранение, legacy-версии | `EngineCore/Sy1Codec.hpp` + `LegacyTables.hpp` (генерируется) | `EngineCoreTests` (`legacy_fixture.txt`), `check_legacy.py` |
| Банки, zip-импорт, перенос/копирование/удаление патчей | `EngineCore/PatchLibrary.hpp` | `EngineCoreTests/PatchLibraryTests.cpp` |
| MIDI learn, CC-масштабирование | `EngineCore/MidiLearn.hpp` + `SynthEditorCore` | `MidiLearnTests.cpp`, `tools/au_harness` |
| MIDI program change | `EngineCore/ProgramChange.hpp` + `SynthEditorCore::SyncProgramBank` | `ProgramChangeTests.cpp` |
| Состояние в сессии хоста | `EditorCore/State.hpp` | `EngineCoreTests` (state round-trip) |
| Пути, настройки, темы, корзина | `EditorCore/Storage.hpp`, `Settings.hpp`, `Themes.hpp`, `Trash.hpp` | `SettingsTests.cpp`, `ThemesTests.cpp` |
| Панель, ручки, LCD, меню, темы | `Shared/SynthPanelView.swift`, `SynthControls.swift`, `SynthModel.swift`, `SynthTheme.swift` | `tools/ui_snapshot`, `ThemesTests` (паритет с C++) |
| Браузер банков, write-диалог, iPad-файлы | `Shared/SynthLibraryViews.swift`, `SynthLibrary.swift` | `ui_snapshot` режимы `write-*`, ручная проверка |
| VST3: хост-интеграция, параметры, state, программы | `OxoSynth2VST3/source/*` | `validator`, `auval` |
| AUv3 / iPad | `OxoSynth2AudioUnit/DSP/*`, `AudioUnit/*` | `tools/au_harness`, сборка симулятора |
| Windows VST3 | `Windows_Build/Source/*` | `build_windows.bat`, `tools/win_check` |
| Идентичность, версия, имена бандлов | `Identity.xcconfig` (+ целое `version` в plist'ах) | сборка + `validator` |
| Новое измерение оригинала / пересборка таблиц | `tools/s1ref/measure_*.py` → `reference/meta/*.json` → `gen_tables.py` | `EngineCore/MeasuredTables.hpp` (только генерация) |
| Сравнение с оригиналом | `tools/s1ref/compare.py`, `compare_sy1.py`, `tools/s2render` | отчёты level / spec / env |
| Производительность | `tools/bench/main.cpp`, `build_windows.bat --bench` | бенч + инструкции / `time -l` |
| Инсталляторы | `tools/prepare_mac_installer`, `tools/prepare_win_installer.bat` | `dist/mac`, `dist/win` |
| Документация | `docs/docs.toml`, `docs/README.md` | `make docs-check` |

Генерируемые файлы правим только через генератор: `MeasuredTables.hpp` / `LegacyTables.hpp`
(`tools/s1ref/gen_tables.py`, `gen_legacy.py`), `OxoSynth2Identity.h` (из `Identity.xcconfig`),
служебные фрагменты документов (`scripts/doc_system.py sync`).

## Сборка и артефакты

| Команда | Платформа | Результат |
| --- | --- | --- |
| `./build_vst3.sh` | macOS | `OxoSynth2VST3/build/VST3/Release/OxoSynth2.vst3` + `Oxo Synth2.component`; ставит в `~/Library/Audio/Plug-Ins`. `--no-install` — только сборка |
| `xcodebuild -project OxoSynth2.xcodeproj -scheme OxoSynth2AudioUnit ... build` | macOS (iPad Simulator) | `.app` + AUv3 appex в DerivedData |
| `build_windows.bat [--msvc] [--bench]` | Windows | `Windows_Build\build\VST3\Release\OxoSynth2.vst3` + тесты хранилища; ничего не устанавливает |
| `make test` | macOS / Linux (C++20) | `EngineCoreTests/build/engine_core_tests` |
| `make bench` / `synth2_bench.exe` | macOS / Windows | % одного ядра на сценариях и патчах |
| `make -C tools/s2render` | macOS | рендер job'ов нашим движком (сравнение) |
| `make -C tools/s1ref` | macOS x86_64 + Rosetta + оригинал | `build/s1ref/s1ref`, эталонные рендеры |
| `tools/prepare_mac_installer` | macOS | `dist/mac/OxoSynth2-<ver>-<build>-macOS.{pkg,dmg}` |
| `tools\prepare_win_installer.bat` | Windows | `dist/win/<Product>-<ver>-<build>-Windows-x64.{exe,zip}` |

Полные команды, требования и диагностика — [development.md](development.md).

## Данные и пути

| Что | Где |
| --- | --- |
| Патч | `.sy1` — текст, 99 значений в порядке Synth1; имя в первой строке |
| Состояние инстанса | чанк `'OXS2'` v4 (volume, program slot, bank, theme, `.sy1`) |
| Настройки, MIDI-карта, темы (mac) | `~/Library/Application Support/Oxo Music/Oxo Synth2` |
| Библиотека банков (mac) | `~/Music/Oxo Music/Oxo Synth2` |
| iPad | контейнер appex + app group `group.com.oxo.music.oxosynth2`, банки в `Synth2` |
| Windows | `%APPDATA%\Oxo Music\Oxo Synth2`; portable: `Synth2_Data` рядом с `.vst3` |
| Встроенные банки | `Shared/Resources/Synth2 Factory` (read-only, 001 = INIT), `Synth2 Starter Banks` |
| Измерения | `reference/meta/*.json`, `reference/meta/nativemap.txt` |

Идентификаторы, имена бандлов и формат данных — контракт совместимости; см.
[architecture.md](architecture.md) § Инварианты. Историю решений фиксируют
[ADR](adr/README.md).

<!-- agents:footer:start — генерируется scripts/doc_system.py sync; не редактировать вручную -->
> 🤖 Проверено: 2026-11-13 · база: 14852b7. Правила ведения документации — [AGENTS.md](../AGENTS.md).
<!-- agents:footer:end -->
