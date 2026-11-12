# Changelog

Все заметные изменения проекта фиксируются здесь. Формат основан на
[Keep a Changelog](https://keepachangelog.com/ru/1.1.0/), версии — по
[Semantic Versioning](https://semver.org/lang/ru/).

Правила:

- изменения пользовательского поведения и публичных команд попадают в `[Unreleased]` в том же
  изменении, что и код;
- категории: `Added`, `Changed`, `Deprecated`, `Removed`, `Fixed`, `Security`;
- записи — для пользователя: что изменилось и зачем, без пересказа диффа;
- историю не переписываем: ошибку исправляем новой записью.

## [Unreleased]

### Added

- Подключён doc-system v1.0.0: самообновляемая документация (`docs/`),
  манифест `docs/docs.toml`, runtime `scripts/doc_system.py`, правила для ИИ-агентов
  (`AGENTS.md`), ADR-процесс и журнал handoff.

## История до doc-system

Изменения кода и звука до подключения документации — в истории git
(`git log --oneline`); формат `Keep a Changelog` ведётся с этого момента.

<!-- agents:footer:start — генерируется scripts/doc_system.py sync; не редактировать вручную -->
> 🤖 Проверено: 2026-11-13 · база: 14852b7. Правила ведения документации — [AGENTS.md](AGENTS.md).
<!-- agents:footer:end -->
