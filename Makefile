# RAQ Form Filler
#
#   make            -> compila o .exe do Windows (cross-compile com mingw-w64)
#   make test       -> compila e roda os testes no Linux (PDF + utilidades)
#   make widths     -> regenera src/pdf_widths.h a partir dos AFM da Helvetica
#   make clean

CC_WIN  := x86_64-w64-mingw32-gcc
RC_WIN  := x86_64-w64-mingw32-windres
CC_HOST := gcc

SRC_DIR := src
BUILD   := build
DIST    := dist

CFLAGS_COMUM := -std=c99 -Wall -Wextra -O2 -I$(SRC_DIR)

# -mwindows: sem console atrás da janela.
# Link estático para o .exe rodar em qualquer Windows sem instalar nada.
CFLAGS_WIN := $(CFLAGS_COMUM) -municode -mwindows
LDFLAGS_WIN := -static -static-libgcc -mwindows \
               -lgdi32 -lcomdlg32 -lcomctl32 -lwinspool -lole32 -luser32 -lkernel32

# Fontes comuns às duas plataformas.
SRC_NUCLEO := $(SRC_DIR)/util.c $(SRC_DIR)/pdf.c $(SRC_DIR)/render.c \
              $(SRC_DIR)/backend_pdf.c $(SRC_DIR)/store.c

SRC_WIN := $(SRC_NUCLEO) $(SRC_DIR)/win_util.c $(SRC_DIR)/backend_gdi.c \
           $(SRC_DIR)/ui_form.c $(SRC_DIR)/ui_admin.c $(SRC_DIR)/main.c

EXE := $(DIST)/RAQ.exe

.PHONY: all windows test widths clean

all: windows

windows: $(EXE)

RES := $(BUILD)/raq_res.o

$(RES): res/raq.rc res/raq.manifest
	@mkdir -p $(BUILD)
	$(RC_WIN) -I res -i res/raq.rc -o $@

$(EXE): $(SRC_WIN) $(RES) $(wildcard $(SRC_DIR)/*.h)
	@mkdir -p $(DIST)
	$(CC_WIN) $(CFLAGS_WIN) -o $@ $(SRC_WIN) $(RES) $(LDFLAGS_WIN)
	@echo "gerado: $@"

test: $(BUILD)/test_pdf $(BUILD)/test_store
	./$(BUILD)/test_pdf
	./$(BUILD)/test_store

$(BUILD)/test_pdf: tests/test_pdf.c $(SRC_NUCLEO) $(wildcard $(SRC_DIR)/*.h)
	@mkdir -p $(BUILD)
	$(CC_HOST) $(CFLAGS_COMUM) -pedantic -o $@ tests/test_pdf.c $(SRC_NUCLEO)

$(BUILD)/test_store: tests/test_store.c $(SRC_NUCLEO) $(wildcard $(SRC_DIR)/*.h)
	@mkdir -p $(BUILD)
	$(CC_HOST) $(CFLAGS_COMUM) -pedantic -o $@ tests/test_store.c $(SRC_NUCLEO)

widths:
	python3 tools/gen_widths.py

clean:
	rm -rf $(BUILD) $(DIST)
