#ifndef RAQ_BACKEND_GDI_H
#define RAQ_BACKEND_GDI_H

#include "model.h"
#include <windows.h>

/* Imprime o formulário. `impressora` vazio ou NULL usa a padrão do Windows.
   Devolve 0 em caso de sucesso. */
int raq_imprime(const Formulario *f, HWND pai, const char *impressora);

/* Nome da impressora que será realmente usada, em UTF-8: `impressora` se
   estiver preenchida, senão a padrão do Windows. Devolve 0 se não houver
   nenhuma impressora instalada. */
int raq_impressora_em_uso(const char *impressora, char *dst, int dst_bytes);

/* Lista as impressoras instaladas, incluindo as de rede. Devolve quantas. */
int raq_lista_impressoras(char nomes[][256], int max);

#endif /* RAQ_BACKEND_GDI_H */
