# doc-system: дополнительные make-цели.
# Подключение (необязательно): добавьте в Makefile строку
#   -include docs.mk
# и вызывайте `make docs-check` и т.п.

PY ?= python3

.PHONY: docs-check docs-sync docs-stale docs-touch docs-baseline docs-version

docs-check: ## Проверить документацию (структура, ссылки, свежесть)
	$(PY) scripts/doc_system.py check

docs-sync: ## Перегенерировать футеры и индекс документов
	$(PY) scripts/doc_system.py sync

docs-stale: ## Показать документы, требующие перепроверки
	$(PY) scripts/doc_system.py stale

docs-touch: ## Отметить документы проверенными: make docs-touch ARGS="docs/architecture.md"
	@test -n "$(ARGS)" || { echo 'Укажите документы: make docs-touch ARGS="docs/architecture.md"'; exit 1; }
	$(PY) scripts/doc_system.py touch $(ARGS)

docs-baseline: ## Один раз после первого коммита: зафиксировать базу проверки
	$(PY) scripts/doc_system.py touch --all

docs-version: ## Версия doc-system
	$(PY) scripts/doc_system.py version
