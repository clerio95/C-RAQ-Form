#include "store.h"
#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <direct.h>
#  define CRIA_PASTA(p) _mkdir(p)
#else
#  include <sys/stat.h>
#  define CRIA_PASTA(p) mkdir(p, 0775)
#endif

/* ---------- caminhos ---------- */

static void caminho(const Store *s, const char *arquivo, char *dst, size_t n)
{
    snprintf(dst, n, "%s%s%s", s->dir,
             (s->dir[0] && s->dir[strlen(s->dir) - 1] != '/'
                        && s->dir[strlen(s->dir) - 1] != '\\') ? "/" : "",
             arquivo);
}

static FILE *abre(const Store *s, const char *arquivo, const char *modo)
{
    char p[640];
    caminho(s, arquivo, p, sizeof p);
    return fopen(p, modo);
}

/* ---------- CSV ---------- */

/* Divide a linha em campos separados por ';'. Devolve a quantidade de campos.
   Modifica `linha` no lugar. */
static int csv_divide(char *linha, char **campos, int max)
{
    int n = 0;
    char *p = linha;

    /* remove quebra de linha e um BOM eventual */
    linha[strcspn(linha, "\r\n")] = '\0';
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB
        && (unsigned char)p[2] == 0xBF) p += 3;

    campos[n++] = p;
    while (*p && n < max) {
        if (*p == ';') { *p = '\0'; campos[n++] = p + 1; }
        p++;
    }
    while (n < max) campos[n++] = (char *)"";
    return n;
}

/* Campos não podem conter o separador; trocamos por vírgula na gravação. */
static void csv_escreve_campo(FILE *f, const char *s)
{
    for (; s && *s; s++)
        fputc((*s == ';' || *s == '\n' || *s == '\r') ? ',' : *s, f);
}

static void csv_linha(FILE *f, int n, ...)
{
    va_list ap;
    int i;
    va_start(ap, n);
    for (i = 0; i < n; i++) {
        if (i) fputc(';', f);
        csv_escreve_campo(f, va_arg(ap, const char *));
    }
    va_end(ap);
    fputc('\n', f);
}

static void num_para_texto(double v, int casas, char *dst, size_t n)
{
    snprintf(dst, n, "%.*f", casas, v);
}

/* ---------- padrões da primeira execução ---------- */

static void semeia_produtos(Store *s)
{
    static const struct {
        const char *nome, *cor;
        double mmin, mmax;
        int gas; double gmin, gmax;
        int aehc; double amin, amax;
    } base[] = {
        { "GASOLINA C",   "AMARELADA",  0.7350, 0.7750, 1, 26.0, 28.0, 0, 0, 0 },
        { "GASOLINA ADV", "ESVERDEADA", 0.7350, 0.7750, 1, 26.0, 28.0, 0, 0, 0 },
        { "DIESEL S10",   "AMARELADA",  0.8170, 0.8650, 0, 0, 0, 0, 0, 0 },
        { "ETANOL ADV",   "ESVERDEADA", 0.8076, 0.8112, 0, 0, 0, 1, 99.3, 100.0 }
    };
    int i;

    s->n_produtos = 0;
    for (i = 0; i < (int)(sizeof base / sizeof base[0]); i++) {
        Produto *p = &s->produtos[s->n_produtos];
        memset(p, 0, sizeof *p);
        p->id = i + 1;
        str_copy(p->nome, sizeof p->nome, base[i].nome);
        str_copy(p->aspecto_padrao, sizeof p->aspecto_padrao, "L.I.I.");
        str_copy(p->cor_padrao, sizeof p->cor_padrao, base[i].cor);
        p->massa_min = base[i].mmin; p->massa_max = base[i].mmax;
        p->usa_teor_gas = base[i].gas;
        p->gas_min = base[i].gmin;   p->gas_max = base[i].gmax;
        p->usa_teor_aehc = base[i].aehc;
        p->aehc_min = base[i].amin;  p->aehc_max = base[i].amax;
        p->ativo = 1;
        s->n_produtos++;
    }
}

static void semeia_config(Store *s)
{
    memset(&s->config, 0, sizeof s->config);
    str_copy(s->config.razao_social,  sizeof s->config.razao_social,  "");
    str_copy(s->config.pasta_saida,   sizeof s->config.pasta_saida,   "Formularios");
    senha_hash("admin", s->config.senha_hash, sizeof s->config.senha_hash);
}

