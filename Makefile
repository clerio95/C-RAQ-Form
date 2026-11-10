# RAQ Form Filler
#
#   make            -> compila o .exe do Windows (cross-compile com mingw-w64)
#   make test       -> compila e roda os testes no Linux (PDF + utilidades)
#   make widths     -> regenera src/pdf_widths.h a partir dos AFM da Helvetica
#   make clean
#
#   make VERSION=1.2.0  -> carimba a versão no .exe (o workflow de release usa
#                          a tag vX.Y.Z)

CC_WIN  := x86_64-w64-mingw32-gcc
RC_WIN  := x86_64-w64-mingw32-windres
CC_HOST := gcc

VERSION ?= 1.1.0
comma   := ,
VERSAO_NUM := $(subst .,$(comma),$(VERSION)),0

SRC_DIR := src
BUILD   := build
DIST    := dist

CFLAGS_COMUM := -std=c99 -Wall -Wextra -O2 -I$(SRC_DIR)

# -mwindows: sem console atrás da janela.
# Link estático para o .exe rodar em qualquer Windows sem instalar nada.
CFLAGS_WIN := $(CFLAGS_COMUM) -municode -mwindows
LDFLAGS_WIN := -static -static-libgcc -mwindows \
               -lgdi32 -lcomdlg32 -lcomctl32 -lwinspool -lshell32 -lole32 -luser32 -lkernel32

# Fontes comuns às duas plataformas.
SRC_NUCLEO := $(SRC_DIR)/util.c $(SRC_DIR)/sha256.c $(SRC_DIR)/pdf.c $(SRC_DIR)/render.c \
              $(SRC_DIR)/backend_pdf.c $(SRC_DIR)/store.c

SRC_WIN := $(SRC_NUCLEO) $(SRC_DIR)/win_util.c $(SRC_DIR)/backend_gdi.c \
           $(SRC_DIR)/ui_form.c $(SRC_DIR)/ui_admin.c $(SRC_DIR)/ui_setup.c \
           $(SRC_DIR)/main.c

EXE := $(DIST)/RAQ.exe

.PHONY: all windows test widths clean FORCE

all: windows

windows: $(EXE)

RES := $(BUILD)/raq_res.o

VERSAO_H := $(BUILD)/versao.h

# Regrava só quando a versão muda, para não recompilar à toa.
$(VERSAO_H): FORCE
	@mkdir -p $(BUILD)
	@printf '#define RAQ_VERSAO_STR "%s"\n#define RAQ_VERSAO_NUM %s\n' \
	    '$(VERSION)' '$(VERSAO_NUM)' > $@.tmp
	@cmp -s $@.tmp $@ && rm -f $@.tmp || mv $@.tmp $@

$(RES): res/raq.rc res/raq.manifest $(VERSAO_H)
	@mkdir -p $(BUILD)
	$(RC_WIN) -I res -I $(BUILD) -i res/raq.rc -o $@

$(EXE): $(SRC_WIN) $(RES) $(VERSAO_H) $(wildcard $(SRC_DIR)/*.h)
	@mkdir -p $(DIST)
	$(CC_WIN) $(CFLAGS_WIN) -I$(BUILD) -o $@ $(SRC_WIN) $(RES) $(LDFLAGS_WIN)
	@echo "gerado: $@ (versão $(VERSION))"

test: $(BUILD)/test_pdf $(BUILD)/test_store
	./$(BUILD)/test_pdf
	./$(BUILD)/test_store

$(BUILD)/test_pdf: tests/test_pdf.c $(SRC_NUCLEO) $(wildcard $(SRC_DIR)/*.h)
	@mkdir -p $(BUILD)
	$(CC_HOST) $(CFLAGS_COMUM) -pedantic -o $@ tests/test_pdf.c $(SRC_NUCLEO)

$(BUILD)/test_store: tests/test_store.c $(SRC_NUCLEO) $(wildcard $(SRC_DIR)/*.h)
	@mkdir -p $(BUILD)
	$(CC_HOST) $(CFLAGS_COMUM) -pedantic -o $@ tests/test_store.c $(SRC_NUCLEO)

FORCE:

widths:
	python3 tools/gen_widths.py

clean:
	rm -rf $(BUILD) $(DIST)
