# Участие в разработке Oxo Synth2

Правила для людей и ИИ-агентов. Сначала прочитай [AGENTS.md](AGENTS.md) — там рабочий цикл,
Definition of Done и карта репозитория. Документация — [docs/README.md](docs/README.md).

## Перед началом

1. Установи окружение по [docs/development.md](docs/development.md).
2. Подтяни сабмодули (`git submodule update --init --recursive`) и собери свою платформу:
   `./build_vst3.sh` (macOS) или `build_windows.bat` (Windows).
3. Прочитай [docs/architecture.md](docs/architecture.md) и [docs/roadmap.md](docs/roadmap.md).
4. Проверь исходное состояние: `make -C EngineCoreTests test` и
   `python3 scripts/doc_system.py check`.

## Локальные проверки

```bash
make -C EngineCoreTests test                   # обязательный шлюз
python3 scripts/doc_system.py check            # если меняли только документацию
python3 scripts/doc_system.py stale            # что требует перепроверки
```

Меняли код — добавь тесты. Меняли поведение — запись в `CHANGELOG.md` → `[Unreleased]`.
Меняли документы — отметь проверку:
`python3 scripts/doc_system.py touch <пути>`.

## Коммиты

Один коммит — одно логическое изменение. Сообщение на английском, в стиле истории репозитория:

- **subject** — короткое императивное предложение без префикса типа (`Filters: …`,
  `Version 1.1.5 (build 10)`); заглавная буква, без точки в конце;
- пустая строка;
- **тело** — зачем и что именно, с фактами и измерениями (числа, затронутые патчи, до/после),
  если они есть; без пересказа диффа;
- без AI-трейлеров и подписей инструментов.

```text
Filters: the original's output limiter after the SVF and the ladder

Replace the per-filter AGC with one output limiter fitted on the original
(3.7 ms up, 6.9 ms down, 1 below 0.39); 446 of 686 library patches change,
none worse by more than 0.5 dB.
```

Перед коммитом `make -C EngineCoreTests test` должен быть зелёным, для изменений движка — есть
отчёт сравнения рендеров, для плагина — `validator` / `auval`. Git-хуков в репозитории нет:
проверки запускаются вручную.

## Документация и решения

- Документы ведутся как код: обновляй их в том же изменении, что и код (карта — в `AGENTS.md`).
- Существенное решение — новый ADR по процессу в [docs/adr/README.md](docs/adr/README.md).
- В описании PR: что, зачем, как проверено, какие документы обновлены. Шаблона PR в репозитории
  нет — структура свободная, важны факты и способ проверки.
- Состояние работы между сессиями — [docs/agents/handoff.md](docs/agents/handoff.md).

<!-- agents:footer:start — генерируется scripts/doc_system.py sync; не редактировать вручную -->
> 🤖 Проверено: 2026-11-13 · база: 14852b7. Правила ведения документации — [AGENTS.md](AGENTS.md).
<!-- agents:footer:end -->
