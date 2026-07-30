#include "render.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

/* Geometria extraída do RAQ-TEMPLATE.pdf original: as réguas verticais ficam
   em x = 46,3 e 561,8 e a coluna de rótulos termina em 192,3. */
#define X_ESQ            46.3
#define X_DIR           561.8
#define X_ROTULO_FIM    192.3
#define Y_TOPO          742.1

#define ALT_CABECALHO    28.05   /* 4 linhas do bloco do posto */
#define ALT_BANDA        17.40   /* faixas "DADOS DE RECEBIMENTO" / "RESULTADOS" */
#define ALT_LINHA        17.33   /* linhas da tabela */

#define ESPESSURA         0.75
#define FONTE_TITULO     12.0
#define FONTE_CORPO      10.0
#define FONTE_MINIMA      5.5    /* piso do encolhimento automático */

#define REC_PAD           3.0    /* respiro interno das células */
#define BASE_OFFSET       5.0    /* base do texto acima da régua inferior */

/* Rótulos das 12 linhas de dados de recebimento, na ordem do formulário. */
typedef enum {
    L_PRODUTO = 0, L_VOLUME, L_DATA, L_DISTRIBUIDOR, L_CNPJ_DIST,
    L_TRANSPORTADOR, L_CNPJ_TRANSP, L_NF, L_PLACA, L_MOTORISTA, L_RG,
    L_ANALISTA, L_RECEBIMENTO_N
} LinhaRecebimento;

/* Todo texto literal deste arquivo está em UTF-8 e passa por
   utf8_to_winansi antes de ir para o backend. */
static const char *ROTULO_RECEBIMENTO[L_RECEBIMENTO_N] = {
    "Produto", "Volume recebido (litros)", "Data da coleta", "Distribuidor",
    "CNPJ do Distribuidor", "Transportador", "CNPJ do Transportador",
    "Nota Fiscal do Produto", "Placa do Caminhão/Reboque", "Nome Motorista",
    "RG Motorista", "Nome do Analista"
};

static const char *ROTULO_ANALISE[ANALISE_N] = {
    "Aspecto", "Cor", "Massa Específica a 20°C",
    "Teor de álcool na Gasolina", "Teor Alcoólico no AEHC",
    "Teor de Biodiesel"
};

/* Unidade impressa junto do valor. Aspecto e Cor não têm; a massa específica
   sai em kg/m³ e os três teores em porcentagem. */
static const char *UNIDADE_ANALISE[ANALISE_N] = {
    "", "", " kg/m³", "%", "%", "%"
};

/* Linhas mescladas: uma célula única atravessando todas as colunas. */
static int linha_mesclada(LinhaRecebimento l)
{
    return l != L_PRODUTO && l != L_VOLUME;
}

/* ---------- helpers de desenho ---------- */

static void hline(const DrawBackend *b, double x1, double x2, double y)
{
    b->linha(b->ctx, x1, y, x2, y, ESPESSURA);
}

static void vline(const DrawBackend *b, double x, double y1, double y2)
{
    b->linha(b->ctx, x, y1, x, y2, ESPESSURA);
}

/* Escreve o texto dentro da largura disponível: primeiro tenta encolher a
   fonte até FONTE_MINIMA; se ainda não couber, corta com reticências. Vale a
   pena porque razões sociais longas são a regra, não a exceção. */
static void texto_ajustado(const DrawBackend *b, double x, double y,
                           int negrito, double tam, const char *s,
                           double largura_max, int centralizado)
{
    char corte[256];
    double w;

    if (!s || !*s) return;

    w = b->largura(b->ctx, negrito, tam, s);
    while (w > largura_max && tam > FONTE_MINIMA) {
        tam -= 0.25;
        w = b->largura(b->ctx, negrito, tam, s);
    }

    if (w > largura_max) {
        size_t n;
        str_copy(corte, sizeof corte, s);
        for (n = strlen(corte); n > 0; n--) {
            corte[n - 1] = '\0';
            str_copy(corte + (n - 1), sizeof corte - (n - 1), "...");
            w = b->largura(b->ctx, negrito, tam, corte);
            if (w <= largura_max) break;
            corte[n - 1] = '\0';
        }
        if (n == 0) return;
        s = corte;
    }

    if (centralizado) x += (largura_max - w) / 2.0;
    b->texto(b->ctx, x, y, negrito, tam, s);
}

