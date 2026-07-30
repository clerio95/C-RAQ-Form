/* ui_admin.c — tela de administração: dados fixos do posto, pasta de saída,
   produtos com seus padrões e faixas, e manutenção de todos os cadastros.

   Protegida por senha (padrão "admin", alterável aqui mesmo). O hash é um
   FNV-1a: serve para impedir que o operador curioso mexa na configuração, não
   para resistir a um ataque de verdade — e o arquivo de dados fica ao lado do
   executável de qualquer jeito. */
#include "ui_admin.h"
#include "store.h"
#include "util.h"
#include "win_util.h"

#include <stdio.h>
#include <string.h>

#define ID_RAZAO        100
#define ID_CNPJ         101
#define ID_ENDERECO     102
#define ID_BAIRRO       103
#define ID_CIDADE       104
#define ID_PASTA        105

#define ID_TIPO_CAD     200
#define ID_LISTA_CAD    201
#define ID_CAD_NOVO     202
#define ID_CAD_EDITAR   203
#define ID_CAD_EXCLUIR  204
#define ID_CAD_PADRAO   205

#define ID_LISTA_PROD   300
#define ID_PROD_NOVO    301
#define ID_PROD_EDITAR  302
#define ID_PROD_EXCLUIR 303

#define ID_SENHA        400
#define ID_SALVAR       401
#define ID_FECHAR       402

/* Editor de produto */
#define ID_P_NOME       500
#define ID_P_ASPECTO    501
#define ID_P_COR        502
#define ID_P_MASSA_MIN  503
#define ID_P_MASSA_MAX  504
#define ID_P_USA_GAS    505
#define ID_P_GAS_MIN    506
#define ID_P_GAS_MAX    507
#define ID_P_USA_AEHC   508
#define ID_P_AEHC_MIN   509
#define ID_P_AEHC_MAX   510
#define ID_P_USA_BIO    511
#define ID_P_BIO_MIN    512
#define ID_P_BIO_MAX    513

enum { CAD_DISTRIBUIDOR = 0, CAD_TRANSPORTADOR, CAD_PLACA, CAD_MOTORISTA,
       CAD_ANALISTA, CAD_N };

typedef struct {
    Store *store;
    HFONT  fonte;
    int    alterou;

    HWND razao, cnpj, endereco, bairro, cidade, pasta;
    HWND tipo_cad, lista_cad, btn_padrao;
    HWND lista_prod;
} Admin;

static Admin *admin_de(HWND h)
{
    return (Admin *)GetWindowLongPtrW(h, GWLP_USERDATA);
}

static void numero_br(double v, int casas, char *dst, size_t n)
{
    char *p;
    snprintf(dst, n, "%.*f", casas, v);
    for (p = dst; *p; p++) if (*p == '.') *p = ',';
}

/* ---------- listas ---------- */

static void lista_add(HWND lista, const char *utf8)
{
    wchar_t w[512];
    u8_para_w(utf8, w, (int)(sizeof w / sizeof w[0]));
    SendMessageW(lista, LB_ADDSTRING, 0, (LPARAM)w);
}

static int lista_sel(HWND lista)
{
    return (int)SendMessageW(lista, LB_GETCURSEL, 0, 0);
}

static void recarrega_produtos(Admin *a)
{
    int i;
    SendMessageW(a->lista_prod, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < a->store->n_produtos; i++) {
        const Produto *p = &a->store->produtos[i];
        char linha[320], mmin[32], mmax[32];
        numero_br(p->massa_min, 2, mmin, sizeof mmin);
        numero_br(p->massa_max, 2, mmax, sizeof mmax);
        snprintf(linha, sizeof linha,
                 "%s   —   %s / %s   —   massa %s a %s kg/m³",
                 p->nome, p->aspecto_padrao, p->cor_padrao, mmin, mmax);
        lista_add(a->lista_prod, linha);
    }
}