/* ---------- leitura ---------- */

static void le_config(Store *s)
{
    FILE *f = abre(s, "config.ini", "rb");
    char linha[1024];

    semeia_config(s);
    if (!f) return;

    while (fgets(linha, sizeof linha, f)) {
        char *eq;
        char *chave = linha;
        if ((unsigned char)linha[0] == 0xEF) chave += 3;
        linha[strcspn(linha, "\r\n")] = '\0';
        if (*chave == '#' || *chave == ';' || !*chave) continue;
        eq = strchr(chave, '=');
        if (!eq) continue;
        *eq = '\0';
        str_trim(chave);

        if      (!strcmp(chave, "razao_social"))  str_copy(s->config.razao_social,  sizeof s->config.razao_social,  eq + 1);
        else if (!strcmp(chave, "cnpj"))          str_copy(s->config.cnpj,          sizeof s->config.cnpj,          eq + 1);
        else if (!strcmp(chave, "endereco"))      str_copy(s->config.endereco,      sizeof s->config.endereco,      eq + 1);
        else if (!strcmp(chave, "bairro"))        str_copy(s->config.bairro,        sizeof s->config.bairro,        eq + 1);
        else if (!strcmp(chave, "cidade_estado")) str_copy(s->config.cidade_estado, sizeof s->config.cidade_estado, eq + 1);
        else if (!strcmp(chave, "pasta_saida"))   str_copy(s->config.pasta_saida,   sizeof s->config.pasta_saida,   eq + 1);
        else if (!strcmp(chave, "senha_hash"))    str_copy(s->config.senha_hash,    sizeof s->config.senha_hash,    eq + 1);
    }
    fclose(f);

    if (str_empty(s->config.pasta_saida))
        str_copy(s->config.pasta_saida, sizeof s->config.pasta_saida, "Formularios");
    if (str_empty(s->config.senha_hash))
        senha_hash("admin", s->config.senha_hash, sizeof s->config.senha_hash);
}

static void le_produtos(Store *s)
{
    FILE *f = abre(s, "produtos.csv", "rb");
    char linha[1024];
    int primeira = 1;

    if (!f) { semeia_produtos(s); store_salva_produtos(s); return; }

    s->n_produtos = 0;
    while (fgets(linha, sizeof linha, f) && s->n_produtos < MAX_PRODUTOS) {
        char *c[14];
        Produto *p;
        if (primeira) { primeira = 0; if (strstr(linha, "nome")) continue; }
        if (str_empty(linha)) continue;
        csv_divide(linha, c, 14);

        p = &s->produtos[s->n_produtos];
        memset(p, 0, sizeof *p);
        p->id = atoi(c[0]);
        str_copy(p->nome, sizeof p->nome, c[1]);
        str_copy(p->aspecto_padrao, sizeof p->aspecto_padrao, c[2]);
        str_copy(p->cor_padrao, sizeof p->cor_padrao, c[3]);
        num_parse(c[4], &p->massa_min);
        num_parse(c[5], &p->massa_max);
        p->usa_teor_gas = atoi(c[6]);
        num_parse(c[7], &p->gas_min);
        num_parse(c[8], &p->gas_max);
        p->usa_teor_aehc = atoi(c[9]);
        num_parse(c[10], &p->aehc_min);
        num_parse(c[11], &p->aehc_max);
        p->ativo = c[12][0] ? atoi(c[12]) : 1;
        if (str_empty(p->nome)) continue;
        s->n_produtos++;
    }
    fclose(f);

    if (s->n_produtos == 0) { semeia_produtos(s); store_salva_produtos(s); }
}

static void le_empresas(Store *s, const char *arquivo, Empresa *lista, int *n)
{
    FILE *f = abre(s, arquivo, "rb");
    char linha[1024];
    int primeira = 1;

    *n = 0;
    if (!f) return;

    while (fgets(linha, sizeof linha, f) && *n < MAX_CADASTROS) {
        char *c[4];
        Empresa *e;
        if (primeira) { primeira = 0; if (strstr(linha, "nome")) continue; }
        if (str_empty(linha)) continue;
        csv_divide(linha, c, 4);

        e = &lista[*n];
        memset(e, 0, sizeof *e);
        e->id = atoi(c[0]);
        str_copy(e->nome, sizeof e->nome, c[1]);
        str_copy(e->cnpj, sizeof e->cnpj, c[2]);
        e->padrao = atoi(c[3]);
        if (str_empty(e->nome)) continue;
        (*n)++;
    }
    fclose(f);
}

