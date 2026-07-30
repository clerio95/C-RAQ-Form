/* backend_gdi.c — implementa DrawBackend sobre um DC de impressora.

   Ponto importante: a largura do texto continua sendo medida pela tabela
   Helvetica do pdf.c, não pelo GDI. Assim o encolhimento automático e a
   centralização decidem exatamente a mesma coisa no papel e no PDF — o que
   sai da impressora é o mesmo desenho do arquivo salvo. Arial é
   metricamente compatível com Helvetica, então o traço bate. */
#include "backend_gdi.h"
#include "pdf.h"
#include "render.h"
#include "util.h"
#include "win_util.h"

#include <stdlib.h>
#include <string.h>
#include <winspool.h>

typedef struct {
    HDC   dc;
    int   dpi_x, dpi_y;
    int   off_x, off_y;      /* margem física não imprimível */
    HFONT fonte_atual;
    double tam_atual;
    int    negrito_atual;
} GdiCtx;

static int px_x(const GdiCtx *g, double pt)
{
    return (int)(pt / 72.0 * g->dpi_x + 0.5) - g->off_x;
}

/* O PDF tem origem embaixo à esquerda; o GDI, em cima à esquerda. */
static int px_y(const GdiCtx *g, double pt)
{
    return (int)((RAQ_PAGINA_ALTURA - pt) / 72.0 * g->dpi_y + 0.5) - g->off_y;
}

static void bg_linha(void *ctx, double x1, double y1, double x2, double y2,
                     double espessura)
{
    GdiCtx *g = (GdiCtx *)ctx;
    int largura = (int)(espessura / 72.0 * g->dpi_x + 0.5);
    HPEN caneta, antiga;

    if (largura < 1) largura = 1;
    caneta = CreatePen(PS_SOLID, largura, RGB(0, 0, 0));
    antiga = (HPEN)SelectObject(g->dc, caneta);

    MoveToEx(g->dc, px_x(g, x1), px_y(g, y1), NULL);
    LineTo(g->dc, px_x(g, x2), px_y(g, y2));

    SelectObject(g->dc, antiga);
    DeleteObject(caneta);
}

static void bg_texto(void *ctx, double x, double y, int negrito, double tam,
                     const char *winansi)
{
    GdiCtx *g = (GdiCtx *)ctx;
    wchar_t w[512];
    int i, n;

    if (!winansi || !*winansi) return;

    /* O renderer entrega WinAnsi (CP1252); o GDI recebe UTF-16. */
    n = MultiByteToWideChar(1252, 0, winansi, -1, w,
                            (int)(sizeof w / sizeof w[0]));
    if (n <= 0) return;

    if (!g->fonte_atual || tam != g->tam_atual || negrito != g->negrito_atual) {
        HFONT nova = CreateFontW(-(int)(tam / 72.0 * g->dpi_y + 0.5), 0, 0, 0,
                                 negrito ? FW_BOLD : FW_NORMAL, FALSE, FALSE,
                                 FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                 DEFAULT_PITCH | FF_SWISS, L"Arial");
        SelectObject(g->dc, nova);
        if (g->fonte_atual) DeleteObject(g->fonte_atual);
        g->fonte_atual = nova;
        g->tam_atual = tam;
        g->negrito_atual = negrito;
    }

    /* O GDI não conhece a largura Helvetica que o layout usou, então o texto
       é posicionado glifo a glifo com os avanços da tabela do PDF. Isso
       mantém o alinhamento idêntico ao do arquivo. */
    {
        double avanco = 0.0;
        char um[2] = { 0, 0 };
        for (i = 0; i < n - 1; i++) {
            wchar_t c = w[i];
            um[0] = winansi[i];
            TextOutW(g->dc, px_x(g, x + avanco), px_y(g, y), &c, 1);
            avanco += pdf_largura_texto(negrito ? PDF_FONT_BOLD
                                                : PDF_FONT_REGULAR, tam, um);
        }
    }
}

static double bg_largura(void *ctx, int negrito, double tam, const char *s)
{
    (void)ctx;
    return pdf_largura_texto(negrito ? PDF_FONT_BOLD : PDF_FONT_REGULAR, tam, s);
}

int raq_lista_impressoras(char nomes[][256], int max)
{
    DWORD precisa = 0, devolvidas = 0;
    BYTE *buf;
    PRINTER_INFO_4W *info;
    int n = 0, i;

    EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4,
                  NULL, 0, &precisa, &devolvidas);
    if (precisa == 0) return 0;

    buf = (BYTE *)malloc(precisa);
    if (!buf) return 0;

    if (EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 4,
                      buf, precisa, &precisa, &devolvidas)) {
        info = (PRINTER_INFO_4W *)buf;
        for (i = 0; i < (int)devolvidas && n < max; i++)
            if (info[i].pPrinterName)
                w_para_u8(info[i].pPrinterName, nomes[n++], 256);
    }
    free(buf);
    return n;
}

/* Nome da impressora padrão do Windows. */
static int impressora_padrao(char *dst, int dst_bytes)
{
    wchar_t nome[512];
    DWORD n = (DWORD)(sizeof nome / sizeof nome[0]);

    if (!GetDefaultPrinterW(nome, &n)) return 0;
    w_para_u8(nome, dst, dst_bytes);
    return dst[0] != '\0';
}

int raq_impressora_em_uso(const char *impressora, char *dst, int dst_bytes)
{
    if (impressora && *impressora) {
        str_copy(dst, (size_t)dst_bytes, impressora);
        return 1;
    }
    return impressora_padrao(dst, dst_bytes);
}

int raq_imprime(const Formulario *f, HWND pai, const char *impressora)
{
    DOCINFOW di;
    GdiCtx g;
    DrawBackend b;
    HDC dc;
    char nome[256];
    wchar_t wnome[512];
    int ok = 0;

    (void)pai;

    if (!raq_impressora_em_uso(impressora, nome, sizeof nome)) return -1;
    u8_para_w(nome, wnome, (int)(sizeof wnome / sizeof wnome[0]));

    /* Abre direto na impressora escolhida, sem caixa de diálogo: o operador
       já respondeu "sim" à pergunta de imprimir. */
    dc = CreateDCW(L"WINSPOOL", wnome, NULL, NULL);
    if (!dc) return -1;

    memset(&g, 0, sizeof g);
    g.dc    = dc;
    g.dpi_x = GetDeviceCaps(g.dc, LOGPIXELSX);
    g.dpi_y = GetDeviceCaps(g.dc, LOGPIXELSY);
    g.off_x = GetDeviceCaps(g.dc, PHYSICALOFFSETX);
    g.off_y = GetDeviceCaps(g.dc, PHYSICALOFFSETY);
    g.tam_atual = -1.0;

    memset(&di, 0, sizeof di);
    di.cbSize    = sizeof di;
    di.lpszDocName = L"Formulário RAQ";

    SetBkMode(g.dc, TRANSPARENT);
    SetTextAlign(g.dc, TA_LEFT | TA_BASELINE);
    SetTextColor(g.dc, RGB(0, 0, 0));

    if (StartDocW(g.dc, &di) > 0) {
        if (StartPage(g.dc) > 0) {
            b.ctx     = &g;
            b.linha   = bg_linha;
            b.texto   = bg_texto;
            b.largura = bg_largura;
            raq_desenha(&b, f);
            ok = (EndPage(g.dc) > 0);
        }
        EndDoc(g.dc);
    }

    if (g.fonte_atual) DeleteObject(g.fonte_atual);
    DeleteDC(dc);
    return ok ? 0 : -1;
}
