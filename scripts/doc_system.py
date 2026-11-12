#!/usr/bin/env python3
"""doc-system runtime — самообновляемая документация проекта.

Документация ведётся как код: манифест docs/docs.toml описывает, какие файлы обязаны
существовать, из каких исходников они вытекают и когда их проверяли; этот скрипт
проверяет инварианты, перегенерирует служебные фрагменты и сдвигает отметки проверки.

Это копия, установленная пакетом doc-system: обновляйте её через
`python3 <path-to-doc-system>/bin/init.py --upgrade <project>`, а не правьте вручную.

Команды:
    check [--fast] [--strict]   проверить структуру, ссылки, полноту и свежесть
    sync                        перегенерировать футеры, индекс и файлы-указатели
    touch PATH [PATH ...]       отметить документы проверенными (текущий HEAD)
    touch --all                 отметить все неисторические документы (docs-baseline)
    stale [--strict]            показать устаревшие документы
    version                     версия runtime и версия, записанная в манифесте

Код возврата: 0 — успех, 1 — ошибки (или предупреждения при --strict), 2 — ошибка запуска.
"""

from __future__ import annotations

import argparse
import datetime as dt
import fnmatch
import os
import re
import subprocess
import sys
import time
import tomllib
from pathlib import Path
from urllib.parse import unquote

RUNTIME_VERSION = "1.0.0"

ROOT = Path(__file__).resolve().parent.parent
MANIFEST_PATH = ROOT / "docs" / "docs.toml"
DOCS_README = ROOT / "docs" / "README.md"

# Небольшой допуск на округление при сравнении времени отметки и mtime.
MTIME_SLACK = 0.05

FOOTER_START = "<!-- agents:footer:start"
FOOTER_END = "<!-- agents:footer:end -->"
INDEX_START = "<!-- docs:index:start -->"
INDEX_END = "<!-- docs:index:end -->"

LINK_RE = re.compile(r"!?\[[^\]]*\]\(\s*([^)\s]+)(?:\s+\"[^\"]*\")?\s*\)")
HEADING_RE = re.compile(r"^(#{1,6})\s+(.+?)\s*#*\s*$")
VERIFIED_RE = re.compile(r'^verified\s*=\s*\{.*\}\s*$')
PATH_LINE_RE = r'^path\s*=\s*"%s"\s*$'
VERIFIED_LINE = (
    'verified = {{ commit = "{commit}", date = "{date}", time = {time:.3f} }}'
)


# ---------------------------------------------------------------------------
# Загрузка манифеста
# ---------------------------------------------------------------------------


def load_manifest() -> tuple[dict | None, list[str]]:
    if not MANIFEST_PATH.is_file():
        return None, [f"нет манифеста документации: {rel(MANIFEST_PATH)}"]
    try:
        data = tomllib.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    except tomllib.TOMLDecodeError as exc:
        return None, [f"docs/docs.toml не разбирается: {exc}"]
    return data, []


def raw_docs(manifest: dict) -> list[dict]:
    """Все блоки [[doc]] как есть — для валидации манифеста."""
    return [doc for doc in manifest.get("doc", []) if isinstance(doc, dict)]


def docs_of(manifest: dict) -> list[dict]:
    """Только корректные записи: без path остальные проверки не выполняются."""
    return [
        doc
        for doc in raw_docs(manifest)
        if isinstance(doc.get("path"), str) and doc["path"]
    ]


def pointers_of(manifest: dict) -> list[dict]:
    return [
        pointer
        for pointer in manifest.get("pointer", [])
        if isinstance(pointer, dict)
        and isinstance(pointer.get("template"), str)
        and isinstance(pointer.get("target"), str)
    ]


def meta_of(manifest: dict) -> dict:
    return manifest.get("meta", {})


# ---------------------------------------------------------------------------
# Утилиты
# ---------------------------------------------------------------------------


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def rel_link(from_file: Path, target: Path) -> str:
    return os.path.relpath(target, from_file.parent).replace(os.sep, "/")


