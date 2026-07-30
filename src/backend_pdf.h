#ifndef RAQ_BACKEND_PDF_H
#define RAQ_BACKEND_PDF_H

#include "model.h"
#include "pdf.h"
#include "render.h"

/* Desenha o formulário e grava o PDF. Devolve 0 em caso de sucesso. */
int raq_gera_pdf(const Formulario *f, const char *caminho);

#endif /* RAQ_BACKEND_PDF_H */