/* Converte de UTF-8 e escreve. */
static void texto_utf8(const DrawBackend *b, double x, double y, int negrito,
                       double tam, const char *utf8, double largura_max,
                       int centralizado)
{
    char wa[512];
    utf8_to_winansi(utf8, wa, sizeof wa);
    texto_ajustado(b, x, y, negrito, tam, wa, largura_max, centralizado);
}

/* Rótulo do bloco do posto seguido do valor, na mesma linha. */
static void linha_posto(const DrawBackend *b, double x, double y,
                        double largura_total, const char *rotulo_utf8,
                        const char *valor)
{
    char rot[128];
    double wr;

    utf8_to_winansi(rotulo_utf8, rot, sizeof rot);
    wr = b->largura(b->ctx, 0, FONTE_CORPO, rot);
    b->texto(b->ctx, x, y, 0, FONTE_CORPO, rot);
    texto_utf8(b, x + wr + 4.0, y, 0, FONTE_CORPO, valor,
               largura_total - wr - 4.0, 0);
}

/* Valor de uma linha mesclada. */
static const char *valor_mesclado(const Formulario *f, LinhaRecebimento l)
{
    switch (l) {
    case L_DATA:           return f->data_coleta;
    case L_DISTRIBUIDOR:   return f->distribuidor;
    case L_CNPJ_DIST:      return f->distribuidor_cnpj;
    case L_TRANSPORTADOR:  return f->transportador;
    case L_CNPJ_TRANSP:    return f->transportador_cnpj;
    case L_NF:             return f->nota_fiscal;
    case L_PLACA:          return f->placa;
    case L_MOTORISTA:      return f->motorista;
    case L_RG:             return f->motorista_rg;
    case L_ANALISTA:       return f->analista;
    default:               return "";
    }
}

const char *coluna_valor_analise(const ColunaProduto *c, int linha)
{
    if (!c || !c->produto) return "-";
    switch (linha) {
    case ANALISE_ASPECTO:   return c->aspecto;
    case ANALISE_COR:       return c->cor;
    case ANALISE_MASSA:     return c->massa;
    case ANALISE_TEOR_GAS:  return c->produto->usa_teor_gas  ? c->teor_gas  : "-";
    case ANALISE_TEOR_AEHC: return c->produto->usa_teor_aehc ? c->teor_aehc : "-";
    case ANALISE_TEOR_BIO:  return c->produto->usa_teor_bio  ? c->teor_bio  : "-";
    default:                return "-";
    }
}

/* ---------- desenho principal ---------- */