def git(*args: str) -> str | None:
    try:
        result = subprocess.run(
            ["git", *args], cwd=ROOT, capture_output=True, text=True, check=False
        )
    except FileNotFoundError:
        return None
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def git_toplevel() -> Path | None:
    out = git("rev-parse", "--show-toplevel")
    return Path(out).resolve() if out else None


def in_project_repo() -> bool:
    """Проект — корень git-репозитория (а не подкаталог чужого)."""
    top = git_toplevel()
    return top is not None and top == ROOT.resolve()


def head_short() -> str | None:
    if not in_project_repo():
        return None
    return git("rev-parse", "--short", "HEAD") or None


def commit_exists(sha: str) -> bool:
    return git("rev-parse", "--verify", "--quiet", f"{sha}^{{commit}}") is not None


def changed_files(sha: str) -> list[str] | None:
    """Файлы, изменённые относительно коммита sha (включая незакоммиченные правки)."""
    out = git("diff", "--name-only", "-z", sha)
    if out is None:
        return None
    files = {p for p in out.split("\0") if p}
    files |= dirty_paths()
    return sorted(files)


def dirty_paths() -> set[str]:
    """Пути с незакоммиченными изменениями, включая неотслеживаемые файлы."""
    out = git("status", "--porcelain")
    if out is None:
        return set()
    result: set[str] = set()
    for line in out.splitlines():
        if len(line) < 4:
            continue
        path = line[3:].split(" -> ")[-1].strip()
        if path:
            result.add(path)
    return result


def normalize_generated(path: str, text: str) -> str:
    """Убрать служебные фрагменты, чтобы отметка touch не выглядела как правка."""
    if path.endswith(".md"):
        text = re.sub(
            r"<!-- agents:footer:start.*?<!-- agents:footer:end -->",
            "",
            text,
            flags=re.S,
        )
    if path == "docs/docs.toml":
        text = "\n".join(
            line for line in text.splitlines() if not VERIFIED_RE.match(line.strip())
        )
    return text.strip()


def is_substantively_dirty(path: str) -> bool:
    """Файл изменён не только служебными вставками (футер, verified-строки)."""
    full = ROOT / path
    try:
        current = normalize_generated(path, full.read_text(encoding="utf-8"))
    except OSError:
        return False
    head = git("show", f"HEAD:{path}")
    if head is None:
        return True  # новый/неотслеживаемый файл
    return current != normalize_generated(path, head)


def is_substantively_changed(path: str, base: str) -> bool:
    """Содержимое файла изменилось относительно коммита base, а не только служебные вставки."""
    full = ROOT / path
    try:
        current: str | None = normalize_generated(
            path, full.read_text(encoding="utf-8")
        )
    except OSError:
        current = None
    original = git("show", f"{base}:{path}")
    if original is None:
        return current is not None
    if current is None:
        return True  # файл удалён
    return current != normalize_generated(path, original)


def file_change_time(path: str, dirty: set[str]) -> float:
    """Момент последнего изменения файла: mtime для содержательных правок, иначе коммит."""
    full = ROOT / path
    if path in dirty and is_substantively_dirty(path):
        try:
            return full.stat().st_mtime
        except OSError:
            return 0.0
    out = git("log", "-1", "--format=%ct", "--", path)
    if out and out.isdigit():
        return float(out)
    try:
        return full.stat().st_mtime
    except OSError:
        return 0.0


def doc_change_time(path: str, dirty: set[str]) -> float:
    """Время обновления документа: touch/правка в рабочем дереве считается проверкой."""
    if path in dirty:
        try:
            return (ROOT / path).stat().st_mtime
        except OSError:
            return 0.0
    return file_change_time(path, dirty)


def strip_code(text: str) -> str:
    """Убрать fenced-блоки и инлайн-код: ссылки внутри них не проверяем."""
    text = re.sub(r"```.*?```", "", text, flags=re.S)
    text = re.sub(r"~~~.*?~~~", "", text, flags=re.S)
    return re.sub(r"`[^`\n]*`", "", text)


