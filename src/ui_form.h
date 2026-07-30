#ifndef RAQ_UI_FORM_H
#define RAQ_UI_FORM_H

#include "model.h"
#include <windows.h>

/* Cria e exibe a janela principal do formulário. Devolve NULL em caso de erro. */
HWND ui_form_cria(HINSTANCE inst, Store *store);

/* Recarrega combos e cabeçalho depois que o admin mexeu nos cadastros. */
void ui_form_recarrega(HWND janela);

#endif /* RAQ_UI_FORM_H */