static void recarrega_cadastro(Admin *a)
{
    Store *s = a->store;
    int tipo = combo_sel(a->tipo_cad);
    int i;

    SendMessageW(a->lista_cad, LB_RESETCONTENT, 0, 0);
    EnableWindow(a->btn_padrao, tipo == CAD_DISTRIBUIDOR || tipo == CAD_TRANSPORTADOR);

    switch (tipo) {
    case CAD_DISTRIBUIDOR:
    case CAD_TRANSPORTADOR: {
        const Empresa *lista = (tipo == CAD_DISTRIBUIDOR) ? s->distribuidores
                                                          : s->transportadores;
        int n = (tipo == CAD_DISTRIBUIDOR) ? s->n_distribuidores
                                           : s->n_transportadores;
        for (i = 0; i < n; i++) {
            char linha[320];
            snprintf(linha, sizeof linha, "%s%s   —   %s",
                     lista[i].padrao ? "* " : "", lista[i].nome, lista[i].cnpj);
            lista_add(a->lista_cad, linha);
        }
        break;
    }
    case CAD_PLACA:
        for (i = 0; i < s->n_placas; i++) lista_add(a->lista_cad, s->placas[i].placa);
        break;
    case CAD_MOTORISTA:
        for (i = 0; i < s->n_motoristas; i++) {
            char linha[320];
            snprintf(linha, sizeof linha, "%s   —   RG %s",
                     s->motoristas[i].nome, s->motoristas[i].rg);
            lista_add(a->lista_cad, linha);
        }
        break;
    case CAD_ANALISTA:
        for (i = 0; i < s->n_analistas; i++)
            lista_add(a->lista_cad, s->analistas[i].nome);
        break;
    }
}

/* Toda gravação passa por aqui para que uma falha de disco apareça na hora,
   em vez de o cadastro sumir na próxima abertura do programa. */
static void salva_cadastro(Admin *a, int tipo)
{
    int r = -1;
    switch (tipo) {
    case CAD_DISTRIBUIDOR:  r = store_salva_distribuidores(a->store);  break;
    case CAD_TRANSPORTADOR: r = store_salva_transportadores(a->store); break;
    case CAD_PLACA:         r = store_salva_placas(a->store);          break;
    case CAD_MOTORISTA:     r = store_salva_motoristas(a->store);      break;
    case CAD_ANALISTA:      r = store_salva_analistas(a->store);       break;
    }
    a->alterou = 1;
    if (r != 0) {
        char m[900];
        snprintf(m, sizeof m,
                 "O cadastro não pôde ser gravado em:\n\n%s\n\n"
                 "Ele vale só até fechar o programa. Verifique a permissão "
                 "de escrita nessa pasta.", a->store->dir);
        erro(NULL, "Erro ao salvar", m);
    }
}

/* ---------- operações sobre os cadastros ---------- */

static void cadastro_novo(HWND h, Admin *a)
{
    Store *s = a->store;
    int tipo = combo_sel(a->tipo_cad);
    char nome[MAX_NOME] = "", extra[MAX_CNPJ] = "";
    int r = -1;

    switch (tipo) {
    case CAD_DISTRIBUIDOR:
    case CAD_TRANSPORTADOR:
        if (!pede_texto(h, "Novo cadastro", "Nome / Razão social:", nome, sizeof nome, 0)) return;
        if (!pede_texto(h, "Novo cadastro", "CNPJ:", extra, sizeof extra, 0)) return;
        if (!cnpj_valido(extra) &&
            !pergunta(h, "Novo cadastro",
                      "O CNPJ não passou na verificação do dígito.\n\nCadastrar assim mesmo?"))
            return;
        r = (tipo == CAD_DISTRIBUIDOR)
          ? store_add_empresa(s->distribuidores, &s->n_distribuidores, nome, extra)
          : store_add_empresa(s->transportadores, &s->n_transportadores, nome, extra);
        break;
    case CAD_PLACA:
        if (!pede_texto(h, "Nova placa", "Placa:", nome, sizeof nome, 0)) return;
        r = store_add_placa(s, nome);
        break;
    case CAD_MOTORISTA:
        if (!pede_texto(h, "Novo motorista", "Nome:", nome, sizeof nome, 0)) return;
        if (!pede_texto(h, "Novo motorista", "RG:", extra, sizeof extra, 0)) return;
        r = store_add_motorista(s, nome, extra);
        break;
    case CAD_ANALISTA:
        if (!pede_texto(h, "Novo analista", "Nome:", nome, sizeof nome, 0)) return;
        r = store_add_analista(s, nome);
        break;
    }

    if (r < 0) { erro(h, "Cadastro", "Nome repetido ou lista cheia."); return; }
    salva_cadastro(a, tipo);
    recarrega_cadastro(a);
}