def headings(text: str) -> list[str]:
    result: list[str] = []
    in_fence = False
    fence = ""
    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith("```") or stripped.startswith("~~~"):
            marker = stripped[:3]
            if not in_fence:
                in_fence, fence = True, marker
            elif marker == fence:
                in_fence = False
            continue
        if in_fence:
            continue
        match = HEADING_RE.match(line)
        if match:
            result.append(match.group(2).strip())
    return result


# ---------------------------------------------------------------------------
# Генерируемые фрагменты
# ---------------------------------------------------------------------------


def render_footer(doc: dict) -> str:
    path = ROOT / doc["path"]
    agents = rel_link(path, ROOT / "AGENTS.md")
    if doc.get("immutable") and doc["path"].startswith("docs/adr/"):
        body = (
            "> 📜 ADR — историческая запись: решение не переписывается. "
            f"Актуальные правила — [AGENTS.md]({agents})."
        )
    else:
        verified = doc.get("verified", {})
        body = (
            f"> 🤖 Проверено: {verified.get('date', '—')} · база: "
            f"{verified.get('commit', '—')}. Правила ведения документации — "
            f"[AGENTS.md]({agents})."
        )
    header = (
        f"{FOOTER_START} — генерируется scripts/doc_system.py sync; "
        "не редактировать вручную -->"
    )
    return f"{header}\n{body}\n{FOOTER_END}\n"


def render_index(manifest: dict) -> str:
    rows = ["| Документ | О чём | Проверено |", "| --- | --- | --- |"]
    for doc in docs_of(manifest):
        path = doc["path"]
        target = ROOT / path
        link = rel_link(DOCS_README, target)
        date = doc.get("verified", {}).get("date", "—")
        title = doc.get("title") or path
        rows.append(f"| [{title}]({link}) · `{path}` | {doc.get('summary', '')} | {date} |")
    return "\n".join(rows)


def apply_footer(text: str, footer: str) -> str:
    lines = text.splitlines()
    start = next(
        (i for i, line in enumerate(lines) if line.lstrip().startswith(FOOTER_START)),
        None,
    )
    if start is None:
        return text.rstrip() + "\n\n" + footer
    end = next(
        (i for i in range(start + 1, len(lines)) if FOOTER_END in lines[i]), None
    )
    if end is None:
        raise ValueError("начало футера есть, а конец не найден")
    return "\n".join(lines[:start] + footer.rstrip("\n").splitlines() + lines[end + 1:]) + "\n"


def current_footer(text: str) -> str | None:
    lines = text.splitlines()
    start = next(
        (i for i, line in enumerate(lines) if line.lstrip().startswith(FOOTER_START)),
        None,
    )
    if start is None:
        return None
    end = next(
        (i for i in range(start + 1, len(lines)) if FOOTER_END in lines[i]), None
    )
    if end is None:
        return None
    return "\n".join(lines[start : end + 1])


def replace_region(text: str, start_marker: str, end_marker: str, content: str) -> str:
    start = text.find(start_marker)
    if start < 0:
        raise ValueError(f"не найден маркер {start_marker}")
    end = text.find(end_marker, start + len(start_marker))
    if end < 0:
        raise ValueError(f"не найден маркер {end_marker}")
    return f"{text[:start]}{start_marker}\n{content.strip()}\n{text[end:]}"


def region(text: str, start_marker: str, end_marker: str) -> str | None:
    start = text.find(start_marker)
    if start < 0:
        return None
    end = text.find(end_marker, start + len(start_marker))
    if end < 0:
        return None
    return text[start + len(start_marker) : end].strip()


# ---------------------------------------------------------------------------
# check
# ---------------------------------------------------------------------------


