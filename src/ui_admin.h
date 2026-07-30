#ifndef RAQ_UI_ADMIN_H
#define RAQ_UI_ADMIN_H

#include "model.h"
#include <windows.h>

/* Pede a senha e, se conferir, abre a janela de administração.
   Devolve 1 se algo foi alterado e a tela principal precisa recarregar. */
int ui_admin_abre(HWND pai, Store *store);

#endif /* RAQ_UI_ADMIN_H */
