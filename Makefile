# Oxo Synth2 — общие цели разработки и документации.
#
#   make test         C++-тесты движка и переносимых сервисов (macOS / Linux)
#   make bench        CPU-бенч движка (macOS / Linux, 16 голосов)
#   make build        mac VST3 + AUv2 через ./build_vst3.sh (macOS)
#   make docs-check   проверка документации (см. docs.mk)
#   make help         список целей
#
# Windows собирается отдельно: build_windows.bat (см. docs/development.md).

-include docs.mk

CXX ?= clang++
CXXFLAGS ?= -std=c++20 -O2

.PHONY: all help build test bench clean

all: help

help: ## Показать доступные цели
	@echo "Доступные цели:"
	@echo "  make test         — тесты движка (EngineCoreTests)"
	@echo "  make bench        — CPU-бенч движка"
	@echo "  make build        — mac VST3 + AUv2 (build_vst3.sh)"
	@echo "  make docs-check   — проверить документацию"
	@echo "  make docs-stale   — список устаревших документов"
	@echo "  make clean        — убрать артефакты тестов и бенча"

build: ## Собрать mac VST3 + AUv2 и установить (build_vst3.sh)
	./build_vst3.sh

test: ## Прогнать тесты движка и переносимых сервисов
	$(MAKE) -C EngineCoreTests test

bench: ## Собрать и запустить CPU-бенч движка
	mkdir -p build
	$(CXX) $(CXXFLAGS) tools/bench/main.cpp -o build/bench
	./build/bench

clean: ## Убрать артефакты тестов и бенча
	$(MAKE) -C EngineCoreTests clean
	rm -rf build