def check_manifest(manifest: dict, errors: list[str]) -> None:
    meta = meta_of(manifest)
    for required in meta.get("required_files", []):
        if not (ROOT / required).is_file():
            errors.append(f"нет обязательного файла: {required}")
    seen: set[str] = set()
    for doc in raw_docs(manifest):
        path = doc.get("path")
        if not isinstance(path, str) or not path:
            errors.append("в [[doc]] отсутствует path")
            continue
        if path in seen:
            errors.append(f"дубликат документа в манифесте: {path}")
        seen.add(path)
        for key in ("title", "summary"):
            if not doc.get(key):
                errors.append(f"{path}: в манифесте нет обязательного поля {key}")
        if not isinstance(doc.get("sources"), list):
            errors.append(f"{path}: sources должен быть списком")
        if not isinstance(doc.get("sections"), list):
            errors.append(f"{path}: sections должен быть списком")
        verified = doc.get("verified")
        if not isinstance(verified, dict) or "date" not in verified or "commit" not in verified:
            errors.append(f"{path}: verified должен содержать commit и date")
        elif not re.fullmatch(r"\d{4}-\d{2}-\d{2}", str(verified["date"])):
            errors.append(f"{path}: verified.date должен быть в формате YYYY-MM-DD")
        target = ROOT / path
        if not target.is_file():
            errors.append(f"{path}: файл из манифеста не существует")
    for pointer in manifest.get("pointer", []):
        if not isinstance(pointer, dict) or not pointer.get("template") or not pointer.get("target"):
            errors.append("[[pointer]] должен содержать template и target")
    templates = {p.get("template") for p in pointers_of(manifest)}

    for template in templates:
        if template and not (ROOT / template).is_file():
            errors.append(f"нет шаблона указателя: {template}")
    targets = {p.get("target") for p in pointers_of(manifest)}
    for target in targets:
        if target in seen:
            errors.append(f"{target}: файл-указатель не может быть документом манифеста")


def check_coverage(manifest: dict, errors: list[str]) -> None:
    listed = {doc["path"] for doc in docs_of(manifest) if doc.get("path")}
    targets = {p.get("target") for p in pointers_of(manifest)}
    for path in sorted((ROOT / "docs").rglob("*.md")):
        if rel(path) not in listed:
            errors.append(
                f"{rel(path)}: документ не зарегистрирован в docs/docs.toml"
            )
    # Корневые .md проверяются только если проект этого хочет ([meta] root_coverage).
    # По умолчанию false: в чужом проекте уже есть README/AGENTS, которые не обязаны
    # быть частью doc-system.
    if not meta_of(manifest).get("root_coverage", False):
        return
    for path in sorted(ROOT.glob("*.md")):
        name = path.name
        if name not in listed and name not in targets:
            errors.append(f"{name}: документ не зарегистрирован в docs/docs.toml")


def check_sections(manifest: dict, errors: list[str]) -> None:
    for doc in docs_of(manifest):
        path = ROOT / doc["path"]
        if not path.is_file():
            continue
        found = [h.casefold() for h in headings(read(path))]
        joined = "\n".join(found)
        for section in doc.get("sections", []):
            if section.casefold() not in joined:
                errors.append(f"{doc['path']}: нет обязательного раздела «{section}»")


def check_footers(manifest: dict, errors: list[str], stats: dict) -> None:
    for doc in docs_of(manifest):
        path = ROOT / doc["path"]
        if not path.is_file():
            continue
        stats["docs"] += 1
        text = read(path)
        actual = current_footer(text)
        if actual is None:
            errors.append(f"{doc['path']}: нет сгенерированного футера — python3 scripts/doc_system.py sync")
            continue
        if actual.strip() != render_footer(doc).strip():
            errors.append(f"{doc['path']}: футер устарел — python3 scripts/doc_system.py sync")


