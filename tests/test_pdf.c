/* test_pdf.c — gera um PDF de exemplo para conferir o layout contra o
   template original. Compila e roda no Linux, sem tocar na GUI. */
#include "backend_pdf.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

/* id, nome, aspecto, cor, massa min/max, usa_gas + min/max,
   usa_aehc + min/max, usa_bio + min/max, ativo */
static Produto PRODUTOS[4] = {
    { 1, "GASOLINA COMUM", "L.I.I.", "AMARELADA",  735.00, 775.00, 1, 31.0, 33.0, 0, 0, 0,      0, 0, 0,       1 },
    { 2, "GASOLINA ADITIVADA", "L.I.I.", "ESVERDEADA", 735.00, 775.00, 1, 31.0, 33.0, 0, 0, 0,      0, 0, 0,       1 },
    { 3, "DIESEL S10",   "L.I.I.", "AMARELADA",  817.00, 865.00, 0, 0, 0,       0, 0, 0,      1, 14.5, 15.5, 1 },
    { 4, "ETANOL HIDRATADO", "L.I.I.", "INCOLOR", 805.20, 811.20, 0, 0, 0,     1, 92.5, 94.6, 0, 0, 0,     1 }
};

static Config CFG = {
    "AUTO POSTO EXEMPLO LTDA",
    "12.345.678/0001-95",
    "AVENIDA DAS AÇÕES DE EXEMPLO, 1.250",
    "CENTRO",
    "CIDADE/UF",
    "",
    "",
    ""
};

static void coluna(Formulario *f, const Produto *p, const char *vol,
                   const char *massa, const char *gas, const char *aehc,
                   const char *bio)
{
    ColunaProduto *c = &f->colunas[f->n_colunas++];
    memset(c, 0, sizeof *c);
    c->produto = p;
    str_copy(c->volume,    sizeof c->volume,    vol);
    str_copy(c->aspecto,   sizeof c->aspecto,   p->aspecto_padrao);
    str_copy(c->cor,       sizeof c->cor,       p->cor_padrao);
    str_copy(c->massa,     sizeof c->massa,     massa);
    str_copy(c->teor_gas,  sizeof c->teor_gas,  gas);
    str_copy(c->teor_aehc, sizeof c->teor_aehc, aehc);
    str_copy(c->teor_bio,  sizeof c->teor_bio,  bio);
}

static void preenche_comum(Formulario *f)
{
    str_copy(f->data_coleta,        sizeof f->data_coleta,        "30/07/2026");
    str_copy(f->distribuidor,       sizeof f->distribuidor,       "DISTRIBUIDORA EXEMPLO DE PETRÓLEO S.A.");
    str_copy(f->distribuidor_cnpj,  sizeof f->distribuidor_cnpj,  "11.222.333/0001-81");
    str_copy(f->transportador,      sizeof f->transportador,      "TRANSPORTADORA MODELO SÃO CRISTÓVÃO LTDA");
    str_copy(f->transportador_cnpj, sizeof f->transportador_cnpj, "98.765.432/0001-98");
    str_copy(f->nota_fiscal,        sizeof f->nota_fiscal,        "123456");
    str_copy(f->placa,              sizeof f->placa,              "ABC1D23");
    str_copy(f->motorista,          sizeof f->motorista,          "JOSÉ ANTÔNIO DA SILVA");
    str_copy(f->motorista_rg,       sizeof f->motorista_rg,       "1.234.567 SSP/PE");
    str_copy(f->analista,           sizeof f->analista,           "MARIA APARECIDA GONÇALVES");
    str_copy(f->responsavel,        sizeof f->responsavel,        "MARIA APARECIDA GONÇALVES");
}

