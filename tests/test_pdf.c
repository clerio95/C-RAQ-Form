/* test_pdf.c — gera um PDF de exemplo para conferir o layout contra o
   template original. Compila e roda no Linux, sem tocar na GUI. */
#include "backend_pdf.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

static Produto PRODUTOS[4] = {
    { 1, "GASOLINA C",   "L.I.I.", "AMARELADA",  0.7350, 0.7750, 1, 26.0, 28.0, 0, 0, 0, 1 },
    { 2, "GASOLINA ADV", "L.I.I.", "ESVERDEADA", 0.7350, 0.7750, 1, 26.0, 28.0, 0, 0, 0, 1 },
    { 3, "DIESEL S10",   "L.I.I.", "AMARELADA",  0.8170, 0.8650, 0, 0, 0, 0, 0, 0, 1 },
    { 4, "ETANOL ADV",   "L.I.I.", "ESVERDEADA", 0.8076, 0.8112, 0, 0, 0, 1, 99.3, 100.0, 1 }
};

static Config CFG = {
    "AUTO POSTO BOA VIAGEM LTDA",
    "12.345.678/0001-95",
    "AVENIDA GOVERNADOR AGAMENON MAGALHÃES, 1.250",
    "DERBY",
    "RECIFE/PE",
    "",
    ""
};

static void coluna(Formulario *f, const Produto *p, const char *vol,
                   const char *massa, const char *gas, const char *aehc)
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
}

static void preenche_comum(Formulario *f)
{
    str_copy(f->data_coleta,        sizeof f->data_coleta,        "30/07/2026");
    str_copy(f->distribuidor,       sizeof f->distribuidor,       "IPIRANGA PRODUTOS DE PETRÓLEO S.A.");
    str_copy(f->distribuidor_cnpj,  sizeof f->distribuidor_cnpj,  "33.337.122/0001-27");
    str_copy(f->transportador,      sizeof f->transportador,      "TRANSPORTES SÃO CRISTÓVÃO LTDA");
    str_copy(f->transportador_cnpj, sizeof f->transportador_cnpj, "04.196.645/0001-00");
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
    coluna(&f, &PRODUTOS[0], "5.000", "0,7512", "27,0", "");
    coluna(&f, &PRODUTOS[1], "3.000", "0,7488", "26,5", "");
    coluna(&f, &PRODUTOS[2], "8.000", "0,8402", "",     "");
    coluna(&f, &PRODUTOS[3], "6.000", "0,8090", "",     "99,6");
    if (raq_gera_pdf(&f, "build/exemplo-4-produtos.pdf") != 0) {
        fprintf(stderr, "falha ao gerar exemplo de 4 produtos\n");
        return 1;
    }

    /* Caso 2: um único produto — a tabela deve ter uma coluna larga. */
    memset(&f, 0, sizeof f);
    f.config = &CFG;
    preenche_comum(&f);
    coluna(&f, &PRODUTOS[2], "12.000", "0,8402", "", "");
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
    coluna(&f, &PRODUTOS[0], "5.000", "0,7512", "27,0", "");
    coluna(&f, &PRODUTOS[3], "6.000", "0,8090", "", "99,6");
    if (raq_gera_pdf(&f, "build/exemplo-2-produtos.pdf") != 0) {
        fprintf(stderr, "falha ao gerar exemplo de 2 produtos\n");
        return 1;
    }

    /* Sanidade das utilidades. */
    {
        struct { const char *cnpj; int esperado; } casos[] = {
            { "33.337.122/0001-27", 1 },
            { "12.345.678/0001-95", 1 },
            { "11.111.111/1111-11", 0 },
            { "33.337.122/0001-28", 0 },
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
                { "0,7512",   0.7512 },
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