def check_index(manifest: dict, errors: list[str]) -> None:
    if not DOCS_README.is_file():
        errors.append("нет docs/README.md")
        return
    text = read(DOCS_README)
    actual = region(text, INDEX_START, INDEX_END)
    if actual is None:
        errors.append("docs/README.md: нет маркеров индекса docs:index — python3 scripts/doc_system.py sync")
        return
    if actual != render_index(manifest).strip():
        errors.append("docs/README.md: индекс документации устарел — python3 scripts/doc_system.py sync")


def check_pointers(manifest: dict, errors: list[str], stats: dict) -> None:
    for pointer in pointers_of(manifest):
        template = ROOT / pointer.get("template", "")
        target = ROOT / pointer.get("target", "")
        stats["pointers"] += 1
        if not template.is_file():
            errors.append(f"нет шаблона указателя: {pointer.get('template')}")
            continue
        if not target.is_file():
            errors.append(
                f"{pointer.get('target')}: файл-указатель отсутствует — python3 scripts/doc_system.py sync"
            )
            continue
        if read(template) != read(target):
            errors.append(
                f"{pointer.get('target')}: файл-указатель устарел — python3 scripts/doc_system.py sync"
            )


def markdown_slug(text: str) -> str:
    """Slug заголовка по правилам GitHub: пунктуация удаляется, пробелы → дефисы."""
    text = text.strip().lower()
    text = re.sub(r"[^\w\s-]", "", text, flags=re.UNICODE)
    text = text.replace("`", "")
    return re.sub(r"\s+", "-", text).strip("-")


def check_links(manifest: dict, errors: list[str], stats: dict) -> None:
    files = {ROOT / doc["path"] for doc in docs_of(manifest)}
    files |= {ROOT / p["target"] for p in pointers_of(manifest)}
    for path in sorted(files):
        if not path.is_file() or path.suffix not in {".md", ".mdc"}:
            continue
        text = strip_code(read(path))
        for match in LINK_RE.finditer(text):
            target = unquote(match.group(1))
            if not target or target.startswith(("#", "/", "http://", "https://", "mailto:")):
                continue
            if "{" in target:
                continue
            path_part, _, anchor = target.partition("#")
            if not path_part:
                continue
            resolved = (path.parent / path_part).resolve()
            stats["links"] += 1
            if not resolved.exists():
                errors.append(f"{rel(path)}: ссылка ведёт в никуда: {target}")
                continue
            if anchor and resolved.suffix in {".md", ".mdc"}:
                slugs = {markdown_slug(h) for h in headings(read(resolved))}
                if markdown_slug(anchor) not in slugs:
                    errors.append(f"{rel(path)}: якорь не найден: {target}")


def check_handoff(manifest: dict, warnings: list[str]) -> None:
    meta = meta_of(manifest)
    handoff_path = meta.get("handoff_path")
    if not handoff_path:
        return
    path = ROOT / handoff_path
    if not path.is_file():
        warnings.append(f"{handoff_path}: журнал handoff отсутствует")
        return
    soft_days = int(meta.get("handoff_soft_days", 30))
    dates = [
        dt.date.fromisoformat(m.group(1))
        for m in re.finditer(r"^##\s+(\d{4}-\d{2}-\d{2})", read(path), re.M)
    ]
    if not dates:
        warnings.append(
            f"{handoff_path}: нет ни одной записи формата «## YYYY-MM-DD»"
        )
        return
    age = (dt.date.today() - max(dates)).days
    if age > soft_days:
        warnings.append(
            f"{handoff_path}: последняя запись {age} дн. назад (порог {soft_days})"
        )


