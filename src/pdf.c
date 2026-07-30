#include "pdf.h"
#include "pdf_widths.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Buffer que cresce sozinho — o conteúdo da página é montado aqui. */
typedef struct {
    char  *dados;
    size_t tam, cap;
} Buf;

struct Pdf {
    Buf    conteudo;
    double largura, altura;
    double espessura_atual;
    int    erro;
};

static void buf_reserva(Buf *b, size_t extra)
{
    size_t nova;
    char *p;
    if (b->tam + extra + 1 <= b->cap) return;
    nova = b->cap ? b->cap : 4096;
    while (nova < b->tam + extra + 1) nova *= 2;
    p = realloc(b->dados, nova);
    if (!p) return;                     /* sinalizado adiante por tam parado */
    b->dados = p;
    b->cap = nova;
}

static void buf_add(Buf *b, const char *s, size_t n)
{
    buf_reserva(b, n);
    if (b->tam + n + 1 > b->cap) return;
    memcpy(b->dados + b->tam, s, n);
    b->tam += n;
    b->dados[b->tam] = '\0';
}

static void buf_printf(Buf *b, const char *fmt, ...)
{
    char tmp[512];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n > 0) buf_add(b, tmp, (size_t)n < sizeof tmp ? (size_t)n : sizeof tmp - 1);
}

Pdf *pdf_novo(double largura, double altura)
{
    Pdf *p = calloc(1, sizeof *p);
    if (!p) return NULL;
    p->largura = largura;
    p->altura  = altura;
    p->espessura_atual = -1.0;
    return p;
}

void pdf_libera(Pdf *p)
{
    if (!p) return;
    free(p->conteudo.dados);
    free(p);
}

static void define_espessura(Pdf *p, double w)
{
    if (w == p->espessura_atual) return;
    buf_printf(&p->conteudo, "%.2f w\n", w);
    p->espessura_atual = w;
}

void pdf_linha(Pdf *p, double x1, double y1, double x2, double y2, double espessura)
{
    if (!p) return;
    define_espessura(p, espessura);
    buf_printf(&p->conteudo, "%.2f %.2f m %.2f %.2f l S\n", x1, y1, x2, y2);
}

void pdf_retangulo(Pdf *p, double x, double y, double w, double h, double espessura)
{
    if (!p) return;
    define_espessura(p, espessura);
    buf_printf(&p->conteudo, "%.2f %.2f %.2f %.2f re S\n", x, y, w, h);
}

void pdf_texto(Pdf *p, double x, double y, PdfFont fonte, double tam, const char *s)
{
    const unsigned char *q;

    if (!p || !s || !*s) return;

    buf_printf(&p->conteudo, "BT /F%d %.2f Tf %.2f %.2f Td (",
               fonte == PDF_FONT_BOLD ? 2 : 1, tam, x, y);

    for (q = (const unsigned char *)s; *q; q++) {
        if (*q == '(' || *q == ')' || *q == '\\') {
            char esc[2] = { '\\', (char)*q };
            buf_add(&p->conteudo, esc, 2);
        } else if (*q < 32) {
            buf_printf(&p->conteudo, "\\%03o", *q);
        } else {
            buf_add(&p->conteudo, (const char *)q, 1);
        }
    }
    buf_add(&p->conteudo, ") Tj ET\n", 8);
}

double pdf_largura_texto(PdfFont fonte, double tam, const char *s)
{
    const short *tab = (fonte == PDF_FONT_BOLD) ? largura_helvetica_bold
                                                : largura_helvetica;
    const unsigned char *q;
    long total = 0;

    if (!s) return 0.0;
    for (q = (const unsigned char *)s; *q; q++)
        total += (*q >= 32) ? tab[*q - 32] : 0;
    return (double)total * tam / 1000.0;
}

/* ---------- gravação ---------- */

#define N_OBJETOS 6

int pdf_salva(Pdf *p, const char *caminho)
{
    FILE *f;
    long off[N_OBJETOS + 1];
    long inicio_xref;
    int i;

    if (!p || !caminho) return -1;
    f = fopen(caminho, "wb");
    if (!f) return -1;

    fputs("%PDF-1.4\n%\xE2\xE3\xCF\xD3\n", f);

    off[1] = ftell(f);
    fputs("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n", f);

    off[2] = ftell(f);
    fputs("2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n", f);

    off[3] = ftell(f);
    fprintf(f,
            "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.2f %.2f]\n"
            "   /Resources << /Font << /F1 5 0 R /F2 6 0 R >> >>\n"
            "   /Contents 4 0 R >>\nendobj\n",
            p->largura, p->altura);

    off[4] = ftell(f);
    fprintf(f, "4 0 obj\n<< /Length %lu >>\nstream\n",
            (unsigned long)p->conteudo.tam);
    if (p->conteudo.tam)
        fwrite(p->conteudo.dados, 1, p->conteudo.tam, f);
    fputs("endstream\nendobj\n", f);

    off[5] = ftell(f);
    fputs("5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica"
          " /Encoding /WinAnsiEncoding >>\nendobj\n", f);

    off[6] = ftell(f);
    fputs("6 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold"
          " /Encoding /WinAnsiEncoding >>\nendobj\n", f);

    inicio_xref = ftell(f);
    fprintf(f, "xref\n0 %d\n", N_OBJETOS + 1);
    fputs("0000000000 65535 f \n", f);
    for (i = 1; i <= N_OBJETOS; i++)
        fprintf(f, "%010ld 00000 n \n", off[i]);

    fprintf(f,
            "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",
            N_OBJETOS + 1, inicio_xref);

    if (ferror(f)) { fclose(f); return -1; }
    return fclose(f) == 0 ? 0 : -1;
}