void raq_desenha(const DrawBackend *b, const Formulario *f)
{
    double col_x[MAX_PRODUTOS + 1];
    double y, largura_colunas, passo;
    double y_topo_recebimento, y_topo_analise;
    int i, n;

    if (!b || !f) return;

    /* Colunas de produto distribuídas igualmente no espaço restante. Com
       nenhum produto marcado a tabela ainda é desenhada, só que vazia. */
    n = f->n_colunas > 0 ? f->n_colunas : 1;
    if (n > MAX_PRODUTOS) n = MAX_PRODUTOS;
    largura_colunas = X_DIR - X_ROTULO_FIM;
    passo = largura_colunas / n;
    for (i = 0; i <= n; i++) col_x[i] = X_ROTULO_FIM + passo * i;
    col_x[n] = X_DIR;                       /* evita erro de arredondamento */

    /* --- título --- */
    {
        char t[128];
        double w;
        utf8_to_winansi("FORMULÁRIO PARA REGISTRO DA ANÁLISE DA QUALIDADE",
                        t, sizeof t);
        w = b->largura(b->ctx, 0, FONTE_TITULO, t);
        b->texto(b->ctx, (RAQ_PAGINA_LARGURA - w) / 2.0, 780.0, 0,
                 FONTE_TITULO, t);
    }

    /* --- bloco do posto: 4 linhas altas, sem divisões de coluna --- */
    y = Y_TOPO;
    hline(b, X_ESQ, X_DIR, y);

    linha_posto(b, X_ESQ + REC_PAD, y - 12.0, X_DIR - X_ESQ - 2 * REC_PAD,
                "RAZÃO SOCIAL DO POSTO REVENDEDOR:", f->config->razao_social);
    y -= ALT_CABECALHO;
    hline(b, X_ESQ, X_DIR, y);

    linha_posto(b, X_ESQ + REC_PAD, y - 12.0, X_DIR - X_ESQ - 2 * REC_PAD,
                "CNPJ DO POSTO REVENDEDOR:", f->config->cnpj);
    y -= ALT_CABECALHO;
    hline(b, X_ESQ, X_DIR, y);

    linha_posto(b, X_ESQ + REC_PAD, y - 12.0, X_DIR - X_ESQ - 2 * REC_PAD,
                "ENDEREÇO DO POSTO REVENDEDOR:", f->config->endereco);
    y -= ALT_CABECALHO;
    hline(b, X_ESQ, X_DIR, y);

    /* BAIRRO | CIDADE/ESTADO dividem a linha ao meio */
    {
        double meio = 299.5;   /* mesma divisória do template original */
        linha_posto(b, X_ESQ + REC_PAD, y - 12.0, meio - X_ESQ - 2 * REC_PAD,
                    "BAIRRO:", f->config->bairro);
        linha_posto(b, meio + REC_PAD, y - 12.0, X_DIR - meio - 2 * REC_PAD,
                    "CIDADE/ESTADO:", f->config->cidade_estado);
        vline(b, meio, y - ALT_CABECALHO, y);
    }
    y -= ALT_CABECALHO;
    hline(b, X_ESQ, X_DIR, y);

    /* --- faixa DADOS DE RECEBIMENTO --- */
    texto_utf8(b, X_ESQ, y - 12.5, 0, FONTE_CORPO, "DADOS DE RECEBIMENTO",
               X_DIR - X_ESQ, 1);
    y -= ALT_BANDA;
    hline(b, X_ESQ, X_DIR, y);
    y_topo_recebimento = y;

    /* --- 12 linhas de recebimento --- */
    for (i = 0; i < L_RECEBIMENTO_N; i++) {
        double base = y - ALT_LINHA + BASE_OFFSET;

        texto_utf8(b, X_ESQ + REC_PAD, base, 0, FONTE_CORPO,
                   ROTULO_RECEBIMENTO[i],
                   X_ROTULO_FIM - X_ESQ - 2 * REC_PAD, 0);

        if (linha_mesclada((LinhaRecebimento)i)) {
            texto_utf8(b, X_ROTULO_FIM + REC_PAD, base, 0, FONTE_CORPO,
                       valor_mesclado(f, (LinhaRecebimento)i),
                       X_DIR - X_ROTULO_FIM - 2 * REC_PAD, 1);
        } else {
            int c;
            for (c = 0; c < f->n_colunas && c < n; c++) {
                const char *v = (i == L_PRODUTO)
                              ? (f->colunas[c].produto ? f->colunas[c].produto->nome : "")
                              : f->colunas[c].volume;
                texto_utf8(b, col_x[c] + REC_PAD, base, 0, FONTE_CORPO, v,
                           col_x[c + 1] - col_x[c] - 2 * REC_PAD, 1);
            }
        }

        y -= ALT_LINHA;
        hline(b, X_ESQ, X_DIR, y);
    }

    /* --- faixa RESULTADOS DA ANÁLISE --- */
    texto_utf8(b, X_ESQ, y - 12.5, 0, FONTE_CORPO, "RESULTADOS DA ANÁLISE",
               X_DIR - X_ESQ, 1);
    y -= ALT_BANDA;
    hline(b, X_ESQ, X_DIR, y);
    y_topo_analise = y;

    /* --- 5 linhas de análise --- */
    for (i = 0; i < ANALISE_N; i++) {
        double base = y - ALT_LINHA + BASE_OFFSET;
        int c;

        texto_utf8(b, X_ESQ + REC_PAD, base, 0, FONTE_CORPO,
                   ROTULO_ANALISE[i],
                   X_ROTULO_FIM - X_ESQ - 2 * REC_PAD, 0);

        for (c = 0; c < f->n_colunas && c < n; c++) {
            const char *v = coluna_valor_analise(&f->colunas[c], i);
            char com_unidade[128];

            /* A unidade acompanha o número, mas não o "-" das linhas que não
               se aplicam ao produto. */
            if (*UNIDADE_ANALISE[i] && !str_empty(v) && strcmp(v, "-")) {
                snprintf(com_unidade, sizeof com_unidade, "%s%s",
                         v, UNIDADE_ANALISE[i]);
                v = com_unidade;
            }
            texto_utf8(b, col_x[c] + REC_PAD, base, 0, FONTE_CORPO, v,
                       col_x[c + 1] - col_x[c] - 2 * REC_PAD, 1);
        }

        y -= ALT_LINHA;
        hline(b, X_ESQ, X_DIR, y);
    }

    /* --- réguas verticais --- */
    vline(b, X_ESQ, y, Y_TOPO);
    vline(b, X_DIR, y, Y_TOPO);

    /* A coluna de rótulos atravessa os dois blocos tabulares inteiros, mas
       nunca o cabeçalho do posto nem as faixas de título. */
    vline(b, X_ROTULO_FIM, y_topo_recebimento - L_RECEBIMENTO_N * ALT_LINHA,
          y_topo_recebimento);
    vline(b, X_ROTULO_FIM, y_topo_analise - ANALISE_N * ALT_LINHA,
          y_topo_analise);

    /* As divisórias entre produtos existem em todas as linhas de análise, mas
       só nas linhas não mescladas do bloco de recebimento — é isso que produz
       a célula única de Distribuidor, Nota Fiscal, Motorista e companhia. */
    for (i = 1; i < n; i++)
        vline(b, col_x[i], y_topo_analise - ANALISE_N * ALT_LINHA, y_topo_analise);

    for (i = 0; i < L_RECEBIMENTO_N; i++) {
        double topo = y_topo_recebimento - i * ALT_LINHA;
        int c;
        if (linha_mesclada((LinhaRecebimento)i)) continue;
        for (c = 1; c < n; c++) vline(b, col_x[c], topo - ALT_LINHA, topo);
    }

    /* --- rodapé --- */
    {
        double wr;
        char t[128];
        utf8_to_winansi("Responsável pelo Preenchimento:", t, sizeof t);
        wr = b->largura(b->ctx, 0, FONTE_CORPO, t);
        b->texto(b->ctx, 63.0, 226.0, 0, FONTE_CORPO, t);
        texto_utf8(b, 63.0 + wr + 4.0, 226.0, 0, FONTE_CORPO, f->responsavel,
                   X_DIR - 63.0 - wr - 4.0, 0);

        b->texto(b->ctx, 63.0, 197.0, 0, FONTE_CORPO, "Assinatura:");
        wr = b->largura(b->ctx, 0, FONTE_CORPO, "Assinatura:");
        hline(b, 63.0 + wr + 8.0, 400.0, 194.0);
    }
}