def doc_staleness(
    doc: dict, manifest: dict, head: str | None, baseline_note: bool = True
) -> list[str]:
    """Вернуть список предупреждений об устаревании конкретного документа."""
    messages: list[str] = []
    if doc.get("immutable"):
        return messages
    path = doc["path"]
    verified = doc.get("verified", {})
    base = str(verified.get("commit", "")).strip()
    date_text = str(verified.get("date", ""))
    max_age = int(meta_of(manifest).get("max_age_days", 90))

    try:
        verified_date = dt.date.fromisoformat(date_text)
    except ValueError:
        verified_date = None
    if verified_date is not None:
        age = (dt.date.today() - verified_date).days
        if age > max_age:
            messages.append(
                f"{path}: не проверялся {age} дн. (порог {max_age}) — перечитайте и выполните touch"
            )

    if not head:
        return messages  # в репозитории ещё нет коммитов — база будет зафиксирована позже
    if base in ("", "none"):
        if baseline_note:
            messages.append(
                f"{path}: база проверки не зафиксирована — выполните python3 scripts/doc_system.py touch --all"
            )
        return messages
    if not commit_exists(base):
        messages.append(
            f"{path}: коммит проверки {base} не найден (история переписана?) — перепроверьте документ и touch"
        )
        return messages

    changed = changed_files(base)
    if changed is None:
        messages.append(f"{path}: не удалось сравнить с коммитом {base}")
        return messages
    sources = [s for s in doc.get("sources", []) if isinstance(s, str)]
    if not sources:
        return messages
    dirty = dirty_paths()
    # Момент touch хранится в манифесте: это детерминированный маркер проверки,
    # не зависящий от порядка записи файлов при sync и от коммитов.
    stamp = verified.get("time")
    doc_time = float(stamp) if isinstance(stamp, (int, float)) else doc_change_time(path, dirty)
    hits = sorted(
        {
            file
            for file in changed
            if any(fnmatch.fnmatchcase(file, pattern) for pattern in sources)
            and is_substantively_changed(file, base)
            and file_change_time(file, dirty) > doc_time + MTIME_SLACK
        }
    )
    if hits:
        shown = ", ".join(hits[:3]) + (" и др." if len(hits) > 3 else "")
        messages.append(
            f"{path}: после проверки ({base}) изменился код: {shown} — перечитайте и выполните touch"
        )
    return messages


# ---------------------------------------------------------------------------
# sync
# ---------------------------------------------------------------------------


def sync_all(quiet: bool = False) -> list[str]:
    manifest, errors = load_manifest()
    if manifest is None:
        return errors

    # Сначала строим полный план изменений в памяти, и только если ошибок нет —
    # записываем. Так частично синхронизированное дерево не остаётся после сбоя.
    files: dict[Path, str] = {}

    def load(path: Path) -> str:
        if path not in files:
            files[path] = read(path)
        return files[path]

    for doc in docs_of(manifest):
        path = ROOT / doc["path"]
        if not path.is_file():
            errors.append(f"{doc['path']}: файл из манифеста не существует")
            continue
        try:
            files[path] = apply_footer(load(path), render_footer(doc))
        except ValueError as exc:
            errors.append(f"{doc['path']}: {exc}")

    if not DOCS_README.is_file():
        errors.append("docs/README.md: файл не существует")
    else:
        try:
            files[DOCS_README] = replace_region(
                load(DOCS_README), INDEX_START, INDEX_END, render_index(manifest)
            )
        except ValueError as exc:
            errors.append(f"docs/README.md: {exc}")

    for pointer in pointers_of(manifest):
        template = ROOT / pointer["template"]
        target = ROOT / pointer["target"]
        if not template.is_file():
            errors.append(f"нет шаблона указателя: {pointer['template']}")
            continue
        files[target] = read(template)

    if errors:
        return errors  # план неполный — ничего не пишем

    changed: list[str] = []
    for path, new_text in files.items():
        old_text = read(path) if path.is_file() else None
        if old_text != new_text:
            write(path, new_text)
            changed.append(rel(path))

    if not quiet:
        if changed:
            print("Синхронизировано:")
            for item in changed:
                print(f"  · {item}")
        else:
            print("Синхронизация не требуется: всё совпадает с манифестом.")
    return errors


# ---------------------------------------------------------------------------
# touch
# ---------------------------------------------------------------------------


def normalize_path(raw: str) -> str:
    path = Path(raw).as_posix()
    while path.startswith("./"):
        path = path[2:]
    return path


