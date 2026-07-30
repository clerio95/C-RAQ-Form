/* ui_form.c — janela principal: o formulário que o usuário preenche.

   Regras de interface que valem a pena registrar:
   - marcar um produto revela o campo de volume e a coluna dele no bloco de
     análise; desmarcar esconde. A tabela do PDF acompanha.
   - campos fora da faixa do admin ficam com fundo avermelhado, mas não
     travam nada: uma carga fora de especificação precisa poder ser
     registrada.
   - as linhas de teor só aparecem para os produtos a que se aplicam. */
#include "ui_form.h"
#include "backend_gdi.h"
#include "backend_pdf.h"
#include "store.h"
#include "ui_admin.h"
#include "util.h"
#include "win_util.h"

#include <stdio.h>
#include <string.h>

/* ---------- identificadores dos controles ---------- */

#define ID_CHECK_PRODUTO   1000   /* + índice do produto */
#define ID_VOLUME          1100   /* + índice do produto */

#define ID_DATA            2000
#define ID_DISTRIBUIDOR    2001
#define ID_NOVO_DIST       2002
#define ID_TRANSPORTADOR   2004
#define ID_NOVO_TRANSP     2005
#define ID_NF              2007
#define ID_PLACA           2008
#define ID_NOVA_PLACA      2009
#define ID_MOTORISTA       2010
#define ID_NOVO_MOTORISTA  2011
#define ID_ANALISTA        2013
#define ID_NOVO_ANALISTA   2014
#define ID_RESPONSAVEL     2015

/* Bloco de análise: 3000 + coluna*10 + linha */
#define ID_ANALISE         3000
#define ID_ANALISE_COL(id) (((id) - ID_ANALISE) / 10)
#define ID_ANALISE_LIN(id) (((id) - ID_ANALISE) % 10)

#define ID_GERAR           4000
#define ID_LIMPAR          4001
#define ID_ADMIN           4002

/* ---------- geometria ---------- */

#define LARG_CLIENTE   960
#define MARGEM          14
#define ALT_LINHA_UI    30
#define ALT_CTL         24
#define LARG_ROTULO    150
#define LARG_COLUNA    185
#define X_COL0         180

/* ---------- estado ---------- */

typedef struct {
    HWND check, volume;
} CtlProduto;

typedef struct {
    HWND cabecalho;
    HWND aspecto, cor, massa, teor_gas, teor_aehc, teor_bio;
    HWND faixa_massa, faixa_gas, faixa_aehc, faixa_bio;
    HWND rot_massa, rot_gas, rot_aehc;   /* nada: rótulos são da coluna 0 */
} CtlColuna;

typedef struct {
    Store *store;
    HFONT  fonte, fonte_peq, fonte_titulo;
    HBRUSH pincel_erro;

    HWND posto_razao, posto_cnpj, posto_endereco;
    CtlProduto produto[MAX_PRODUTOS];
    CtlColuna  coluna[MAX_PRODUTOS];

    HWND data, distribuidor, cnpj_dist, transportador, cnpj_transp;
    HWND nf, placa, motorista, rg_motorista, analista, responsavel;
    HWND rot_analise[ANALISE_N];

    /* 1 quando o valor digitado está fora da faixa do produto */
    int fora_faixa[MAX_PRODUTOS][ANALISE_N];

    int altura_total;      /* altura do conteúdo, para a barra de rolagem */
    int desloc;            /* deslocamento atual da rolagem, em pixels */
    int responsavel_tocado;/* usuário editou o responsável à mão */
    int n_produtos_ui;     /* produtos que têm controles criados na tela */

    /* Escrever num EDIT dispara EN_CHANGE igual à digitação. Sem esta trava,
       o próprio programa preenchendo o campo "Responsável" o marcaria como
       editado pelo usuário, e ele pararia de seguir o analista. */
    int preenchendo;
} App;

static App *app_de(HWND h)
{
    return (App *)GetWindowLongPtrW(h, GWLP_USERDATA);
}

/* ---------- helpers ---------- */

static void numero_br(double v, int casas, char *dst, size_t n)
{
    char *p;
    snprintf(dst, n, "%.*f", casas, v);
    for (p = dst; *p; p++) if (*p == '.') *p = ',';
}

/* `unidade` sai junto dos dois extremos: os teores são percentuais e ficam
   ambíguos sem o símbolo. A massa específica passa "". */
static void texto_faixa(char *dst, size_t n, double min, double max, int casas,
                        const char *unidade)
{
    char a[32], b[32];
    numero_br(min, casas, a, sizeof a);
    numero_br(max, casas, b, sizeof b);
    snprintf(dst, n, "faixa: %s%s a %s%s", a, unidade, b, unidade);
}

