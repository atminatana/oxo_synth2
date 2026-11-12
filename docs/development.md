# Разработка Oxo Synth2

Практическое руководство: сборка, проверки, тесты, инструменты. Правила процесса — в
[AGENTS.md](../AGENTS.md), устройство кода — в [architecture.md](architecture.md), где что лежит —
в [project-map.md](project-map.md).

## Команды

| Команда | Платформа | Что делает |
| --- | --- | --- |
| `git submodule update --init --recursive` | все | подтянуть `third_party/vst3sdk` и `third_party/AudioUnitSDK` (без них CMake падает) |
| `./build_vst3.sh` | macOS | собрать VST3 + AUv2 и установить в `~/Library/Audio/Plug-Ins` |
| `./build_vst3.sh --no-install` | macOS | только собрать (для инсталлятора: не плодить копии в `~/Library`) |
| `xcodebuild -project OxoSynth2.xcodeproj -scheme OxoSynth2AudioUnit -destination 'generic/platform=iOS Simulator' CODE_SIGNING_ALLOWED=NO build` | macOS | iPad-приложение + AUv3 appex (команда целиком — в `AGENTS.md` § Builds and tests) |
| `build_windows.bat` | Windows | VST3 x64 компилятором clang-cl в `Windows_Build\build` + тесты хранилища |
| `build_windows.bat --msvc` | Windows | то же MSVC (в `Windows_Build\build-msvc`, для сравнения кода) |
| `build_windows.bat --bench` | Windows | собрать и прогнать `tools/bench` на компиляторе плагина |
| `make test` | macOS / Linux | псевдоним `make -C EngineCoreTests test` |
| `make -C EngineCoreTests test` | macOS / Linux | обязательный шлюз: все C++-тесты движка и переносимых сервисов |
| `make bench` | macOS | `clang++ -std=c++20 -O2 tools/bench/main.cpp -o build/bench && ./build/bench` |
| `make -C tools/s2render` | macOS | рендер job'ов нашим движком (сравнение с оригиналом) |
| `make -C tools/s1ref` | macOS x86_64 | харнесс оригинала (нужен Synth1 и Rosetta) |
| `tools/win_check/check.sh` | macOS + mingw-w64 | compile-check Windows-исходников (предупреждения = ошибки) |
| `tools/prepare_mac_installer` | macOS | `.pkg` + `.dmg` в `dist/mac` |
| `tools\prepare_win_installer.bat` | Windows | `.exe` + `.zip` в `dist\win` (нужен Inno Setup 6.3+) |
| `python3 scripts/doc_system.py check` | все | проверка документации: манифест, разделы, ссылки, якоря, свежесть |
| `python3 scripts/doc_system.py sync` | все | перегенерировать футеры и индекс документов |
| `python3 scripts/doc_system.py stale` | все | список потенциально устаревших документов |
| `python3 scripts/doc_system.py touch <пути>` | все | отметить документы проверенными (после чтения, не вместо) |
| `make docs-check` / `make docs-sync` / `make docs-touch ARGS="..."` | все | то же через `docs.mk` |

Требования к окружению:

- **macOS**: Xcode (полный, не только Command Line Tools — `build_vst3.sh` задаёт
  `DEVELOPER_DIR`), CMake (берётся из Xcode toolchain / `PATH`). Минимальная ОС продукта —
  macOS 14, сборка универсальная (arm64 + x86_64).
- **iPad**: Xcode с iOS 17 SDK; сборка под симулятор не требует подписи
  (`CODE_SIGNING_ALLOWED=NO`), установка на устройство — требует team в
  `Identity.local.xcconfig`.
- **Windows**: Visual Studio 2022+ с workload «Desktop development with C++» и компонентами
  «C++ Clang Compiler for Windows» + «MSBuild support for LLVM (clang-cl) toolset»; CMake из
  `%CMAKE%`, `PATH` или из Visual Studio. Инсталлятор — Inno Setup 6.3+.
- **Тесты и документация на Linux/macOS**: `clang++` с C++20 (цель `EngineCoreTests`
  рассчитана на него), `zlib` (`-lz`), утилита `zip` в `PATH` (тесты банков собирают zip
  внешней командой), Python ≥ 3.11 (для `tomllib` в doc-system).