def set_verified(
    text: str, path: str, commit: str, date_text: str, stamp: float
) -> str:
    lines = text.splitlines(keepends=True)
    starts = [i for i, line in enumerate(lines) if line.strip() == "[[doc]]"]
    starts.append(len(lines))
    path_re = re.compile(PATH_LINE_RE % re.escape(path), re.M)
    verified_line = VERIFIED_LINE.format(commit=commit, date=date_text, time=stamp) + "\n"
    for idx in range(len(starts) - 1):
        begin, end = starts[idx], starts[idx + 1]
        block = "".join(lines[begin:end])
        if not path_re.search(block):
            continue
        for j in range(begin, end):
            if VERIFIED_RE.match(lines[j]):
                lines[j] = verified_line
                return "".join(lines)
        # verified отсутствует — вставляем после полного массива sources,
        # в том числе после закрывающей скобки многострочного массива.
        for j in range(begin, end):
            stripped = lines[j].strip()
            if not stripped.startswith("sources"):
                continue
            insert_at = j + 1
            if "[" in stripped and "]" not in stripped:
                k = j + 1
                while k < end and "]" not in lines[k]:
                    k += 1
                if k >= end:
                    raise ValueError(
                        f"{path}: не найден конец массива sources в docs/docs.toml"
                    )
                insert_at = k + 1
            lines.insert(insert_at, verified_line)
            return "".join(lines)
        raise ValueError(f"{path}: в блоке манифеста нет полей verified/sources")
    raise ValueError(f"{path}: документ не найден в docs/docs.toml")


def cmd_version(_args: argparse.Namespace) -> int:
    manifest, _errors = load_manifest()
    project_version = "-"
    if manifest is not None:
        project_version = str(meta_of(manifest).get("system_version", "-"))
    print(f"doc-system runtime {RUNTIME_VERSION}; в манифесте {project_version}")
    return 0


def cmd_sync(_args: argparse.Namespace) -> int:
    errors = sync_all()
    if errors:
        print_errors(errors)
        return 1
    return 0


def cmd_touch(args: argparse.Namespace) -> int:
    manifest, errors = load_manifest()
    if manifest is None:
        print_errors(errors)
        return 1
    known = {doc["path"] for doc in docs_of(manifest)}
    paths = [normalize_path(p) for p in args.paths]
    if args.all:
        paths += [doc["path"] for doc in docs_of(manifest) if not doc.get("immutable")]
    ordered = list(dict.fromkeys(paths))
    if not ordered:
        print("Укажите документы: touch PATH ... или touch --all", file=sys.stderr)
        return 2
    unknown = [p for p in ordered if p not in known]
    if unknown:
        print(f"Не зарегистрированы в манифесте: {', '.join(unknown)}", file=sys.stderr)
        return 2

    commit = head_short() or "none"
    today = dt.date.today().isoformat()
    stamp = time.time()
    text = read(MANIFEST_PATH)
    try:
        for path in ordered:
            text = set_verified(text, path, commit, today, stamp)
        tomllib.loads(text)  # не записываем заведомо битый манифест
    except (ValueError, tomllib.TOMLDecodeError) as exc:
        print(f"Ошибка: {exc}. Манифест не изменён.", file=sys.stderr)
        return 1
    write(MANIFEST_PATH, text)

    synced = sync_all()
    print(f"Отмечено проверенным ({commit}, {today}):")
    for path in ordered:
        print(f"  · {path}")
    if synced:
        print_errors(synced)
        return 1
    return 0


# ---------------------------------------------------------------------------
# stale
# ---------------------------------------------------------------------------


def cmd_stale(args: argparse.Namespace) -> int:
    manifest, errors = load_manifest()
    if manifest is None:
        print_errors(errors)
        return 1
    head = head_short()
    warnings: list[str] = []
    for doc in docs_of(manifest):
        warnings.extend(doc_staleness(doc, manifest, head))
    check_handoff(manifest, warnings)
    if warnings:
        print("Требуют внимания:")
        for message in warnings:
            print(f"  ! {message}")
    else:
        print("Все документы актуальны.")
    if args.strict and warnings:
        return 1
    return 0


