/* pdf.h — escritor de PDF mínimo: uma página A4, linhas e texto.
   Usa as fontes base Helvetica com WinAnsiEncoding, que já cobrem todo o
   português, então nada precisa ser embutido no arquivo. */
#ifndef RAQ_PDF_H
#define RAQ_PDF_H

#include <stddef.h>

typedef struct Pdf Pdf;

typedef enum { PDF_FONT_REGULAR = 0, PDF_FONT_BOLD = 1 } PdfFont;

/* Cria um documento de uma página. Medidas em pontos, origem no canto
   inferior esquerdo (convenção do PDF). */
Pdf *pdf_novo(double largura, double altura);
void pdf_libera(Pdf *p);

void pdf_linha(Pdf *p, double x1, double y1, double x2, double y2, double espessura);
void pdf_retangulo(Pdf *p, double x, double y, double w, double h, double espessura);

/* O texto entra em WinAnsi (já convertido) e é escapado internamente. */
void pdf_texto(Pdf *p, double x, double y, PdfFont fonte, double tam, const char *s);

/* Largura do texto em pontos, para centralizar e para quebrar linha. */
double pdf_largura_texto(PdfFont fonte, double tam, const char *s);

/* Grava o arquivo. Devolve 0 em caso de sucesso. */
int  pdf_salva(Pdf *p, const char *caminho);

#endif /* RAQ_PDF_H */