static void cadastro_editar(HWND h, Admin *a)
{
    Store *s = a->store;
    int tipo = combo_sel(a->tipo_cad);
    int i = lista_sel(a->lista_cad);
    char nome[MAX_NOME], extra[MAX_CNPJ];

    if (i < 0) { aviso(h, "Editar", "Selecione um registro na lista."); return; }

    switch (tipo) {
    case CAD_DISTRIBUIDOR:
    case CAD_TRANSPORTADOR: {
        Empresa *e = (tipo == CAD_DISTRIBUIDOR) ? &s->distribuidores[i]
                                                : &s->transportadores[i];
        if (i >= (tipo == CAD_DISTRIBUIDOR ? s->n_distribuidores : s->n_transportadores))
            return;
        str_copy(nome, sizeof nome, e->nome);
        if (!pede_texto(h, "Editar", "Nome / Razão social:", nome, sizeof nome, 0)) return;
        str_copy(extra, sizeof extra, e->cnpj);
        if (!pede_texto(h, "Editar", "CNPJ:", extra, sizeof extra, 0)) return;
        str_copy(e->nome, sizeof e->nome, nome);
        cnpj_formata(extra, e->cnpj, sizeof e->cnpj);
        break;
    }
    case CAD_PLACA:
        if (i >= s->n_placas) return;
        str_copy(nome, sizeof nome, s->placas[i].placa);
        if (!pede_texto(h, "Editar", "Placa:", nome, sizeof nome, 0)) return;
        str_copy(s->placas[i].placa, sizeof s->placas[i].placa, nome);
        break;
    case CAD_MOTORISTA:
        if (i >= s->n_motoristas) return;
        str_copy(nome, sizeof nome, s->motoristas[i].nome);
        if (!pede_texto(h, "Editar", "Nome:", nome, sizeof nome, 0)) return;
        str_copy(extra, sizeof extra, s->motoristas[i].rg);
        if (!pede_texto(h, "Editar", "RG:", extra, sizeof extra, 0)) return;
        str_copy(s->motoristas[i].nome, sizeof s->motoristas[i].nome, nome);
        str_copy(s->motoristas[i].rg,   sizeof s->motoristas[i].rg,   extra);
        break;
    case CAD_ANALISTA:
        if (i >= s->n_analistas) return;
        str_copy(nome, sizeof nome, s->analistas[i].nome);
        if (!pede_texto(h, "Editar", "Nome:", nome, sizeof nome, 0)) return;
        str_copy(s->analistas[i].nome, sizeof s->analistas[i].nome, nome);
        break;
    }

    salva_cadastro(a, tipo);
    recarrega_cadastro(a);
}

static void cadastro_excluir(HWND h, Admin *a)
{
    Store *s = a->store;
    int tipo = combo_sel(a->tipo_cad);
    int i = lista_sel(a->lista_cad);

    if (i < 0) { aviso(h, "Excluir", "Selecione um registro na lista."); return; }
    if (!pergunta(h, "Excluir",
                  "Excluir o registro selecionado?\n\n"
                  "Os formulários já gerados não são afetados."))
        return;

    switch (tipo) {
    case CAD_DISTRIBUIDOR:  store_remove(s->distribuidores,  &s->n_distribuidores,  sizeof(Empresa),   i); break;
    case CAD_TRANSPORTADOR: store_remove(s->transportadores, &s->n_transportadores, sizeof(Empresa),   i); break;
    case CAD_PLACA:         store_remove(s->placas,          &s->n_placas,          sizeof(Placa),     i); break;
    case CAD_MOTORISTA:     store_remove(s->motoristas,      &s->n_motoristas,      sizeof(Motorista), i); break;
    case CAD_ANALISTA:      store_remove(s->analistas,       &s->n_analistas,       sizeof(Analista),  i); break;
    }

    salva_cadastro(a, tipo);
    recarrega_cadastro(a);
}

static void cadastro_padrao(HWND h, Admin *a)
{
    Store *s = a->store;
    int tipo = combo_sel(a->tipo_cad);
    int i = lista_sel(a->lista_cad);
    Empresa *lista;
    int n, k;

    if (tipo != CAD_DISTRIBUIDOR && tipo != CAD_TRANSPORTADOR) return;
    if (i < 0) { aviso(h, "Padrão", "Selecione um registro na lista."); return; }

    lista = (tipo == CAD_DISTRIBUIDOR) ? s->distribuidores : s->transportadores;
    n     = (tipo == CAD_DISTRIBUIDOR) ? s->n_distribuidores : s->n_transportadores;
    if (i >= n) return;

    for (k = 0; k < n; k++) lista[k].padrao = 0;
    lista[i].padrao = 1;

    salva_cadastro(a, tipo);
    recarrega_cadastro(a);
    SendMessageW(a->lista_cad, LB_SETCURSEL, (WPARAM)i, 0);
}