# ---------------------------------------------------------------------------
# check
# ---------------------------------------------------------------------------


def print_errors(errors: list[str]) -> None:
    if errors:
        print("Ошибки:")
        for error in errors:
            print(f"  ✗ {error}")


def cmd_check(args: argparse.Namespace) -> int:
    manifest, errors = load_manifest()
    if manifest is None:
        print_errors(errors)
        return 1
    warnings: list[str] = []
    stats = {"docs": 0, "pointers": 0, "links": 0}

    system_version = str(meta_of(manifest).get("system_version", "")).strip()
    if system_version and system_version != RUNTIME_VERSION:
        warnings.append(
            f"docs/docs.toml: версия doc-system {system_version}, runtime {RUNTIME_VERSION} — "
            "запустите init.py --upgrade для проекта"
        )

    check_manifest(manifest, errors)
    check_coverage(manifest, errors)
    check_sections(manifest, errors)
    check_footers(manifest, errors, stats)
    check_index(manifest, errors)
    check_pointers(manifest, errors, stats)
    check_links(manifest, errors, stats)
    check_handoff(manifest, warnings)
    if not args.fast:
        top = git_toplevel()
        if top is not None and top != ROOT.resolve() and not in_project_repo():
            warnings.append(
                "проект не является корнем git-репозитория — проверка свежести по коммитам отключена"
            )
        head = head_short()
        if head:
            # База проверки обязательна, как только в репозитории появился коммит:
            # иначе предупреждения о свежести не работают, а CI остаётся зелёным.
            for doc in docs_of(manifest):
                base = str(doc.get("verified", {}).get("commit", "")).strip()
                if not doc.get("immutable") and base in ("", "none"):
                    errors.append(
                        f"{doc['path']}: база проверки не зафиксирована — выполните python3 scripts/doc_system.py touch --all"
                    )
        for doc in docs_of(manifest):
            warnings.extend(
                doc_staleness(doc, manifest, head, baseline_note=False)
            )

    if warnings:
        print("Предупреждения:")
        for warning in warnings:
            print(f"  ! {warning}")
    print_errors(errors)
    print(
        f"Итог: документов {stats['docs']}, указателей {stats['pointers']}, "
        f"ссылок {stats['links']}, ошибок {len(errors)}, предупреждений {len(warnings)}."
    )
    if errors:
        return 1
    if args.strict and warnings:
        return 1
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="doc_system.py",
        description="doc-system: проверка и поддержание самообновляемой документации.",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    check = sub.add_parser("check", help="проверить документацию")
    check.add_argument(
        "--fast", action="store_true", help="без проверки свежести по git (для pre-commit)"
    )
    check.add_argument(
        "--strict",
        action="store_true",
        help="предупреждения считать ошибками (аудит)",
    )
    check.set_defaults(func=cmd_check)

    sync = sub.add_parser("sync", help="перегенерировать служебные фрагменты")
    sync.set_defaults(func=cmd_sync)

    touch = sub.add_parser("touch", help="отметить документы проверенными")
    touch.add_argument("paths", nargs="*", help="пути документов из манифеста")
    touch.add_argument(
        "--all",
        action="store_true",
        dest="all",
        help="отметить все неисторические документы (docs-baseline)",
    )
    touch.set_defaults(func=cmd_touch)

    stale = sub.add_parser("stale", help="показать устаревшие документы")
    stale.add_argument(
        "--strict",
        action="store_true",
        help="ненулевой код возврата при наличии устаревших",
    )
    stale.set_defaults(func=cmd_stale)

    version = sub.add_parser("version", help="показать версии runtime и манифеста")
    version.set_defaults(func=cmd_version)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except (OSError, ValueError) as exc:
        print(f"Ошибка: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