int main(void)
{
    Formulario f;
    int i;

    /* Caso 1: os quatro produtos. */
    memset(&f, 0, sizeof f);
    f.config = &CFG;
    preenche_comum(&f);
    coluna(&f, &PRODUTOS[0], "5.000", "751,20", "32,0", "",     "");
    coluna(&f, &PRODUTOS[1], "3.000", "748,80", "31,5", "",     "");
    coluna(&f, &PRODUTOS[2], "8.000", "840,20", "",     "",     "15,0");
    coluna(&f, &PRODUTOS[3], "6.000", "809,00", "",     "93,4", "");
    if (raq_gera_pdf(&f, "build/exemplo-4-produtos.pdf") != 0) {
        fprintf(stderr, "falha ao gerar exemplo de 4 produtos\n");
        return 1;
    }

    /* Caso 2: um único produto — a tabela deve ter uma coluna larga. */
    memset(&f, 0, sizeof f);
    f.config = &CFG;
    preenche_comum(&f);
    coluna(&f, &PRODUTOS[2], "12.000", "840,20", "", "", "15,0");
    if (raq_gera_pdf(&f, "build/exemplo-1-produto.pdf") != 0) {
        fprintf(stderr, "falha ao gerar exemplo de 1 produto\n");
        return 1;
    }

    /* Caso 3: dois produtos e textos exagerados, para testar o encolhimento. */
    memset(&f, 0, sizeof f);
    f.config = &CFG;
    preenche_comum(&f);
    str_copy(f.distribuidor, sizeof f.distribuidor,
             "DISTRIBUIDORA NACIONAL DE COMBUSTÍVEIS E LUBRIFICANTES DO NORDESTE S.A.");
    coluna(&f, &PRODUTOS[0], "5.000", "751,20", "32,0", "", "");
    coluna(&f, &PRODUTOS[3], "6.000", "809,00", "", "93,4", "");
    if (raq_gera_pdf(&f, "build/exemplo-2-produtos.pdf") != 0) {
        fprintf(stderr, "falha ao gerar exemplo de 2 produtos\n");
        return 1;
    }

    /* Sanidade das utilidades. */
    {
        struct { const char *cnpj; int esperado; } casos[] = {
            { "11.222.333/0001-81", 1 },
            { "98.765.432/0001-98", 1 },
            { "12.345.678/0001-95", 1 },
            { "11.111.111/1111-11", 0 },
            { "11.222.333/0001-82", 0 },
            { "123", 0 }
        };
        int falhas = 0;
        for (i = 0; i < (int)(sizeof casos / sizeof casos[0]); i++) {
            int r = cnpj_valido(casos[i].cnpj);
            if (r != casos[i].esperado) {
                fprintf(stderr, "cnpj_valido(\"%s\") = %d, esperado %d\n",
                        casos[i].cnpj, r, casos[i].esperado);
                falhas++;
            }
        }
        if (!data_valida("29/02/2024")) { fprintf(stderr, "29/02/2024 devia ser válida\n"); falhas++; }
        if (data_valida("29/02/2023"))  { fprintf(stderr, "29/02/2023 devia ser inválida\n"); falhas++; }
        if (data_valida("31/04/2024"))  { fprintf(stderr, "31/04/2024 devia ser inválida\n"); falhas++; }
        {
            /* O ponto é milhar no formato brasileiro, mas quem digita no
               teclado numérico escreve "0.7512" — e isso não pode virar 7512. */
            struct { const char *entrada; double esperado; } n[] = {
                { "751,20",   751.20 },   /* massa específica em kg/m³ */
                { "751.20",   751.20 },   /* mesma coisa com ponto decimal */
                { "1.234,50", 1234.50 },
                { "0.7512",   0.7512 },   /* ponto decimal, 4 casas */
                { "5.000",    5000.0 },   /* 3 casas: milhar, volume em litros */
                { "12.000",   12000.0 },
                { "27,0",     27.0 },
                { "27.0",     27.0 },     /* 1 casa: decimal */
                { "99,6",     99.6 }
            };
            int k;
            for (k = 0; k < (int)(sizeof n / sizeof n[0]); k++) {
                double v = -1;
                if (!num_parse(n[k].entrada, &v)
                    || v < n[k].esperado - 0.0001 || v > n[k].esperado + 0.0001) {
                    fprintf(stderr, "num_parse(\"%s\") = %f, esperado %f\n",
                            n[k].entrada, v, n[k].esperado);
                    falhas++;
                }
            }
        }
        {
            char wa[64];
            utf8_to_winansi("ÁLCOOL Ç ã °", wa, sizeof wa);
            if ((unsigned char)wa[0] != 0xC1) {
                fprintf(stderr, "utf8_to_winansi falhou: 0x%02X\n",
                        (unsigned char)wa[0]);
                falhas++;
            }
        }
        if (falhas) { fprintf(stderr, "%d teste(s) falharam\n", falhas); return 1; }
    }

    printf("ok: PDFs em build/, utilidades conferidas\n");
    return 0;
}