/* ---------- editor de produto ---------- */

typedef struct {
    Produto *p;
    int      confirmou;
    HFONT    fonte;
    HWND nome, aspecto, cor, massa_min, massa_max;
    HWND usa_gas, gas_min, gas_max;
    HWND usa_aehc, aehc_min, aehc_max;
    HWND usa_bio, bio_min, bio_max;
} EditorProduto;

static void ep_define_num(HWND ctl, double v, int casas)
{
    char t[32];
    numero_br(v, casas, t, sizeof t);
    ctl_define_texto(ctl, t);
}

static double ep_le_num(HWND ctl)
{
    char t[64];
    double v = 0;
    ctl_texto(ctl, t, sizeof t);
    num_parse(t, &v);
    return v;
}

static void ep_atualiza_habilitados(EditorProduto *e)
{
    int g = SendMessageW(e->usa_gas,  BM_GETCHECK, 0, 0) == BST_CHECKED;
    int a = SendMessageW(e->usa_aehc, BM_GETCHECK, 0, 0) == BST_CHECKED;
    int b = SendMessageW(e->usa_bio,  BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(e->gas_min,  g); EnableWindow(e->gas_max,  g);
    EnableWindow(e->aehc_min, a); EnableWindow(e->aehc_max, a);
    EnableWindow(e->bio_min,  b); EnableWindow(e->bio_max,  b);
}

static LRESULT CALLBACK editor_produto_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    EditorProduto *e = (EditorProduto *)GetWindowLongPtrW(h, GWLP_USERDATA);

    switch (m) {
    case WM_CREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lp;
        HFONT f;
        int y = 14;

        e = (EditorProduto *)cs->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)e);
        f = e->fonte;

        cria_rotulo(h, "Nome do produto:", 14, y + 4, 150, 18, f);
        e->nome = cria_edit(h, 170, y, 250, 24, ID_P_NOME, f);
        y += 32;

        cria_rotulo(h, "Aspecto padrão:", 14, y + 4, 150, 18, f);
        e->aspecto = cria_edit(h, 170, y, 250, 24, ID_P_ASPECTO, f);
        y += 32;

        cria_rotulo(h, "Cor padrão:", 14, y + 4, 150, 18, f);
        e->cor = cria_edit(h, 170, y, 250, 24, ID_P_COR, f);
        y += 40;

        cria_rotulo(h, "Massa específica (kg/m³) — mín.:", 14, y + 4, 175, 18, f);
        e->massa_min = cria_edit(h, 190, y, 100, 24, ID_P_MASSA_MIN, f);
        cria_rotulo(h, "máximo:", 300, y + 4, 55, 18, f);
        e->massa_max = cria_edit(h, 360, y, 100, 24, ID_P_MASSA_MAX, f);
        y += 40;

        e->usa_gas = cria_check(h, "Exige teor de álcool na gasolina",
                                14, y + 2, 250, 20, ID_P_USA_GAS, f);
        y += 26;
        cria_rotulo(h, "mínimo:", 34, y + 4, 60, 18, f);
        e->gas_min = cria_edit(h, 100, y, 100, 24, ID_P_GAS_MIN, f);
        cria_rotulo(h, "máximo:", 220, y + 4, 55, 18, f);
        e->gas_max = cria_edit(h, 280, y, 100, 24, ID_P_GAS_MAX, f);
        y += 38;

        e->usa_aehc = cria_check(h, "Exige teor alcoólico no AEHC",
                                 14, y + 2, 250, 20, ID_P_USA_AEHC, f);
        y += 26;
        cria_rotulo(h, "mínimo:", 34, y + 4, 60, 18, f);
        e->aehc_min = cria_edit(h, 100, y, 100, 24, ID_P_AEHC_MIN, f);
        cria_rotulo(h, "máximo:", 220, y + 4, 55, 18, f);
        e->aehc_max = cria_edit(h, 280, y, 100, 24, ID_P_AEHC_MAX, f);
        y += 38;

        e->usa_bio = cria_check(h, "Exige teor de biodiesel",
                                14, y + 2, 250, 20, ID_P_USA_BIO, f);
        y += 26;
        cria_rotulo(h, "mínimo:", 34, y + 4, 60, 18, f);
        e->bio_min = cria_edit(h, 100, y, 100, 24, ID_P_BIO_MIN, f);
        cria_rotulo(h, "máximo:", 220, y + 4, 55, 18, f);
        e->bio_max = cria_edit(h, 280, y, 100, 24, ID_P_BIO_MAX, f);
        cria_rotulo(h, "(os teores são em %)", 390, y + 4, 170, 18, f);
        y += 44;

        cria_ctl(h, L"BUTTON", "Salvar", BS_DEFPUSHBUTTON | WS_TABSTOP,
                 280, y, 90, 28, IDOK, f);
        cria_botao(h, "Cancelar", 380, y, 90, 28, IDCANCEL, f);

        ctl_define_texto(e->nome,    e->p->nome);
        ctl_define_texto(e->aspecto, e->p->aspecto_padrao);
        ctl_define_texto(e->cor,     e->p->cor_padrao);
        ep_define_num(e->massa_min, e->p->massa_min, 2);
        ep_define_num(e->massa_max, e->p->massa_max, 2);
        SendMessageW(e->usa_gas, BM_SETCHECK,
                     e->p->usa_teor_gas ? BST_CHECKED : BST_UNCHECKED, 0);
        ep_define_num(e->gas_min, e->p->gas_min, 1);
        ep_define_num(e->gas_max, e->p->gas_max, 1);
        SendMessageW(e->usa_aehc, BM_SETCHECK,
                     e->p->usa_teor_aehc ? BST_CHECKED : BST_UNCHECKED, 0);
        ep_define_num(e->aehc_min, e->p->aehc_min, 1);
        ep_define_num(e->aehc_max, e->p->aehc_max, 1);
        SendMessageW(e->usa_bio, BM_SETCHECK,
                     e->p->usa_teor_bio ? BST_CHECKED : BST_UNCHECKED, 0);
        ep_define_num(e->bio_min, e->p->bio_min, 1);
        ep_define_num(e->bio_max, e->p->bio_max, 1);
        ep_atualiza_habilitados(e);
        SetFocus(e->nome);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_P_USA_GAS:
        case ID_P_USA_AEHC:
        case ID_P_USA_BIO:
            ep_atualiza_habilitados(e);
            return 0;

        case IDOK: {
            char nome[MAX_NOME];
            double mmin, mmax;

            ctl_texto(e->nome, nome, sizeof nome);
            if (str_empty(nome)) {
                aviso(h, "Produto", "Informe o nome do produto.");
                return 0;
            }
            mmin = ep_le_num(e->massa_min);
            mmax = ep_le_num(e->massa_max);
            if (mmin > mmax) {
                aviso(h, "Produto",
                      "O mínimo da massa específica não pode ser maior que o máximo.");
                return 0;
            }

            str_copy(e->p->nome, sizeof e->p->nome, nome);
            ctl_texto(e->aspecto, e->p->aspecto_padrao, sizeof e->p->aspecto_padrao);
            ctl_texto(e->cor,     e->p->cor_padrao,     sizeof e->p->cor_padrao);
            e->p->massa_min = mmin;
            e->p->massa_max = mmax;
            e->p->usa_teor_gas = SendMessageW(e->usa_gas, BM_GETCHECK, 0, 0) == BST_CHECKED;
            e->p->gas_min  = ep_le_num(e->gas_min);
            e->p->gas_max  = ep_le_num(e->gas_max);
            e->p->usa_teor_aehc = SendMessageW(e->usa_aehc, BM_GETCHECK, 0, 0) == BST_CHECKED;
            e->p->aehc_min = ep_le_num(e->aehc_min);
            e->p->aehc_max = ep_le_num(e->aehc_max);
            e->p->usa_teor_bio = SendMessageW(e->usa_bio, BM_GETCHECK, 0, 0) == BST_CHECKED;
            e->p->bio_min = ep_le_num(e->bio_min);
            e->p->bio_max = ep_le_num(e->bio_max);
            e->p->ativo = 1;

            e->confirmou = 1;
            DestroyWindow(h);
            return 0;
        }
        case IDCANCEL:
            DestroyWindow(h);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