static void le_placas(Store *s)
{
    FILE *f = abre(s, "placas.csv", "rb");
    char linha[1024];
    int primeira = 1;

    s->n_placas = 0;
    if (!f) return;

    while (fgets(linha, sizeof linha, f) && s->n_placas < MAX_CADASTROS) {
        char *c[2];
        if (primeira) { primeira = 0; if (strstr(linha, "placa")) continue; }
        if (str_empty(linha)) continue;
        csv_divide(linha, c, 2);
        s->placas[s->n_placas].id = atoi(c[0]);
        str_copy(s->placas[s->n_placas].placa,
                 sizeof s->placas[s->n_placas].placa, c[1]);
        if (str_empty(s->placas[s->n_placas].placa)) continue;
        s->n_placas++;
    }
    fclose(f);
}

static void le_motoristas(Store *s)
{
    FILE *f = abre(s, "motoristas.csv", "rb");
    char linha[1024];
    int primeira = 1;

    s->n_motoristas = 0;
    if (!f) return;

    while (fgets(linha, sizeof linha, f) && s->n_motoristas < MAX_CADASTROS) {
        char *c[3];
        Motorista *m;
        if (primeira) { primeira = 0; if (strstr(linha, "nome")) continue; }
        if (str_empty(linha)) continue;
        csv_divide(linha, c, 3);
        m = &s->motoristas[s->n_motoristas];
        m->id = atoi(c[0]);
        str_copy(m->nome, sizeof m->nome, c[1]);
        str_copy(m->rg,   sizeof m->rg,   c[2]);
        if (str_empty(m->nome)) continue;
        s->n_motoristas++;
    }
    fclose(f);
}

static void le_analistas(Store *s)
{
    FILE *f = abre(s, "analistas.csv", "rb");
    char linha[1024];
    int primeira = 1;

    s->n_analistas = 0;
    if (!f) return;

    while (fgets(linha, sizeof linha, f) && s->n_analistas < MAX_CADASTROS) {
        char *c[2];
        if (primeira) { primeira = 0; if (strstr(linha, "nome")) continue; }
        if (str_empty(linha)) continue;
        csv_divide(linha, c, 2);
        s->analistas[s->n_analistas].id = atoi(c[0]);
        str_copy(s->analistas[s->n_analistas].nome,
                 sizeof s->analistas[s->n_analistas].nome, c[1]);
        if (str_empty(s->analistas[s->n_analistas].nome)) continue;
        s->n_analistas++;
    }
    fclose(f);
}

int store_carrega(Store *s, const char *dir)
{
    if (!s) return -1;
    memset(s, 0, sizeof *s);
    str_copy(s->dir, sizeof s->dir, dir && *dir ? dir : "dados");
    CRIA_PASTA(s->dir);

    le_config(s);
    le_produtos(s);
    le_empresas(s, "distribuidores.csv",  s->distribuidores,  &s->n_distribuidores);
    le_empresas(s, "transportadores.csv", s->transportadores, &s->n_transportadores);
    le_placas(s);
    le_motoristas(s);
    le_analistas(s);
    return 0;
}

/* ---------- gravação ---------- */

int store_salva_config(const Store *s)
{
    FILE *f = abre(s, "config.ini", "wb");
    if (!f) return -1;
    fprintf(f, "# Dados fixos do posto, usados no cabecalho de todo formulario.\n");
    fprintf(f, "razao_social=%s\n",  s->config.razao_social);
    fprintf(f, "cnpj=%s\n",          s->config.cnpj);
    fprintf(f, "endereco=%s\n",      s->config.endereco);
    fprintf(f, "bairro=%s\n",        s->config.bairro);
    fprintf(f, "cidade_estado=%s\n", s->config.cidade_estado);
    fprintf(f, "pasta_saida=%s\n",   s->config.pasta_saida);
    fprintf(f, "senha_hash=%s\n",    s->config.senha_hash);
    return fclose(f) == 0 ? 0 : -1;
}

