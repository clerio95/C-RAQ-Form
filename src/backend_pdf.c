/* backend_pdf.c — implementa DrawBackend escrevendo num Pdf. */
#include "backend_pdf.h"

static void bp_linha(void *ctx, double x1, double y1, double x2, double y2,
                     double espessura)
{
    pdf_linha((Pdf *)ctx, x1, y1, x2, y2, espessura);
}

static void bp_texto(void *ctx, double x, double y, int negrito, double tam,
                     const char *s)
{
    pdf_texto((Pdf *)ctx, x, y, negrito ? PDF_FONT_BOLD : PDF_FONT_REGULAR,
              tam, s);
}

static double bp_largura(void *ctx, int negrito, double tam, const char *s)
{
    (void)ctx;
    return pdf_largura_texto(negrito ? PDF_FONT_BOLD : PDF_FONT_REGULAR, tam, s);
}

int raq_gera_pdf(const Formulario *f, const char *caminho)
{
    DrawBackend b;
    Pdf *p;
    int r;

    p = pdf_novo(RAQ_PAGINA_LARGURA, RAQ_PAGINA_ALTURA);
    if (!p) return -1;

    b.ctx     = p;
    b.linha   = bp_linha;
    b.texto   = bp_texto;
    b.largura = bp_largura;

    raq_desenha(&b, f);
    r = pdf_salva(p, caminho);
    pdf_libera(p);
    return r;
}