static int edita_produto(HWND pai, Produto *p, HFONT fonte)
{
    static int registrado = 0;
    HINSTANCE inst = GetModuleHandleW(NULL);
    EditorProduto e;
    HWND h;

    if (!registrado) {
        WNDCLASSW wc;
        memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc   = editor_produto_proc;
        wc.hInstance     = inst;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"RaqEditorProduto";
        RegisterClassW(&wc);
        registrado = 1;
    }

    memset(&e, 0, sizeof e);
    e.p = p;
    e.fonte = fonte;

    h = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RaqEditorProduto", L"Produto",
                        WS_POPUPWINDOW | WS_CAPTION,
                        CW_USEDEFAULT, CW_USEDEFAULT, 590, 470,
                        pai, NULL, inst, &e);
    if (!h) return 0;

    roda_modal(h, pai);
    return e.confirmou;
}

/* ---------- operações sobre produtos ---------- */

static void produto_novo(HWND h, Admin *a)
{
    Store *s = a->store;
    Produto novo;
    int i;

    if (s->n_produtos >= MAX_PRODUTOS) {
        erro(h, "Produtos", "Limite de produtos atingido.");
        return;
    }

    memset(&novo, 0, sizeof novo);
    str_copy(novo.aspecto_padrao, sizeof novo.aspecto_padrao, "L.I.I.");
    str_copy(novo.cor_padrao, sizeof novo.cor_padrao, "AMARELADA");
    novo.ativo = 1;

    if (!edita_produto(h, &novo, a->fonte)) return;

    for (i = 0; i < s->n_produtos; i++)
        if (!strcmp(s->produtos[i].nome, novo.nome)) {
            erro(h, "Produtos", "Já existe um produto com esse nome.");
            return;
        }

    novo.id = s->n_produtos + 1;
    s->produtos[s->n_produtos++] = novo;
    if (store_salva_produtos(s) != 0)
        erro(h, "Erro ao salvar", "O produto não pôde ser gravado no disco.");
    a->alterou = 1;
    recarrega_produtos(a);

    aviso(h, "Produtos",
          "Produto cadastrado. Feche e abra o programa para que ele apareça "
          "na tela de preenchimento.");
}