- **Измерения оригинала**: только macOS: оригинальный `Synth1.component` (x86_64) + Rosetta.
  Python-скрипты — из venv: `python3 -m venv .venv && .venv/bin/pip install numpy scipy`
  (см. [tools/s1ref/README.md](../tools/s1ref/README.md)).

## Структура проекта

Дерево с назначением каталогов и таблица «задача → файлы» — в
[project-map.md](project-map.md). Кратко:

```text
EngineCore/        header-only C++20 движок oxo::s2 (без платформенного кода)
EditorCore/        переносимые сервисы редактора: state, storage, settings, themes, C-хуки
Shared/            SwiftUI-панель (mac + iPad) и встроенные банки
OxoSynth2VST3/     mac VST3 + AUv2-обёртка (CMake)
OxoSynth2AudioUnit/, OxoSynth2/, OxoSynth2.xcodeproj/   iPad AUv3 + приложение-оболочка
Windows_Build/     Windows x64 VST3 (CMake, Win32/GDI+)
EngineCoreTests/   C++-тесты (make test)
tools/             s1ref, s2render, bench, au_harness, ui_snapshot, инсталляторы
reference/meta/    закоммиченные измерения; EngineCore/*Tables.hpp генерируются из них
third_party/       сабмодули SDK; не редактируются
```

## Тесты

- **`make -C EngineCoreTests test`** — полный шлюз для движка и переносимых сервисов.
  Флаги: `-std=c++20 -Wall -Wextra -Werror -O1 -g`, линкуется с `-lz` (на Darwin —
  `CoreFoundation`). Раннер — `EngineCoreTests/build/engine_core_tests`; `make -C
  EngineCoreTests clean` чистит.
- Покрытие по файлам: `ParamsTests` (99 параметров, диапазоны, формат значений),
  `PatchLibraryTests` (банки, слоты, zip, import / move / copy / delete, read-only factory),
  `MidiLearnTests`, `ProgramChangeTests`, `SettingsTests`, `ThemesTests` (парсит
  `Shared/SynthTheme.swift` — паритет C++/Swift), `UpdateCheckTests` (парсит
  `Shared/SynthUpdateCheck.swift`), `SynthMidiTests`, `LfoTests`, плюс `legacy_fixture.txt`
  (значения реальных legacy-патчей без имён).
- **Чистая комната и звук**: поведение сверяется с оригиналом рендерами, а не тестами:
  `tools/s1ref/compare.py <group> [substr]` (группы `presets`, `sweeps`, `pitch`, `filter`,
  `velocity`), `compare_sy1.py <folder>` для произвольной папки, `check_fm.py`, `check_eq.py`,
  `check_unison.py`, `check_legacy.py`. Перед сравнением пересобрать `tools/s2render`.
  Любое изменение движка: отрендерить все фабричные / стартовые / библиотечные патчи до и
  после и сравнить (правило в `AGENTS.md`).
- **Плагин в хосте**: `OxoSynth2VST3/build/bin/Release/validator <bundle>` (ожидается 47/47;
  запускать с `HOME=$(mktemp -d)`, чтобы валидатор не трогал личную библиотеку) и
  `auval -v aumu OXS2 OXOM`.
- **AUv3-мост**: `tools/au_harness` (команда сборки — в `main.mm`): рендер, MIDI 1 и UMP,
  MIDI learn, state, хуки библиотеки.
- **Windows**: `build_windows.bat` прогоняет тесты хранилища; перед отправкой на Windows —
  `tools/win_check/check.sh` (компиляция mingw-w64, предупреждения = ошибки).
- **UI**: `tools/ui_snapshot` рендерит панель в PNG с mock-хостом (режимы `logo`, `write-*`,
  `ipad`, `ipad-wide`); `auhost.swift` открывает установленный AUv2 и снимает настоящий
  редактор.
- Требование к сдаче: `make -C EngineCoreTests test` зелёный; для изменений движка — отчёт
  сравнения рендеров; для плагина — `validator` / `auval`.

## Хуки и CI

- В репозитории **нет CI и git-хуков**: каталогов `.github/` и `.githooks/` нет. Шлюз —
  локальный: `make -C EngineCoreTests test` + `python3 scripts/doc_system.py check`.