int store_salva_produtos(const Store *s)
{
    FILE *f = abre(s, "produtos.csv", "wb");
    int i;
    if (!f) return -1;
    fputs("id;nome;aspecto_padrao;cor_padrao;massa_min;massa_max;"
          "usa_teor_gas;gas_min;gas_max;usa_teor_aehc;aehc_min;aehc_max;ativo\n", f);
    for (i = 0; i < s->n_produtos; i++) {
        const Produto *p = &s->produtos[i];
        char id[16], mmin[32], mmax[32], gmin[32], gmax[32], amin[32], amax[32];
        char ug[4], ua[4], at[4];
        snprintf(id, sizeof id, "%d", p->id);
        num_para_texto(p->massa_min, 4, mmin, sizeof mmin);
        num_para_texto(p->massa_max, 4, mmax, sizeof mmax);
        num_para_texto(p->gas_min,   1, gmin, sizeof gmin);
        num_para_texto(p->gas_max,   1, gmax, sizeof gmax);
        num_para_texto(p->aehc_min,  1, amin, sizeof amin);
        num_para_texto(p->aehc_max,  1, amax, sizeof amax);
        snprintf(ug, sizeof ug, "%d", p->usa_teor_gas);
        snprintf(ua, sizeof ua, "%d", p->usa_teor_aehc);
        snprintf(at, sizeof at, "%d", p->ativo);
        csv_linha(f, 13, id, p->nome, p->aspecto_padrao, p->cor_padrao,
                  mmin, mmax, ug, gmin, gmax, ua, amin, amax, at);
    }
    return fclose(f) == 0 ? 0 : -1;
}

static int salva_empresas(const Store *s, const char *arquivo,
                          const Empresa *lista, int n)
{
    FILE *f = abre(s, arquivo, "wb");
    int i;
    if (!f) return -1;
    fputs("id;nome;cnpj;padrao\n", f);
    for (i = 0; i < n; i++) {
        char id[16], pd[4];
        snprintf(id, sizeof id, "%d", lista[i].id);
        snprintf(pd, sizeof pd, "%d", lista[i].padrao);
        csv_linha(f, 4, id, lista[i].nome, lista[i].cnpj, pd);
    }
    return fclose(f) == 0 ? 0 : -1;
}

int store_salva_distribuidores(const Store *s)
{
    return salva_empresas(s, "distribuidores.csv",
                          s->distribuidores, s->n_distribuidores);
}

int store_salva_transportadores(const Store *s)
{
    return salva_empresas(s, "transportadores.csv",
                          s->transportadores, s->n_transportadores);
}

int store_salva_placas(const Store *s)
{
    FILE *f = abre(s, "placas.csv", "wb");
    int i;
    if (!f) return -1;
    fputs("id;placa\n", f);
    for (i = 0; i < s->n_placas; i++) {
        char id[16];
        snprintf(id, sizeof id, "%d", s->placas[i].id);
        csv_linha(f, 2, id, s->placas[i].placa);
    }
    return fclose(f) == 0 ? 0 : -1;
}

int store_salva_motoristas(const Store *s)
{
    FILE *f = abre(s, "motoristas.csv", "wb");
    int i;
    if (!f) return -1;
    fputs("id;nome;rg\n", f);
    for (i = 0; i < s->n_motoristas; i++) {
        char id[16];
        snprintf(id, sizeof id, "%d", s->motoristas[i].id);
        csv_linha(f, 3, id, s->motoristas[i].nome, s->motoristas[i].rg);
    }
    return fclose(f) == 0 ? 0 : -1;
}

int store_salva_analistas(const Store *s)
{
    FILE *f = abre(s, "analistas.csv", "wb");
    int i;
    if (!f) return -1;
    fputs("id;nome\n", f);
    for (i = 0; i < s->n_analistas; i++) {
        char id[16];
        snprintf(id, sizeof id, "%d", s->analistas[i].id);
        csv_linha(f, 2, id, s->analistas[i].nome);
    }
    return fclose(f) == 0 ? 0 : -1;
}

