#ifndef RAQ_BACKEND_GDI_H
#define RAQ_BACKEND_GDI_H

#include "model.h"
#include <windows.h>

/* Imprime o formulário na impressora padrão. Devolve 0 em caso de sucesso. */
int raq_imprime(const Formulario *f, HWND pai);

#endif /* RAQ_BACKEND_GDI_H */
