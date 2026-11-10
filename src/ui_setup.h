#ifndef RAQ_UI_SETUP_H
#define RAQ_UI_SETUP_H

#include "model.h"
#include <windows.h>

/* Assistente da primeira execução: senha do administrador, dados do posto,
   produtos, analistas, distribuidores, transportadores, pasta dos PDFs e
   impressora. Grava tudo no fim. Devolve 1 se concluído e gravado, 0 se o
   usuário cancelou (nada é gravado). */
int ui_setup_executa(HINSTANCE inst, Store *store);

#endif /* RAQ_UI_SETUP_H */