int store_registra_historico(const Store *s, const Formulario *f,
                             const char *arquivo_pdf)
{
    char p[640];
    FILE *h;
    int novo, i;
    char quando[32];
    char produtos[512] = "", volumes[512] = "";

    caminho(s, "historico.csv", p, sizeof p);
    h = fopen(p, "rb");
    novo = (h == NULL);
    if (h) fclose(h);

    h = fopen(p, "ab");
    if (!h) return -1;
    if (novo)
        fputs("data_hora;arquivo;nota_fiscal;data_coleta;distribuidor;"
              "transportador;placa;motorista;analista;produtos;volumes\n", h);

    for (i = 0; i < f->n_colunas; i++) {
        if (i) { strncat(produtos, " | ", sizeof produtos - strlen(produtos) - 1);
                 strncat(volumes,  " | ", sizeof volumes  - strlen(volumes)  - 1); }
        if (f->colunas[i].produto)
            strncat(produtos, f->colunas[i].produto->nome,
                    sizeof produtos - strlen(produtos) - 1);
        strncat(volumes, f->colunas[i].volume,
                sizeof volumes - strlen(volumes) - 1);
    }

    agora_iso(quando, sizeof quando);
    csv_linha(h, 11, quando, arquivo_pdf, f->nota_fiscal, f->data_coleta,
              f->distribuidor, f->transportador, f->placa, f->motorista,
              f->analista, produtos, volumes);

    return fclose(h) == 0 ? 0 : -1;
}

/* ---------- manipulação das listas ---------- */

static int proximo_id_generico(const void *lista, int n, size_t tam)
{
    int maior = 0, i;
    for (i = 0; i < n; i++) {
        int id = *(const int *)((const char *)lista + (size_t)i * tam);
        if (id > maior) maior = id;
    }
    return maior + 1;
}

int store_add_empresa(Empresa *lista, int *n, const char *nome, const char *cnpj)
{
    int i;
    if (*n >= MAX_CADASTROS || str_empty(nome)) return -1;
    for (i = 0; i < *n; i++)
        if (!strcmp(lista[i].nome, nome)) return -1;

    memset(&lista[*n], 0, sizeof lista[*n]);
    lista[*n].id = proximo_id_generico(lista, *n, sizeof *lista);
    str_copy(lista[*n].nome, sizeof lista[*n].nome, nome);
    cnpj_formata(cnpj, lista[*n].cnpj, sizeof lista[*n].cnpj);
    return (*n)++;
}

int store_add_placa(Store *s, const char *placa)
{
    int i;
    if (s->n_placas >= MAX_CADASTROS || str_empty(placa)) return -1;
    for (i = 0; i < s->n_placas; i++)
        if (!strcmp(s->placas[i].placa, placa)) return -1;

    s->placas[s->n_placas].id =
        proximo_id_generico(s->placas, s->n_placas, sizeof *s->placas);
    str_copy(s->placas[s->n_placas].placa,
             sizeof s->placas[s->n_placas].placa, placa);
    return s->n_placas++;
}

int store_add_motorista(Store *s, const char *nome, const char *rg)
{
    int i;
    if (s->n_motoristas >= MAX_CADASTROS || str_empty(nome)) return -1;
    for (i = 0; i < s->n_motoristas; i++)
        if (!strcmp(s->motoristas[i].nome, nome)) return -1;

    s->motoristas[s->n_motoristas].id =
        proximo_id_generico(s->motoristas, s->n_motoristas, sizeof *s->motoristas);
    str_copy(s->motoristas[s->n_motoristas].nome,
             sizeof s->motoristas[s->n_motoristas].nome, nome);
    str_copy(s->motoristas[s->n_motoristas].rg,
             sizeof s->motoristas[s->n_motoristas].rg, rg);
    return s->n_motoristas++;
}

int store_add_analista(Store *s, const char *nome)
{
    int i;
    if (s->n_analistas >= MAX_CADASTROS || str_empty(nome)) return -1;
    for (i = 0; i < s->n_analistas; i++)
        if (!strcmp(s->analistas[i].nome, nome)) return -1;

    s->analistas[s->n_analistas].id =
        proximo_id_generico(s->analistas, s->n_analistas, sizeof *s->analistas);
    str_copy(s->analistas[s->n_analistas].nome,
             sizeof s->analistas[s->n_analistas].nome, nome);
    return s->n_analistas++;
}

void store_remove(void *lista, int *n, size_t tam_item, int indice)
{
    char *base = (char *)lista;
    if (indice < 0 || indice >= *n) return;
    memmove(base + (size_t)indice * tam_item,
            base + (size_t)(indice + 1) * tam_item,
            (size_t)(*n - indice - 1) * tam_item);
    (*n)--;
}

int store_indice_padrao(const Empresa *lista, int n)
{
    int i;
    for (i = 0; i < n; i++) if (lista[i].padrao) return i;
    return 0;
}