- `docs.mk` — необязательные make-цели для документации (`make docs-check`, `docs-sync`,
  `docs-stale`, `docs-touch ARGS="..."`); подключается строкой `-include docs.mk` в корневом
  `Makefile`.
- doc-system установлен с `--no-hooks`: pre-commit не ставился. Если хуки понадобятся,
  ставьте их отдельно и опишите здесь; не добавляйте проверки, которые нельзя запустить
  локально.
- Релизные проверки — перед тегом: `validator` 47/47, `auval`, сборка инсталляторов
  (`tools/prepare_mac_installer`, `tools\prepare_win_installer.bat`), ручной smoke-тест в
  хосте. Версия и номер сборки — в `Identity.xcconfig`; один номер сборки = один коммит,
  тег не переиспользуется.

## Добавление зависимости

1. Проверь, нельзя ли обойтись стандартной библиотекой / уже подключёнными средствами
   (движок сознательно header-only, без JUCE — см. [ADR-0003](adr/0003-no-juce-header-only-engine.md)).
2. Согласуй с пользователем: зачем, лицензия (проект MIT; сабмодули — MIT / Apache 2.0),
   цена поддержки и влияние на iPad/Windows.
3. Если решение архитектурное — оформи ADR по [docs/adr/README.md](adr/README.md).
4. Обнови `docs/architecture.md` (зависимости, границы), `docs/development.md` (сборка) и
   `CHANGELOG.md` → `[Unreleased]`.
5. `third_party/` — только git submodule; никогда не правь его файлы. Правка SDK — строковый
   патч по сгенерированной копии в `OxoSynth2VST3/CMakeLists.txt` с `FATAL_ERROR`, если якорь
   исчез.

## Диагностика

| Симптом | Решение |
| --- | --- |
| CMake: нет `vstaudioeffect.cpp` / `funknown.h` | `git submodule update --init --recursive` |
| `build_vst3.sh`: `xcodebuild` не найден | установить полный Xcode; скрипт задаёт `DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer` |
| `validator` падает на личной библиотеке | запускать с `HOME=$(mktemp -d)` |
| `auval` не видит плагин | `./build_vst3.sh` без `--no-install`; проверить `~/Library/Audio/Plug-Ins/Components` |
| Две копии плагина в валидаторе | удалить dev-копии из `~/Library` перед тестом `.pkg` (у pkg свои UID) |
| Windows: сборка «другим» компилятором | каталог `Windows_Build\build` помнит toolset: `build_windows.bat` переконфигурирует через CMake `--fresh` |
| iPad: 0 предупреждений в нашем коде | `CLANG_WARN_COMMA = NO` — намеренно (компактные запятые движка) |
| `Shared/` перестал собираться под iOS | typecheck-команда из `AGENTS.md` § Rules; mac-код — только под `#if os(macOS)` |
| `docs-check`: база проверки не зафиксирована | один раз `python3 scripts/doc_system.py touch --all` |
| `docs-check`: футер / индекс устарел | `python3 scripts/doc_system.py sync` |
| `docs-check`: «документ не зарегистрирован» | добавить `[[doc]]` в `docs/docs.toml` |
| `docs-check`: «ссылка ведёт в никуда» / «якорь не найден» | исправить путь или заголовок |
| `docs-check`: «изменился код» | перечитать документ и `touch` |
| `make test`: `zip: command not found`, падение `PatchLibraryTests` | установить `zip` (тесты вызывают его как внешнюю команду) |
| `make test` под GCC: `-Werror=format-truncation` в `EngineCore/ProgramChange.hpp` | цель рассчитана на `clang++`; для локальной проверки — `CXX=clang++` или сборка без `-Werror` |
| `ModuleNotFoundError: tomllib` | нужен Python ≥ 3.11 |
| версия doc-system в манифесте не совпадает с runtime | `python3 <path-to-doc-system>/bin/init.py --upgrade <project>` |

<!-- agents:footer:start — генерируется scripts/doc_system.py sync; не редактировать вручную -->
> 🤖 Проверено: 2026-11-13 · база: 14852b7. Правила ведения документации — [AGENTS.md](../AGENTS.md).
<!-- agents:footer:end -->