static void produto_editar(HWND h, Admin *a)
{
    int i = lista_sel(a->lista_prod);
    if (i < 0 || i >= a->store->n_produtos) {
        aviso(h, "Produtos", "Selecione um produto na lista.");
        return;
    }
    if (!edita_produto(h, &a->store->produtos[i], a->fonte)) return;
    if (store_salva_produtos(a->store) != 0)
        erro(h, "Erro ao salvar", "A alteração não pôde ser gravada no disco.");
    a->alterou = 1;
    recarrega_produtos(a);
}

static void produto_excluir(HWND h, Admin *a)
{
    Store *s = a->store;
    int i = lista_sel(a->lista_prod);

    if (i < 0 || i >= s->n_produtos) {
        aviso(h, "Produtos", "Selecione um produto na lista.");
        return;
    }
    if (s->n_produtos <= 1) {
        erro(h, "Produtos", "É preciso manter ao menos um produto cadastrado.");
        return;
    }
    if (!pergunta(h, "Excluir produto",
                  "Excluir o produto selecionado?\n\n"
                  "Os formulários já gerados não são afetados."))
        return;

    store_remove(s->produtos, &s->n_produtos, sizeof(Produto), i);
    if (store_salva_produtos(s) != 0)
        erro(h, "Erro ao salvar", "A exclusão não pôde ser gravada no disco.");
    a->alterou = 1;
    recarrega_produtos(a);

    aviso(h, "Produtos",
          "Produto excluído. Feche e abra o programa para atualizar a tela "
          "de preenchimento.");
}

/* ---------- senha ---------- */

static void altera_senha(HWND h, Admin *a)
{
    char nova[128] = "", conf[128] = "";

    if (!pede_texto(h, "Senha", "Nova senha do administrador:", nova, sizeof nova, 1))
        return;
    if (!pede_texto(h, "Senha", "Repita a nova senha:", conf, sizeof conf, 1))
        return;
    if (strcmp(nova, conf)) {
        erro(h, "Senha", "As senhas não conferem. Nada foi alterado.");
        return;
    }
    senha_hash(nova, a->store->config.senha_hash, sizeof a->store->config.senha_hash);
    a->alterou = 1;
    if (store_salva_config(a->store) != 0) {
        erro(h, "Senha",
             "A nova senha não pôde ser gravada e volta a ser a anterior "
             "quando o programa fechar.");
        return;
    }
    aviso(h, "Senha", "Senha alterada.");
}

/* ---------- janela de administração ---------- */

static int salva_posto(Admin *a)
{
    Config *c = &a->store->config;
    char cnpj[MAX_CNPJ];

    ctl_texto(a->razao,    c->razao_social,  sizeof c->razao_social);
    ctl_texto(a->endereco, c->endereco,      sizeof c->endereco);
    ctl_texto(a->bairro,   c->bairro,        sizeof c->bairro);
    ctl_texto(a->cidade,   c->cidade_estado, sizeof c->cidade_estado);
    ctl_texto(a->pasta,    c->pasta_saida,   sizeof c->pasta_saida);

    ctl_texto(a->cnpj, cnpj, sizeof cnpj);
    cnpj_formata(cnpj, c->cnpj, sizeof c->cnpj);

    if (str_empty(c->pasta_saida))
        str_copy(c->pasta_saida, sizeof c->pasta_saida, "Formularios");

    a->alterou = 1;
    return store_salva_config(a->store) == 0;
}