static int produto_marcado(const App *a, int i)
{
    return a->produto[i].check
        && SendMessageW(a->produto[i].check, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

/* ---------- preenchimento dos combos ---------- */

static void carrega_combos(App *a)
{
    Store *s = a->store;
    int i;

    combo_limpa(a->distribuidor);
    for (i = 0; i < s->n_distribuidores; i++)
        combo_add(a->distribuidor, s->distribuidores[i].nome);
    if (s->n_distribuidores)
        combo_define_sel(a->distribuidor,
                         store_indice_padrao(s->distribuidores, s->n_distribuidores));

    combo_limpa(a->transportador);
    for (i = 0; i < s->n_transportadores; i++)
        combo_add(a->transportador, s->transportadores[i].nome);
    if (s->n_transportadores)
        combo_define_sel(a->transportador,
                         store_indice_padrao(s->transportadores, s->n_transportadores));

    combo_limpa(a->placa);
    for (i = 0; i < s->n_placas; i++) combo_add(a->placa, s->placas[i].placa);
    if (s->n_placas) combo_define_sel(a->placa, 0);

    combo_limpa(a->motorista);
    for (i = 0; i < s->n_motoristas; i++) combo_add(a->motorista, s->motoristas[i].nome);
    if (s->n_motoristas) combo_define_sel(a->motorista, 0);

    combo_limpa(a->analista);
    for (i = 0; i < s->n_analistas; i++) combo_add(a->analista, s->analistas[i].nome);
    if (s->n_analistas) combo_define_sel(a->analista, 0);
}

/* Os campos derivados (CNPJ, RG) acompanham a escolha nos combos. */
static void atualiza_derivados(App *a)
{
    Store *s = a->store;
    int i;

    i = combo_sel(a->distribuidor);
    ctl_define_texto(a->cnpj_dist,
                     (i >= 0 && i < s->n_distribuidores) ? s->distribuidores[i].cnpj : "");

    i = combo_sel(a->transportador);
    ctl_define_texto(a->cnpj_transp,
                     (i >= 0 && i < s->n_transportadores) ? s->transportadores[i].cnpj : "");

    i = combo_sel(a->motorista);
    ctl_define_texto(a->rg_motorista,
                     (i >= 0 && i < s->n_motoristas) ? s->motoristas[i].rg : "");

    /* O responsável segue o analista até o usuário mexer nele. */
    if (!a->responsavel_tocado) {
        i = combo_sel(a->analista);
        a->preenchendo = 1;
        ctl_define_texto(a->responsavel,
                         (i >= 0 && i < s->n_analistas) ? s->analistas[i].nome : "");
        a->preenchendo = 0;
    }
}

static void atualiza_cabecalho(App *a)
{
    const Config *c = &a->store->config;
    char buf[768];

    snprintf(buf, sizeof buf, "Razão social:  %s",
             str_empty(c->razao_social) ? "(não configurado — abra Administração)"
                                        : c->razao_social);
    ctl_define_texto(a->posto_razao, buf);

    snprintf(buf, sizeof buf, "CNPJ:  %s", c->cnpj);
    ctl_define_texto(a->posto_cnpj, buf);

    snprintf(buf, sizeof buf, "Endereço:  %s   —   %s   —   %s",
             c->endereco, c->bairro, c->cidade_estado);
    ctl_define_texto(a->posto_endereco, buf);
}

/* ---------- visibilidade das colunas de análise ---------- */

static void aplica_visibilidade(App *a)
{
    int i;

    /* Percorre os controles existentes, não os produtos do cadastro: o admin
       pode ter excluído produtos desde que a tela foi montada, e os controles
       que sobraram precisam sumir em vez de exibir um nome velho. */
    for (i = 0; i < a->n_produtos_ui; i++) {
        const Produto *p;
        int on, mostra;
        CtlColuna *c = &a->coluna[i];

        if (i >= a->store->n_produtos) {
            ShowWindow(a->produto[i].check,  SW_HIDE);
            ShowWindow(a->produto[i].volume, SW_HIDE);
            ShowWindow(c->cabecalho,   SW_HIDE);
            ShowWindow(c->aspecto,     SW_HIDE);
            ShowWindow(c->cor,         SW_HIDE);
            ShowWindow(c->massa,       SW_HIDE);
            ShowWindow(c->faixa_massa, SW_HIDE);
            ShowWindow(c->teor_gas,    SW_HIDE);
            ShowWindow(c->faixa_gas,   SW_HIDE);
            ShowWindow(c->teor_aehc,   SW_HIDE);
            ShowWindow(c->faixa_aehc,  SW_HIDE);
            ShowWindow(c->teor_bio,    SW_HIDE);
            ShowWindow(c->faixa_bio,   SW_HIDE);
            continue;
        }

        p = &a->store->produtos[i];
        ShowWindow(a->produto[i].check, SW_SHOW);
        on = produto_marcado(a, i);
        mostra = on ? SW_SHOW : SW_HIDE;

        ShowWindow(a->produto[i].volume, mostra);
        ShowWindow(c->cabecalho, mostra);
        ShowWindow(c->aspecto,   mostra);
        ShowWindow(c->cor,       mostra);
        ShowWindow(c->massa,     mostra);
        ShowWindow(c->faixa_massa, mostra);

        /* Teores só existem para os produtos a que se aplicam. */
        ShowWindow(c->teor_gas,  (on && p->usa_teor_gas)  ? SW_SHOW : SW_HIDE);
        ShowWindow(c->faixa_gas, (on && p->usa_teor_gas)  ? SW_SHOW : SW_HIDE);
        ShowWindow(c->teor_aehc, (on && p->usa_teor_aehc) ? SW_SHOW : SW_HIDE);
        ShowWindow(c->faixa_aehc,(on && p->usa_teor_aehc) ? SW_SHOW : SW_HIDE);
        ShowWindow(c->teor_bio,  (on && p->usa_teor_bio)  ? SW_SHOW : SW_HIDE);
        ShowWindow(c->faixa_bio, (on && p->usa_teor_bio)  ? SW_SHOW : SW_HIDE);
    }
}

/* Preenche a coluna com os padrões do produto quando ele é marcado. */
static void aplica_padroes(App *a, int i)
{
    const Produto *p = &a->store->produtos[i];
    CtlColuna *c = &a->coluna[i];
    char faixa[64];

    ctl_define_texto(c->aspecto, p->aspecto_padrao);
    ctl_define_texto(c->cor,     p->cor_padrao);

    texto_faixa(faixa, sizeof faixa, p->massa_min, p->massa_max, 2, " kg/m³");
    ctl_define_texto(c->faixa_massa, faixa);

    if (p->usa_teor_gas) {
        texto_faixa(faixa, sizeof faixa, p->gas_min, p->gas_max, 1, "%");
        ctl_define_texto(c->faixa_gas, faixa);
    }
    if (p->usa_teor_aehc) {
        texto_faixa(faixa, sizeof faixa, p->aehc_min, p->aehc_max, 1, "%");
        ctl_define_texto(c->faixa_aehc, faixa);
    }
    if (p->usa_teor_bio) {
        texto_faixa(faixa, sizeof faixa, p->bio_min, p->bio_max, 1, "%");
        ctl_define_texto(c->faixa_bio, faixa);
    }
}

/* Marca em vermelho o que estiver fora da faixa. */
static void revalida_faixas(App *a, int col)
{
    const Produto *p = &a->store->produtos[col];
    CtlColuna *c = &a->coluna[col];
    struct { HWND ctl; int linha; double min, max; int usa; } campos[4];
    int i;

    campos[0].ctl = c->massa;     campos[0].linha = ANALISE_MASSA;
    campos[0].min = p->massa_min; campos[0].max = p->massa_max; campos[0].usa = 1;
    campos[1].ctl = c->teor_gas;  campos[1].linha = ANALISE_TEOR_GAS;
    campos[1].min = p->gas_min;   campos[1].max = p->gas_max;   campos[1].usa = p->usa_teor_gas;
    campos[2].ctl = c->teor_aehc; campos[2].linha = ANALISE_TEOR_AEHC;
    campos[2].min = p->aehc_min;  campos[2].max = p->aehc_max;  campos[2].usa = p->usa_teor_aehc;
    campos[3].ctl = c->teor_bio;  campos[3].linha = ANALISE_TEOR_BIO;
    campos[3].min = p->bio_min;   campos[3].max = p->bio_max;   campos[3].usa = p->usa_teor_bio;

    for (i = 0; i < 4; i++) {
        char txt[64];
        double v;
        int ruim = 0;

        if (campos[i].usa) {
            ctl_texto(campos[i].ctl, txt, sizeof txt);
            if (!str_empty(txt) && num_parse(txt, &v))
                ruim = (v < campos[i].min || v > campos[i].max);
        }
        if (a->fora_faixa[col][campos[i].linha] != ruim) {
            a->fora_faixa[col][campos[i].linha] = ruim;
            InvalidateRect(campos[i].ctl, NULL, TRUE);
        }
    }
}

/* ---------- construção da interface ---------- */

static int monta_bloco_posto(App *a, HWND h, int y)
{
    cria_grupo(h, "Posto revendedor  (configurado pelo administrador)",
               MARGEM, y, LARG_CLIENTE - 2 * MARGEM, 92, a->fonte);
    a->posto_razao    = cria_rotulo(h, "", MARGEM + 14, y + 24, 900, 18, a->fonte);
    a->posto_cnpj     = cria_rotulo(h, "", MARGEM + 14, y + 44, 900, 18, a->fonte);
    a->posto_endereco = cria_rotulo(h, "", MARGEM + 14, y + 64, 900, 18, a->fonte);
    return y + 92 + 10;
}

static int monta_bloco_produtos(App *a, HWND h, int y)
{
    int n = a->store->n_produtos;
    int linhas = (n + 1) / 2;
    int altura = 28 + linhas * ALT_LINHA_UI + 10;
    int i;

    cria_grupo(h, "Produtos recebidos  (marque o que chegou e informe o volume)",
               MARGEM, y, LARG_CLIENTE - 2 * MARGEM, altura, a->fonte);

    for (i = 0; i < n; i++) {
        int col = i % 2, lin = i / 2;
        int x = MARGEM + 16 + col * 466;
        int cy = y + 26 + lin * ALT_LINHA_UI;

        a->produto[i].check = cria_check(h, a->store->produtos[i].nome,
                                         x, cy + 3, 180, 20,
                                         ID_CHECK_PRODUTO + i, a->fonte);
        cria_rotulo(h, "Volume (litros):", x + 190, cy + 5, 100, 18, a->fonte);
        a->produto[i].volume = cria_edit(h, x + 294, cy, 120, ALT_CTL,
                                         ID_VOLUME + i, a->fonte);
        ShowWindow(a->produto[i].volume, SW_HIDE);
    }
    a->n_produtos_ui = n;
    return y + altura + 10;
}

/* Uma linha "rótulo + controle" da coluna esquerda ou direita. */
static void linha_campo(App *a, HWND h, int coluna, int y, const char *rotulo,
                        HWND *destino, int tipo, int id, int com_botao,
                        int id_botao)
{
    int x  = MARGEM + 16 + coluna * 470;
    int xf = x + LARG_ROTULO;
    int larg = com_botao ? 250 : 285;

    cria_rotulo(h, rotulo, x, y + 5, LARG_ROTULO - 6, 18, a->fonte);

    if (tipo == 0)                        /* edit */
        *destino = cria_edit(h, xf, y, larg, ALT_CTL, id, a->fonte);
    else if (tipo == 1)                   /* combo */
        *destino = cria_combo(h, xf, y, larg, 260, id, a->fonte);
    else                                  /* somente leitura */
        *destino = cria_ctl(h, L"EDIT", "",
                            WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
                            xf, y, larg, ALT_CTL, id, a->fonte);

    if (com_botao)
        cria_botao(h, "Novo...", xf + larg + 6, y, 60, ALT_CTL, id_botao, a->fonte);
}

static int monta_bloco_recebimento(App *a, HWND h, int y)
{
    int altura = 28 + 6 * ALT_LINHA_UI + 10;
    int l = y + 26;

    cria_grupo(h, "Dados de recebimento", MARGEM, y,
               LARG_CLIENTE - 2 * MARGEM, altura, a->fonte);

    linha_campo(a, h, 0, l, "Data da coleta:",  &a->data,          0, ID_DATA, 0, 0);
    linha_campo(a, h, 1, l, "Nota Fiscal:",     &a->nf,            0, ID_NF,   0, 0);
    l += ALT_LINHA_UI;

    linha_campo(a, h, 0, l, "Distribuidor:",    &a->distribuidor,  1, ID_DISTRIBUIDOR,  1, ID_NOVO_DIST);
    linha_campo(a, h, 1, l, "CNPJ Distribuidor:", &a->cnpj_dist,   2, -1, 0, 0);
    l += ALT_LINHA_UI;

    linha_campo(a, h, 0, l, "Transportador:",   &a->transportador, 1, ID_TRANSPORTADOR, 1, ID_NOVO_TRANSP);
    linha_campo(a, h, 1, l, "CNPJ Transportador:", &a->cnpj_transp, 2, -1, 0, 0);
    l += ALT_LINHA_UI;

    linha_campo(a, h, 0, l, "Placa:",           &a->placa,         1, ID_PLACA,     1, ID_NOVA_PLACA);
    l += ALT_LINHA_UI;

    linha_campo(a, h, 0, l, "Motorista:",       &a->motorista,     1, ID_MOTORISTA, 1, ID_NOVO_MOTORISTA);
    linha_campo(a, h, 1, l, "RG do Motorista:", &a->rg_motorista,  2, -1, 0, 0);
    l += ALT_LINHA_UI;

    linha_campo(a, h, 0, l, "Analista:",        &a->analista,      1, ID_ANALISTA,  1, ID_NOVO_ANALISTA);

    return y + altura + 10;
}

static int monta_bloco_analise(App *a, HWND h, int y)
{
    static const char *ROT[ANALISE_N] = {
        "Aspecto:", "Cor:", "Massa Específica a 20°C (kg/m³):",
        "Teor de álcool na Gasolina:", "Teor Alcoólico no AEHC:",
        "Teor de Biodiesel:"
    };
    static const char *ASPECTOS[] = { "L.I.I.", "L.I.I.C.", "TURVO", "COM IMPUREZAS" };
    static const char *CORES[]    = { "AMARELADA", "ESVERDEADA", "INCOLOR", "LARANJA" };

    int altura = 28 + 24 + 2 * ALT_LINHA_UI + 4 * 44 + 10;
    int y_cab = y + 26;
    int y_asp = y_cab + 24;
    int y_cor = y_asp + ALT_LINHA_UI;
    int y_mas = y_cor + ALT_LINHA_UI;
    int y_gas = y_mas + 44;
    int y_aeh = y_gas + 44;
    int y_bio = y_aeh + 44;
    int i, k;

    cria_grupo(h, "Resultados da análise", MARGEM, y,
               LARG_CLIENTE - 2 * MARGEM, altura, a->fonte);

    a->rot_analise[0] = cria_rotulo(h, ROT[0], MARGEM + 16, y_asp + 5, LARG_ROTULO + 10, 18, a->fonte);
    a->rot_analise[1] = cria_rotulo(h, ROT[1], MARGEM + 16, y_cor + 5, LARG_ROTULO + 10, 18, a->fonte);
    a->rot_analise[2] = cria_rotulo(h, ROT[2], MARGEM + 16, y_mas + 5, LARG_ROTULO + 10, 18, a->fonte);
    a->rot_analise[3] = cria_rotulo(h, ROT[3], MARGEM + 16, y_gas + 5, LARG_ROTULO + 10, 18, a->fonte);
    a->rot_analise[4] = cria_rotulo(h, ROT[4], MARGEM + 16, y_aeh + 5, LARG_ROTULO + 10, 18, a->fonte);
    a->rot_analise[5] = cria_rotulo(h, ROT[5], MARGEM + 16, y_bio + 5, LARG_ROTULO + 10, 18, a->fonte);

    for (i = 0; i < a->store->n_produtos && i < MAX_PRODUTOS; i++) {
        CtlColuna *c = &a->coluna[i];
        int x = X_COL0 + i * LARG_COLUNA;
        int w = LARG_COLUNA - 12;
        int base = ID_ANALISE + i * 10;

        c->cabecalho = cria_rotulo(h, a->store->produtos[i].nome, x, y_cab, w, 18, a->fonte);

        c->aspecto = cria_combo(h, x, y_asp, w, 200, base + ANALISE_ASPECTO, a->fonte);
        for (k = 0; k < (int)(sizeof ASPECTOS / sizeof ASPECTOS[0]); k++)
            combo_add(c->aspecto, ASPECTOS[k]);

        c->cor = cria_combo(h, x, y_cor, w, 200, base + ANALISE_COR, a->fonte);
        for (k = 0; k < (int)(sizeof CORES / sizeof CORES[0]); k++)
            combo_add(c->cor, CORES[k]);

        c->massa       = cria_edit(h, x, y_mas, w, ALT_CTL, base + ANALISE_MASSA, a->fonte);
        c->faixa_massa = cria_rotulo(h, "", x, y_mas + ALT_CTL + 2, w, 16, a->fonte_peq);

        c->teor_gas    = cria_edit(h, x, y_gas, w, ALT_CTL, base + ANALISE_TEOR_GAS, a->fonte);
        c->faixa_gas   = cria_rotulo(h, "", x, y_gas + ALT_CTL + 2, w, 16, a->fonte_peq);

        c->teor_aehc   = cria_edit(h, x, y_aeh, w, ALT_CTL, base + ANALISE_TEOR_AEHC, a->fonte);
        c->faixa_aehc  = cria_rotulo(h, "", x, y_aeh + ALT_CTL + 2, w, 16, a->fonte_peq);

        c->teor_bio    = cria_edit(h, x, y_bio, w, ALT_CTL, base + ANALISE_TEOR_BIO, a->fonte);
        c->faixa_bio   = cria_rotulo(h, "", x, y_bio + ALT_CTL + 2, w, 16, a->fonte_peq);
    }

    return y + altura + 10;
}

static int monta_rodape(App *a, HWND h, int y)
{
    cria_rotulo(h, "Responsável pelo Preenchimento:", MARGEM + 16, y + 5, 210, 18, a->fonte);
    a->responsavel = cria_edit(h, MARGEM + 230, y, 420, ALT_CTL, ID_RESPONSAVEL, a->fonte);
    y += ALT_LINHA_UI + 12;

    cria_ctl(h, L"BUTTON", "Gerar formulário", BS_DEFPUSHBUTTON | WS_TABSTOP,
             MARGEM + 16, y, 190, 36, ID_GERAR, a->fonte_titulo);
    cria_botao(h, "Limpar", MARGEM + 216, y, 110, 36, ID_LIMPAR, a->fonte);
    cria_botao(h, "Administração", LARG_CLIENTE - MARGEM - 150, y, 136, 36,
               ID_ADMIN, a->fonte);
    return y + 36 + MARGEM;
}

static void monta_tudo(App *a, HWND h)
{
    int y = MARGEM;
    y = monta_bloco_posto(a, h, y);
    y = monta_bloco_produtos(a, h, y);
    y = monta_bloco_recebimento(a, h, y);
    y = monta_bloco_analise(a, h, y);
    y = monta_rodape(a, h, y);
    a->altura_total = y;
}

/* ---------- rolagem ---------- */

static void ajusta_rolagem(HWND h)
{
    App *a = app_de(h);
    RECT r;
    SCROLLINFO si;

    if (!a) return;
    GetClientRect(h, &r);

    memset(&si, 0, sizeof si);
    si.cbSize = sizeof si;
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = a->altura_total;
    si.nPage  = (UINT)(r.bottom - r.top);
    si.nPos   = a->desloc;
    SetScrollInfo(h, SB_VERT, &si, TRUE);

    /* Se a janela cresceu além do conteúdo, volta o deslocamento. */
    if (a->desloc > a->altura_total - (int)si.nPage) {
        int novo = a->altura_total - (int)si.nPage;
        if (novo < 0) novo = 0;
        if (novo != a->desloc) {
            ScrollWindowEx(h, 0, a->desloc - novo, NULL, NULL, NULL, NULL,
                           SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
            a->desloc = novo;
            SetScrollPos(h, SB_VERT, a->desloc, TRUE);
        }
    }
}

static void rola(HWND h, int novo)
{
    App *a = app_de(h);
    RECT r;
    int maximo;

    GetClientRect(h, &r);
    maximo = a->altura_total - (r.bottom - r.top);
    if (maximo < 0) maximo = 0;
    if (novo < 0) novo = 0;
    if (novo > maximo) novo = maximo;
    if (novo == a->desloc) return;

    ScrollWindowEx(h, 0, a->desloc - novo, NULL, NULL, NULL, NULL,
                   SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    a->desloc = novo;
    SetScrollPos(h, SB_VERT, a->desloc, TRUE);
}

/* ---------- cadastros rápidos ---------- */

static void cadastra_empresa(HWND h, App *a, int transportador)
{
    Empresa *lista = transportador ? a->store->transportadores : a->store->distribuidores;
    int *n = transportador ? &a->store->n_transportadores : &a->store->n_distribuidores;
    HWND combo = transportador ? a->transportador : a->distribuidor;
    char nome[MAX_NOME] = "", cnpj[MAX_CNPJ] = "";
    const char *titulo = transportador ? "Novo transportador" : "Novo distribuidor";
    int i;

    if (!pede_texto(h, titulo, "Nome / Razão social:", nome, sizeof nome, 0))
        return;
    if (!pede_texto(h, titulo, "CNPJ:", cnpj, sizeof cnpj, 0))
        return;

    if (!cnpj_valido(cnpj) &&
        !pergunta(h, titulo,
                  "O CNPJ informado não passou na verificação do dígito.\n\n"
                  "Deseja cadastrar assim mesmo?"))
        return;

    i = store_add_empresa(lista, n, nome, cnpj);
    if (i < 0) { erro(h, titulo, "Não foi possível cadastrar: nome repetido ou lista cheia."); return; }

    if ((transportador ? store_salva_transportadores(a->store)
                       : store_salva_distribuidores(a->store)) != 0)
        erro(h, titulo, "O cadastro vale para este formulário, mas não pôde "
                        "ser gravado no disco.");

    combo_add(combo, nome);
    combo_define_sel(combo, i);
    atualiza_derivados(a);
}

static void cadastra_placa(HWND h, App *a)
{
    char placa[MAX_CAMPO] = "";
    int i;
    if (!pede_texto(h, "Nova placa", "Placa do caminhão/reboque:", placa, sizeof placa, 0))
        return;
    i = store_add_placa(a->store, placa);
    if (i < 0) { erro(h, "Nova placa", "Placa repetida ou lista cheia."); return; }
    if (store_salva_placas(a->store) != 0)
        erro(h, "Nova placa", "A placa vale para este formulário, mas não pôde "
                              "ser gravada no disco.");
    combo_add(a->placa, placa);
    combo_define_sel(a->placa, i);
}

static void cadastra_motorista(HWND h, App *a)
{
    char nome[MAX_NOME] = "", rg[MAX_CAMPO] = "";
    int i;
    if (!pede_texto(h, "Novo motorista", "Nome do motorista:", nome, sizeof nome, 0))
        return;
    if (!pede_texto(h, "Novo motorista", "RG do motorista:", rg, sizeof rg, 0))
        return;
    i = store_add_motorista(a->store, nome, rg);
    if (i < 0) { erro(h, "Novo motorista", "Nome repetido ou lista cheia."); return; }
    if (store_salva_motoristas(a->store) != 0)
        erro(h, "Novo motorista", "O motorista vale para este formulário, mas "
                                  "não pôde ser gravado no disco.");
    combo_add(a->motorista, nome);
    combo_define_sel(a->motorista, i);
    atualiza_derivados(a);
}

static void cadastra_analista(HWND h, App *a)
{
    char nome[MAX_NOME] = "";
    int i;
    if (!pede_texto(h, "Novo analista", "Nome do analista:", nome, sizeof nome, 0))
        return;
    i = store_add_analista(a->store, nome);
    if (i < 0) { erro(h, "Novo analista", "Nome repetido ou lista cheia."); return; }
    if (store_salva_analistas(a->store) != 0)
        erro(h, "Novo analista", "O analista vale para este formulário, mas não "
                                 "pôde ser gravado no disco.");
    combo_add(a->analista, nome);
    combo_define_sel(a->analista, i);
    atualiza_derivados(a);
}

/* ---------- coleta e validação ---------- */

/* Monta o Formulario a partir da tela. Devolve 0 se algo obrigatório faltar,
   escrevendo o motivo em `problema`. */
static int coleta(App *a, Formulario *f, char *problema, size_t nprob)
{
    Store *s = a->store;
    int i, sel;

    memset(f, 0, sizeof *f);
    f->config = &s->config;

    for (i = 0; i < s->n_produtos && i < a->n_produtos_ui; i++) {
        ColunaProduto *c;
        char txt[MAX_CAMPO];

        if (!produto_marcado(a, i)) continue;

        c = &f->colunas[f->n_colunas++];
        memset(c, 0, sizeof *c);
        c->produto = &s->produtos[i];

        ctl_texto(a->produto[i].volume, txt, sizeof txt);
        if (str_empty(txt)) {
            snprintf(problema, nprob,
                     "Informe o volume recebido de %s.", s->produtos[i].nome);
            return 0;
        }
        str_copy(c->volume, sizeof c->volume, txt);

        ctl_texto(a->coluna[i].aspecto, c->aspecto, sizeof c->aspecto);
        ctl_texto(a->coluna[i].cor,     c->cor,     sizeof c->cor);
        ctl_texto(a->coluna[i].massa,   c->massa,   sizeof c->massa);
        if (str_empty(c->massa)) {
            snprintf(problema, nprob,
                     "Informe a massa específica de %s.", s->produtos[i].nome);
            return 0;
        }
        if (s->produtos[i].usa_teor_gas) {
            ctl_texto(a->coluna[i].teor_gas, c->teor_gas, sizeof c->teor_gas);
            if (str_empty(c->teor_gas)) {
                snprintf(problema, nprob,
                         "Informe o teor de álcool de %s.", s->produtos[i].nome);
                return 0;
            }
        }
        if (s->produtos[i].usa_teor_aehc) {
            ctl_texto(a->coluna[i].teor_aehc, c->teor_aehc, sizeof c->teor_aehc);
            if (str_empty(c->teor_aehc)) {
                snprintf(problema, nprob,
                         "Informe o teor alcoólico de %s.", s->produtos[i].nome);
                return 0;
            }
        }
        if (s->produtos[i].usa_teor_bio) {
            ctl_texto(a->coluna[i].teor_bio, c->teor_bio, sizeof c->teor_bio);
            if (str_empty(c->teor_bio)) {
                snprintf(problema, nprob,
                         "Informe o teor de biodiesel de %s.", s->produtos[i].nome);
                return 0;
            }
        }
    }

    if (f->n_colunas == 0) {
        str_copy(problema, nprob, "Marque ao menos um produto recebido.");
        return 0;
    }

    ctl_texto(a->data, f->data_coleta, sizeof f->data_coleta);
    if (!data_valida(f->data_coleta)) {
        str_copy(problema, nprob, "A data da coleta deve estar no formato dd/mm/aaaa.");
        return 0;
    }

    ctl_texto(a->nf, f->nota_fiscal, sizeof f->nota_fiscal);
    if (str_empty(f->nota_fiscal)) {
        str_copy(problema, nprob, "Informe a nota fiscal do produto.");
        return 0;
    }

    sel = combo_sel(a->distribuidor);
    if (sel < 0 || sel >= s->n_distribuidores) {
        str_copy(problema, nprob, "Selecione o distribuidor.");
        return 0;
    }
    str_copy(f->distribuidor, sizeof f->distribuidor, s->distribuidores[sel].nome);
    str_copy(f->distribuidor_cnpj, sizeof f->distribuidor_cnpj, s->distribuidores[sel].cnpj);

    sel = combo_sel(a->transportador);
    if (sel < 0 || sel >= s->n_transportadores) {
        str_copy(problema, nprob, "Selecione o transportador.");
        return 0;
    }
    str_copy(f->transportador, sizeof f->transportador, s->transportadores[sel].nome);
    str_copy(f->transportador_cnpj, sizeof f->transportador_cnpj, s->transportadores[sel].cnpj);

    sel = combo_sel(a->placa);
    if (sel < 0 || sel >= s->n_placas) {
        str_copy(problema, nprob, "Selecione a placa do caminhão.");
        return 0;
    }
    str_copy(f->placa, sizeof f->placa, s->placas[sel].placa);

    sel = combo_sel(a->motorista);
    if (sel < 0 || sel >= s->n_motoristas) {
        str_copy(problema, nprob, "Selecione o motorista.");
        return 0;
    }
    str_copy(f->motorista, sizeof f->motorista, s->motoristas[sel].nome);
    str_copy(f->motorista_rg, sizeof f->motorista_rg, s->motoristas[sel].rg);

    sel = combo_sel(a->analista);
    if (sel < 0 || sel >= s->n_analistas) {
        str_copy(problema, nprob, "Selecione o analista.");
        return 0;
    }
    str_copy(f->analista, sizeof f->analista, s->analistas[sel].nome);

    ctl_texto(a->responsavel, f->responsavel, sizeof f->responsavel);
    if (str_empty(f->responsavel))
        str_copy(f->responsavel, sizeof f->responsavel, f->analista);

    return 1;
}

/* Lista o que está fora da faixa, para a confirmação antes de gerar. */
static int resume_fora_faixa(App *a, char *dst, size_t n)
{
    int i, achou = 0;
    dst[0] = '\0';
    for (i = 0; i < a->n_produtos_ui && i < a->store->n_produtos; i++) {
        int k;
        if (!produto_marcado(a, i)) continue;
        for (k = 0; k < ANALISE_N; k++) {
            static const char *nomes[ANALISE_N] = {
                "", "", "massa específica", "teor de álcool na gasolina",
                "teor alcoólico no AEHC", "teor de biodiesel"
            };
            char linha[160];
            if (!a->fora_faixa[i][k]) continue;
            snprintf(linha, sizeof linha, "  • %s: %s\n",
                     a->store->produtos[i].nome, nomes[k]);
            strncat(dst, linha, n - strlen(dst) - 1);
            achou = 1;
        }
    }
    return achou;
}

/* Caminho completo do PDF, evitando sobrescrever um arquivo existente. */
static void monta_caminho_pdf(App *a, const Formulario *f, char *dst, size_t n)
{
    char pasta[600], data[32], nf[64];
    int sufixo;

    if (a->store->config.pasta_saida[0] == '\0')
        str_copy(pasta, sizeof pasta, "Formularios");
    else
        str_copy(pasta, sizeof pasta, a->store->config.pasta_saida);

    /* Caminho relativo é resolvido a partir da pasta do executável. */
    if (!strchr(pasta, ':') && pasta[0] != '\\' && pasta[0] != '/') {
        char exe[512], completo[600];
        pasta_do_exe(exe, sizeof exe);
        snprintf(completo, sizeof completo, "%s\\%s", exe, pasta);
        str_copy(pasta, sizeof pasta, completo);
    }
    garante_pasta(pasta);

    /* dd/mm/aaaa -> aaaa-mm-dd */
    if (data_valida(f->data_coleta))
        snprintf(data, sizeof data, "%.4s-%.2s-%.2s",
                 f->data_coleta + 6, f->data_coleta + 3, f->data_coleta);
    else
        data_hoje_iso(data, sizeof data);

    so_digitos(f->nota_fiscal, nf, sizeof nf);
    if (str_empty(nf)) str_copy(nf, sizeof nf, "SN");

    snprintf(dst, n, "%s\\RAQ_%s_NF%s.pdf", pasta, data, nf);
    for (sufixo = 2; sufixo < 100; sufixo++) {
        if (!arquivo_existe(dst)) return;
        snprintf(dst, n, "%s\\RAQ_%s_NF%s_%d.pdf", pasta, data, nf, sufixo);
    }
}

static void limpa_formulario(App *a)
{
    int i;
    char hoje[32];

    for (i = 0; i < a->n_produtos_ui; i++) {
        SendMessageW(a->produto[i].check, BM_SETCHECK, BST_UNCHECKED, 0);
        ctl_define_texto(a->produto[i].volume, "");
        ctl_define_texto(a->coluna[i].massa, "");
        ctl_define_texto(a->coluna[i].teor_gas, "");
        ctl_define_texto(a->coluna[i].teor_aehc, "");
        ctl_define_texto(a->coluna[i].teor_bio, "");
        memset(a->fora_faixa[i], 0, sizeof a->fora_faixa[i]);
    }
    ctl_define_texto(a->nf, "");
    data_hoje(hoje, sizeof hoje);
    ctl_define_texto(a->data, hoje);
    a->responsavel_tocado = 0;
    atualiza_derivados(a);
    aplica_visibilidade(a);
}

static void gera(HWND h, App *a)
{
    Formulario f;
    char problema[256] = "", caminho[700], msg[1200], fora[600];

    if (!coleta(a, &f, problema, sizeof problema)) {
        aviso(h, "Faltam dados", problema);
        return;
    }

    if (resume_fora_faixa(a, fora, sizeof fora)) {
        snprintf(msg, sizeof msg,
                 "Os valores abaixo estão fora da faixa esperada:\n\n%s\n"
                 "Deseja gerar o formulário assim mesmo?", fora);
        if (!pergunta(h, "Valores fora da faixa", msg)) return;
    }

    monta_caminho_pdf(a, &f, caminho, sizeof caminho);

    if (raq_gera_pdf(&f, caminho) != 0) {
        snprintf(msg, sizeof msg,
                 "Não foi possível gravar o arquivo:\n\n%s\n\n"
                 "Verifique se a pasta existe e se você tem permissão de escrita.",
                 caminho);
        erro(h, "Erro ao salvar", msg);
        return;
    }

    store_registra_historico(a->store, &f, caminho);

    snprintf(msg, sizeof msg,
             "Formulário salvo em:\n\n%s\n\nDeseja imprimir agora?", caminho);
    if (pergunta(h, "Formulário gerado", msg)) {
        if (raq_imprime(&f, h) != 0)
            erro(h, "Impressão",
                 "Não foi possível imprimir. O arquivo PDF já está salvo e "
                 "pode ser impresso manualmente.");
    }

    limpa_formulario(a);
}

/* ---------- procedimento da janela ---------- */

static LRESULT CALLBACK form_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    App *a = app_de(h);

    switch (m) {
    case WM_CREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lp;
        char hoje[32];

        a = (App *)cs->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)a);

        a->fonte        = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                      DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        a->fonte_peq    = CreateFontW(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                      DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        a->fonte_titulo = CreateFontW(-16, 0, 0, 0, FW_BOLD, 0, 0, 0,
                                      DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                                      DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        a->pincel_erro  = CreateSolidBrush(RGB(255, 226, 226));

        monta_tudo(a, h);
        atualiza_cabecalho(a);
        carrega_combos(a);
        atualiza_derivados(a);
        data_hoje(hoje, sizeof hoje);
        ctl_define_texto(a->data, hoje);
        aplica_visibilidade(a);
        return 0;
    }

    case WM_COMMAND: {
        int id  = LOWORD(wp);
        int cod = HIWORD(wp);

        if (id >= ID_CHECK_PRODUTO && id < ID_CHECK_PRODUTO + MAX_PRODUTOS) {
            int i = id - ID_CHECK_PRODUTO;
            if (produto_marcado(a, i)) aplica_padroes(a, i);
            aplica_visibilidade(a);
            return 0;
        }

        if (id >= ID_ANALISE && id < ID_ANALISE + MAX_PRODUTOS * 10) {
            if (cod == EN_CHANGE) revalida_faixas(a, ID_ANALISE_COL(id));
            return 0;
        }

        switch (id) {
        case ID_DISTRIBUIDOR:
        case ID_TRANSPORTADOR:
        case ID_MOTORISTA:
        case ID_ANALISTA:
            if (cod == CBN_SELCHANGE) atualiza_derivados(a);
            return 0;

        case ID_RESPONSAVEL:
            if (cod == EN_CHANGE && !a->preenchendo) a->responsavel_tocado = 1;
            return 0;

        case ID_NOVO_DIST:      cadastra_empresa(h, a, 0);  return 0;
        case ID_NOVO_TRANSP:    cadastra_empresa(h, a, 1);  return 0;
        case ID_NOVA_PLACA:     cadastra_placa(h, a);       return 0;
        case ID_NOVO_MOTORISTA: cadastra_motorista(h, a);   return 0;
        case ID_NOVO_ANALISTA:  cadastra_analista(h, a);    return 0;

        case ID_GERAR:  gera(h, a); return 0;
        case ID_LIMPAR:
            if (pergunta(h, "Limpar", "Deseja limpar todos os campos preenchidos?"))
                limpa_formulario(a);
            return 0;
        case ID_ADMIN:
            if (ui_admin_abre(h, a->store)) ui_form_recarrega(h);
            return 0;
        }
        break;
    }

    /* Fundo avermelhado nos campos fora da faixa. */
    case WM_CTLCOLOREDIT: {
        HWND ctl = (HWND)lp;
        int id = GetDlgCtrlID(ctl);
        if (id >= ID_ANALISE && id < ID_ANALISE + MAX_PRODUTOS * 10) {
            int col = ID_ANALISE_COL(id), lin = ID_ANALISE_LIN(id);
            if (col >= 0 && col < MAX_PRODUTOS && lin < ANALISE_N
                && a->fora_faixa[col][lin]) {
                SetBkColor((HDC)wp, RGB(255, 226, 226));
                return (LRESULT)a->pincel_erro;
            }
        }
        break;
    }

    case WM_VSCROLL: {
        int pos = a->desloc;
        switch (LOWORD(wp)) {
        case SB_LINEUP:   pos -= 30; break;
        case SB_LINEDOWN: pos += 30; break;
        case SB_PAGEUP:   pos -= 200; break;
        case SB_PAGEDOWN: pos += 200; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO si;
            memset(&si, 0, sizeof si);
            si.cbSize = sizeof si;
            si.fMask  = SIF_TRACKPOS;
            GetScrollInfo(h, SB_VERT, &si);
            pos = si.nTrackPos;
            break;
        }
        default: return 0;
        }
        rola(h, pos);
        return 0;
    }

    case WM_MOUSEWHEEL:
        rola(h, a->desloc - GET_WHEEL_DELTA_WPARAM(wp) / 2);
        return 0;

    case WM_SIZE:
        ajusta_rolagem(h);
        return 0;

    case WM_ERASEBKGND: {
        RECT r;
        GetClientRect(h, &r);
        FillRect((HDC)wp, &r, (HBRUSH)(COLOR_BTNFACE + 1));
        return 1;
    }

    case WM_CLOSE:
        if (!pergunta(h, "Sair", "Deseja fechar o programa?")) return 0;
        DestroyWindow(h);
        return 0;

    case WM_DESTROY:
        if (a) {
            DeleteObject(a->fonte);
            DeleteObject(a->fonte_peq);
            DeleteObject(a->fonte_titulo);
            DeleteObject(a->pincel_erro);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

void ui_form_recarrega(HWND janela)
{
    App *a = app_de(janela);
    int i;
    if (!a) return;

    atualiza_cabecalho(a);
    carrega_combos(a);
    atualiza_derivados(a);

    /* Nomes e faixas dos produtos podem ter mudado. */
    for (i = 0; i < a->n_produtos_ui && i < a->store->n_produtos; i++) {
        ctl_define_texto(a->produto[i].check, a->store->produtos[i].nome);
        ctl_define_texto(a->coluna[i].cabecalho, a->store->produtos[i].nome);
        if (produto_marcado(a, i)) { aplica_padroes(a, i); revalida_faixas(a, i); }
    }
    aplica_visibilidade(a);
    InvalidateRect(janela, NULL, TRUE);
}

HWND ui_form_cria(HINSTANCE inst, Store *store)
{
    static App app;
    WNDCLASSW wc;
    HWND h;
    RECT r;

    memset(&app, 0, sizeof app);
    app.store = store;

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = form_proc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"RaqFormPrincipal";
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    if (!RegisterClassW(&wc)) return NULL;

    /* Dimensiona a janela para caber o conteúdo, sem passar da tela. */
    r.left = 0; r.top = 0; r.right = LARG_CLIENTE; r.bottom = 800;
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW | WS_VSCROLL, FALSE);
    {
        int lt = r.right - r.left;
        int at = r.bottom - r.top;
        int max_a = GetSystemMetrics(SM_CYMAXIMIZED) - 40;
        if (at > max_a) at = max_a;
        h = CreateWindowExW(0, L"RaqFormPrincipal",
                            L"Formulário de Registro da Análise da Qualidade",
                            WS_OVERLAPPEDWINDOW | WS_VSCROLL,
                            CW_USEDEFAULT, CW_USEDEFAULT, lt, at,
                            NULL, NULL, inst, &app);
    }
    if (!h) return NULL;

    ShowWindow(h, SW_SHOW);
    ajusta_rolagem(h);
    UpdateWindow(h);
    return h;
}
