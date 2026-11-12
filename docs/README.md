# Документация проекта Oxo Synth2

Документация ведётся по принципу **docs-as-code**: тот же репозиторий, то же ревью, те же
проверки. Правила для агентов — в [AGENTS.md](../AGENTS.md), здесь — карта документов и
правила поддержания.

## Порядок чтения

1. [AGENTS.md](../AGENTS.md) — контракт: приоритеты, рабочий цикл, DoD, запреты.
2. [architecture.md](architecture.md) — как устроен код и какие инварианты держать.
3. [project-map.md](project-map.md) — карта репозитория: что где лежит и с чего начать задачу.
4. [roadmap.md](roadmap.md) — что сделано и что дальше.
5. [development.md](development.md) — команды, тесты, сборки и диагностика.
6. [adr/](adr/README.md) — почему приняты ключевые решения.
7. [agents/workflow.md](agents/workflow.md) — пошаговый процесс работы агента.
8. [agents/doc-maintenance.md](agents/doc-maintenance.md) — как устроено самообновление.
9. [agents/handoff.md](agents/handoff.md) — состояние последних сессий.

## Карта документации

Таблица генерируется из [docs.toml](docs.toml) командой
`python3 scripts/doc_system.py sync`; вручную её не редактируем.

<!-- docs:index:start -->
| Документ | О чём | Проверено |
| --- | --- | --- |
| [Участие в разработке](../CONTRIBUTING.md) · `CONTRIBUTING.md` | Правила для людей: подготовка, проверки, коммиты, документы | 2026-11-13 |
| [История изменений](../CHANGELOG.md) · `CHANGELOG.md` | Живой журнал изменений в формате Keep a Changelog | 2026-11-13 |
| [Индекс документации](README.md) · `docs/README.md` | С чего начать чтение и как поддерживать документы | 2026-11-13 |
| [Архитектура](architecture.md) · `docs/architecture.md` | Текущее состояние, целевая структура, инварианты, открытые вопросы | 2026-11-13 |
| [Карта проекта](project-map.md) · `docs/project-map.md` | Что где лежит, с чего начать типовую задачу, какие выходят артефакты | 2026-11-13 |
| [Разработка](development.md) · `docs/development.md` | Команды, тесты, сборки и диагностика | 2026-11-13 |
| [Дорожная карта](roadmap.md) · `docs/roadmap.md` | Снимок состояния и планируемые шаги | 2026-11-13 |
| [Рабочий цикл агента](agents/workflow.md) · `docs/agents/workflow.md` | Пошаговый процесс: ориентация, изменения, проверка, отчёт | 2026-11-13 |
| [Поддержание документации](agents/doc-maintenance.md) · `docs/agents/doc-maintenance.md` | Устройство self-updating docs: манифест, команды, политика актуальности | 2026-11-13 |
| [Handoff — журнал передачи работы](agents/handoff.md) · `docs/agents/handoff.md` | Живой журнал: что сделано, что открыто, что делать дальше | 2026-11-13 |
| [ADR — оглавление и процесс](adr/README.md) · `docs/adr/README.md` | Как фиксировать архитектурные решения и как их читать | 2026-11-13 |
| [ADR-0001: фиксируем решения через ADR](adr/0001-record-architecture-decisions.md) · `docs/adr/0001-record-architecture-decisions.md` | Решение использовать лёгкие Architecture Decision Records | 2026-11-12 |
| [ADR-0002: чистая комната](adr/0002-clean-room-via-audio.md) · `docs/adr/0002-clean-room-via-audio.md` | Оригинал изучается только по аудио и опубликованному AU-интерфейсу | 2026-11-13 |
| [ADR-0003: движок без JUCE](adr/0003-no-juce-header-only-engine.md) · `docs/adr/0003-no-juce-header-only-engine.md` | Header-only C++20 движок и нативные оболочки вместо фреймворка | 2026-11-13 |
<!-- docs:index:end -->

## Как поддерживать документацию

- Изменил код → обнови документ по карте «что менял → что обновить» в `AGENTS.md`.
- После проверки документа: `python3 scripts/doc_system.py touch <путь>` (или
  `make docs-touch ARGS="..."`).
- Пользовательское изменение → `CHANGELOG.md` → `[Unreleased]`.
- Неочевидное решение → ADR по шаблону [adr/README.md](adr/README.md).
- Проверка: `python3 scripts/doc_system.py check`; список устаревшего — `... stale`.
  Сгенерировать служебные фрагменты — `... sync`.

Подробности и политика актуальности — [agents/doc-maintenance.md](agents/doc-maintenance.md).

## Как добавить новый документ

1. Создай `.md`-файл в `docs/` (или подкаталоге). Любой `.md` внутри `docs/**` должен быть
   зарегистрирован в манифесте — иначе проверка считает это ошибкой.
2. Добавь запись `[[doc]]` в [docs.toml](docs.toml): `path`, `title`, `summary`, `sources`,
   `sections`, `verified = { commit = "none", date = "YYYY-MM-DD", time = 0.0 }`.
3. Выполни `python3 scripts/doc_system.py sync` — появится футер и строка индекса.
4. Добавь ссылки в этот файл и, если документ важен при старте, в `AGENTS.md`.
5. `python3 scripts/doc_system.py check` должен пройти.

Переименование/удаление: обнови манифест и все ссылки (проверит `check`); причину удаления
зафиксируй в ADR или handoff.

<!-- agents:footer:start — генерируется scripts/doc_system.py sync; не редактировать вручную -->
> 🤖 Проверено: 2026-11-13 · база: 14852b7. Правила ведения документации — [AGENTS.md](../AGENTS.md).
<!-- agents:footer:end -->