static LRESULT CALLBACK admin_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    Admin *a = admin_de(h);

    switch (m) {
    case WM_CREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lp;
        HFONT f;
        int y = 14, i;
        static const char *TIPOS[CAD_N] = {
            "Distribuidores", "Transportadores", "Placas", "Motoristas", "Analistas"
        };

        a = (Admin *)cs->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)a);
        f = a->fonte;

        /* --- dados do posto --- */
        cria_grupo(h, "Dados fixos do posto", 12, y, 660, 210, f);
        y += 26;
        cria_rotulo(h, "Razão social:", 26, y + 4, 130, 18, f);
        a->razao = cria_edit(h, 160, y, 496, 24, ID_RAZAO, f);
        y += 30;
        cria_rotulo(h, "CNPJ:", 26, y + 4, 130, 18, f);
        a->cnpj = cria_edit(h, 160, y, 220, 24, ID_CNPJ, f);
        y += 30;
        cria_rotulo(h, "Endereço:", 26, y + 4, 130, 18, f);
        a->endereco = cria_edit(h, 160, y, 496, 24, ID_ENDERECO, f);
        y += 30;
        cria_rotulo(h, "Bairro:", 26, y + 4, 130, 18, f);
        a->bairro = cria_edit(h, 160, y, 220, 24, ID_BAIRRO, f);
        y += 30;
        cria_rotulo(h, "Cidade/Estado:", 26, y + 4, 130, 18, f);
        a->cidade = cria_edit(h, 160, y, 220, 24, ID_CIDADE, f);
        y += 30;
        cria_rotulo(h, "Pasta dos PDFs:", 26, y + 4, 130, 18, f);
        a->pasta = cria_edit(h, 160, y, 496, 24, ID_PASTA, f);
        y += 28;

        /* Mostrar a pasta de dados evita a dúvida mais cara de diagnosticar:
           "cadastrei e sumiu" quase sempre é o exe rodando de outro lugar. */
        {
            char info[700];
            snprintf(info, sizeof info, "Cadastros gravados em:  %s",
                     a->store->dir);
            cria_rotulo(h, info, 26, y, 630, 18, f);
        }
        y += 26;

        /* --- produtos --- */
        cria_grupo(h, "Produtos, padrões e faixas", 12, y, 660, 180, f);
        a->lista_prod = cria_ctl(h, L"LISTBOX", "",
                                 WS_BORDER | WS_VSCROLL | LBS_NOTIFY | WS_TABSTOP,
                                 26, y + 26, 520, 140, ID_LISTA_PROD, f);
        cria_botao(h, "Novo...",   556, y + 26,  100, 28, ID_PROD_NOVO,    f);
        cria_botao(h, "Editar...", 556, y + 60,  100, 28, ID_PROD_EDITAR,  f);
        cria_botao(h, "Excluir",   556, y + 94,  100, 28, ID_PROD_EXCLUIR, f);
        y += 194;

        /* --- cadastros --- */
        cria_grupo(h, "Cadastros", 12, y, 660, 200, f);
        cria_rotulo(h, "Tipo:", 26, y + 30, 40, 18, f);
        a->tipo_cad = cria_combo(h, 70, y + 26, 200, 200, ID_TIPO_CAD, f);
        for (i = 0; i < CAD_N; i++) combo_add(a->tipo_cad, TIPOS[i]);
        combo_define_sel(a->tipo_cad, 0);

        a->lista_cad = cria_ctl(h, L"LISTBOX", "",
                                WS_BORDER | WS_VSCROLL | LBS_NOTIFY | WS_TABSTOP,
                                26, y + 58, 520, 128, ID_LISTA_CAD, f);
        cria_botao(h, "Novo...",   556, y + 26,  100, 28, ID_CAD_NOVO,    f);
        cria_botao(h, "Editar...", 556, y + 60,  100, 28, ID_CAD_EDITAR,  f);
        cria_botao(h, "Excluir",   556, y + 94,  100, 28, ID_CAD_EXCLUIR, f);
        a->btn_padrao = cria_botao(h, "Definir padrão", 556, y + 128, 100, 28,
                                   ID_CAD_PADRAO, f);
        y += 210;

        /* --- rodapé --- */
        cria_botao(h, "Alterar senha...", 26, y, 140, 30, ID_SENHA, f);
        cria_ctl(h, L"BUTTON", "Salvar dados do posto",
                 BS_DEFPUSHBUTTON | WS_TABSTOP, 380, y, 170, 30, ID_SALVAR, f);
        cria_botao(h, "Fechar", 560, y, 110, 30, ID_FECHAR, f);

        ctl_define_texto(a->razao,    a->store->config.razao_social);
        ctl_define_texto(a->cnpj,     a->store->config.cnpj);
        ctl_define_texto(a->endereco, a->store->config.endereco);
        ctl_define_texto(a->bairro,   a->store->config.bairro);
        ctl_define_texto(a->cidade,   a->store->config.cidade_estado);
        ctl_define_texto(a->pasta,    a->store->config.pasta_saida);
        recarrega_produtos(a);
        recarrega_cadastro(a);
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_TIPO_CAD:
            if (HIWORD(wp) == CBN_SELCHANGE) recarrega_cadastro(a);
            return 0;

        case ID_CAD_NOVO:    cadastro_novo(h, a);    return 0;
        case ID_CAD_EDITAR:  cadastro_editar(h, a);  return 0;
        case ID_CAD_EXCLUIR: cadastro_excluir(h, a); return 0;
        case ID_CAD_PADRAO:  cadastro_padrao(h, a);  return 0;

        case ID_LISTA_PROD:
            if (HIWORD(wp) == LBN_DBLCLK) produto_editar(h, a);
            return 0;
        case ID_LISTA_CAD:
            if (HIWORD(wp) == LBN_DBLCLK) cadastro_editar(h, a);
            return 0;

        case ID_PROD_NOVO:    produto_novo(h, a);    return 0;
        case ID_PROD_EDITAR:  produto_editar(h, a);  return 0;
        case ID_PROD_EXCLUIR: produto_excluir(h, a); return 0;

        case ID_SENHA: altera_senha(h, a); return 0;

        case ID_SALVAR:
            if (salva_posto(a)) {
                aviso(h, "Administração", "Dados do posto salvos.");
            } else {
                char m[900];
                snprintf(m, sizeof m,
                         "Não foi possível gravar em:\n\n%s\n\n"
                         "Verifique se você tem permissão de escrita nessa "
                         "pasta. Programa instalado em \"Arquivos de "
                         "Programas\" costuma bloquear a gravação — mova a "
                         "pasta do RAQ para o Desktop ou para C:\\RAQ.",
                         a->store->dir);
                erro(h, "Erro ao salvar", m);
            }
            return 0;

        case ID_FECHAR:
        case IDCANCEL:
            DestroyWindow(h);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int ui_admin_abre(HWND pai, Store *store)
{
    static int registrado = 0;
    HINSTANCE inst = GetModuleHandleW(NULL);
    Admin a;
    HWND h;
    char senha[128] = "", hash[64];

    if (!pede_texto(pai, "Administração", "Senha:", senha, sizeof senha, 1))
        return 0;

    senha_hash(senha, hash, sizeof hash);
    if (strcmp(hash, store->config.senha_hash)) {
        erro(pai, "Administração", "Senha incorreta.");
        return 0;
    }

    if (!registrado) {
        WNDCLASSW wc;
        memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc   = admin_proc;
        wc.hInstance     = inst;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"RaqAdmin";
        RegisterClassW(&wc);
        registrado = 1;
    }

    memset(&a, 0, sizeof a);
    a.store = store;
    a.fonte = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                          0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                          L"Segoe UI");

    {
        RECT r;
        /* 690x700 cobre exatamente o conteúdo montado em admin_proc e ainda
           cabe numa tela de 768 px de altura, comum em PC de posto. */
        r.left = 0; r.top = 0; r.right = 690; r.bottom = 700;
        AdjustWindowRect(&r, WS_POPUPWINDOW | WS_CAPTION, FALSE);
        h = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RaqAdmin", L"Administração",
                            WS_POPUPWINDOW | WS_CAPTION,
                            CW_USEDEFAULT, CW_USEDEFAULT,
                            r.right - r.left, r.bottom - r.top,
                            pai, NULL, inst, &a);
    }
    if (!h) { DeleteObject(a.fonte); return 0; }

    roda_modal(h, pai);
    DeleteObject(a.fonte);
    return a.alterou;
}
