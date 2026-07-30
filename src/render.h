/* render.h — desenho do formulário RAQ.

   O desenho é feito uma única vez, através da interface DrawBackend. O mesmo
   código alimenta o PDF salvo e o DC da impressora, então papel e arquivo
   nunca divergem.

   Coordenadas seguem a convenção do PDF: origem no canto inferior esquerdo,
   unidade em pontos. O backend da impressora converte internamente. */
#ifndef RAQ_RENDER_H
#define RAQ_RENDER_H

#include "model.h"

typedef struct DrawBackend {
    void *ctx;
    void  (*linha)(void *ctx, double x1, double y1, double x2, double y2,
                   double espessura);
    void  (*texto)(void *ctx, double x, double y, int negrito, double tam,
                   const char *winansi);
    double (*largura)(void *ctx, int negrito, double tam, const char *winansi);
} DrawBackend;

#define RAQ_PAGINA_LARGURA 594.96
#define RAQ_PAGINA_ALTURA  842.04

void raq_desenha(const DrawBackend *b, const Formulario *f);

#endif /* RAQ_RENDER_H */
