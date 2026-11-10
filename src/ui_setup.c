/* ui_setup.c — assistente da primeira execução.

   Roda antes da tela do formulário quando ainda não existe senha de
   administrador nem dados do posto. É uma janela só, com páginas que
   aparecem e somem; nada vai para o disco até "Concluir", então cancelar no
   meio deixa a instalação exatamente como estava (e o assistente volta na
   próxima abertura). */
#include "ui_setup.h"
#include "backend_gdi.h"
#include "store.h"
#include "util.h"
#include "win_util.h"

#include <stdio.h>
#include <string.h>

enum {
    PG_SENHA = 0, PG_POSTO, PG_PRODUTOS, PG_ANALISTAS, PG_DISTRIBUIDORES,
    PG_TRANSPORTADORES, PG_SAIDA, PG_N
};

#define MAX_CTL_PAGINA 48

#define ID_VOLTAR        100
#define ID_AVANCAR       IDOK       /* Enter aciona Avançar */
#define ID_CANCELAR      IDCANCEL   /* Esc aciona Cancelar */

#define ID_SENHA1        200
#define ID_SENHA2        201
#define ID_RAZAO         210
#define ID_CNPJ          211
#define ID_ENDERECO      212
#define ID_BAIRRO        213
#define ID_CIDADE        214
#define ID_PRODUTO       300        /* + índice no catálogo */
#define ID_LISTA_NOME    400        /* + página */
#define ID_LISTA_CNPJ    410
#define ID_LISTA_ADD     420
#define ID_LISTA_DEL     430
#define ID_LISTA         440
#define ID_PASTA         500
#define ID_PROCURAR      501
#define ID_IMPRESSORA    502

#define LARG_CLIENTE     640
#define ALT_CLIENTE      540
#define X0               24
#define Y_CONTEUDO       112

typedef struct {
    HWND nome, cnpj, lista;
} PaginaLista;

typedef struct {
    Store *store;
    int    pagina;
    int    concluido;
    HFONT  fonte, fonte_titulo;

    HWND titulo, descricao;
    HWND btn_voltar, btn_avancar;

    HWND ctl[PG_N][MAX_CTL_PAGINA];
    int  n_ctl[PG_N];

    HWND senha1, senha2;
    HWND razao, cnpj, endereco, bairro, cidade;
    HWND produto[MAX_PRODUTOS];
    int  n_catalogo;
    PaginaLista lista[PG_N];
    HWND pasta, impressora;
    char impressoras[32][256];
    int  n_impressoras;
} Assistente;

static const char *TITULOS[PG_N] = {
    "Senha do administrador",
    "Dados do posto",
    "Produtos que o posto recebe",
    "Analistas",
    "Distribuidores",
    "Transportadores",
    "Onde salvar e imprimir"
};

static const char *DESCRICOES[PG_N] = {
    "Esta senha protege a tela de Administração (dados do posto, produtos e "
    "faixas). Guarde-a: ela não é mostrada em lugar nenhum.",
    "Estes dados saem no cabeçalho de todo formulário. Podem ser alterados "
    "depois em Administração.",
    "Marque os produtos que o posto vende. As faixas são sugestões (gasolina "
    "E32, desde 01/08/2026; diesel B15) — confira com a legislação vigente e "
    "ajuste em Administração > Produtos se preciso.",
    "Quem faz a análise do combustível no recebimento. O nome sai no "
    "formulário. Digite o nome e clique em Adicionar.",
    "Empresas de quem o posto compra o combustível. A primeira da lista vem "
    "escolhida no formulário (dá para trocar em Administração).",
    "Empresas que fazem o transporte até o posto. A primeira da lista vem "
    "escolhida no formulário.",
    "Os formulários em PDF ficam nesta pasta. Na hora de imprimir, o programa "
    "usa a impressora escolhida abaixo."
};

/* ---------- criação de controles por página ---------- */

static HWND guarda(Assistente *a, int pg, HWND c)
{
    if (c && a->n_ctl[pg] < MAX_CTL_PAGINA) a->ctl[pg][a->n_ctl[pg]++] = c;
    return c;
}

/* Rótulos largos são as frases explicativas, que quebram em duas linhas. */
static HWND rotulo(Assistente *a, HWND h, int pg, const char *t,
                   int x, int y, int w)
{
    return guarda(a, pg, cria_rotulo(h, t, x, y + 4, w, w >= 560 ? 40 : 18,
                                     a->fonte));
}

static HWND edit(Assistente *a, HWND h, int pg, int x, int y, int w, int id,
                 int senha)
{
    return guarda(a, pg, cria_ctl(h, L"EDIT", "",
                                  WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL
                                  | (senha ? ES_PASSWORD : 0),
                                  x, y, w, 24, id, a->fonte));
}

static void numero_br(double v, int casas, char *dst, size_t n)
{
    char *p;
    snprintf(dst, n, "%.*f", casas, v);
    for (p = dst; *p; p++) if (*p == '.') *p = ',';
}

static void resumo_produto(const Produto *p, char *dst, size_t n)
{
    char a[32], b[32], c[32], d[32];
    numero_br(p->massa_min, 2, a, sizeof a);
    numero_br(p->massa_max, 2, b, sizeof b);
    if (p->usa_teor_gas) {
        numero_br(p->gas_min, 1, c, sizeof c); numero_br(p->gas_max, 1, d, sizeof d);
        snprintf(dst, n, "%s a %s kg/m³ · álcool %s a %s%%", a, b, c, d);
    } else if (p->usa_teor_aehc) {
        numero_br(p->aehc_min, 1, c, sizeof c); numero_br(p->aehc_max, 1, d, sizeof d);
        snprintf(dst, n, "%s a %s kg/m³ · teor alcoólico %s a %s%%", a, b, c, d);
    } else if (p->usa_teor_bio) {
        numero_br(p->bio_min, 1, c, sizeof c); numero_br(p->bio_max, 1, d, sizeof d);
        snprintf(dst, n, "%s a %s kg/m³ · biodiesel %s a %s%%", a, b, c, d);
    } else {
        snprintf(dst, n, "%s a %s kg/m³", a, b);
    }
}

static void monta_pagina_lista(Assistente *a, HWND h, int pg, int com_cnpj)
{
    PaginaLista *l = &a->lista[pg];
    int y = Y_CONTEUDO;
    int larg_nome = com_cnpj ? 300 : 460;

    rotulo(a, h, pg, com_cnpj ? "Nome / Razão social:" : "Nome:", X0, y, 200);
    if (com_cnpj) rotulo(a, h, pg, "CNPJ:", X0 + larg_nome + 10, y, 150);
    y += 24;
    l->nome = edit(a, h, pg, X0, y, larg_nome, ID_LISTA_NOME + pg, 0);
    if (com_cnpj)
        l->cnpj = edit(a, h, pg, X0 + larg_nome + 10, y, 150, ID_LISTA_CNPJ + pg, 0);
    guarda(a, pg, cria_botao(h, "Adicionar", LARG_CLIENTE - X0 - 110, y, 110, 26,
                             ID_LISTA_ADD + pg, a->fonte));
    y += 40;
    l->lista = guarda(a, pg, cria_ctl(h, L"LISTBOX", "",
                                      WS_BORDER | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY,
                                      X0, y, LARG_CLIENTE - 2 * X0 - 120, 240,
                                      ID_LISTA + pg, a->fonte));
    guarda(a, pg, cria_botao(h, "Remover", LARG_CLIENTE - X0 - 110, y, 110, 26,
                             ID_LISTA_DEL + pg, a->fonte));
    rotulo(a, h, pg, "Pode deixar em branco e cadastrar depois, no botão "
                     "\"Novo...\" do formulário.", X0, y + 246, 580);
}

static void recarrega_lista(Assistente *a, int pg)
{
    Store *s = a->store;
    HWND lb = a->lista[pg].lista;
    int i;
    wchar_t w[512];
    char linha[320];

    SendMessageW(lb, LB_RESETCONTENT, 0, 0);
    if (pg == PG_ANALISTAS) {
        for (i = 0; i < s->n_analistas; i++) {
            u8_para_w(s->analistas[i].nome, w, 512);
            SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)w);
        }
        return;
    }
    {
        const Empresa *e = pg == PG_DISTRIBUIDORES ? s->distribuidores : s->transportadores;
        int n = pg == PG_DISTRIBUIDORES ? s->n_distribuidores : s->n_transportadores;
        for (i = 0; i < n; i++) {
            snprintf(linha, sizeof linha, "%s   —   %s", e[i].nome,
                     str_empty(e[i].cnpj) ? "(sem CNPJ)" : e[i].cnpj);
            u8_para_w(linha, w, 512);
            SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)w);
        }
    }
}

static void monta(Assistente *a, HWND h)
{
    int y, i;
    char em_uso[256], doc[600];

    a->titulo    = cria_rotulo(h, "", X0, 16, LARG_CLIENTE - 2 * X0, 26, a->fonte_titulo);
    a->descricao = cria_rotulo(h, "", X0, 48, LARG_CLIENTE - 2 * X0, 58, a->fonte);

    /* --- senha --- */
    y = Y_CONTEUDO + 10;
    rotulo(a, h, PG_SENHA, "Bem-vindo! Antes de usar o programa, configure-o "
                           "para o seu posto. Leva poucos minutos.",
           X0, y, 590);
    y += 44;
    rotulo(a, h, PG_SENHA, "Nova senha:", X0, y, 150);
    a->senha1 = edit(a, h, PG_SENHA, X0 + 160, y, 240, ID_SENHA1, 1);
    y += 36;
    rotulo(a, h, PG_SENHA, "Repita a senha:", X0, y, 150);
    a->senha2 = edit(a, h, PG_SENHA, X0 + 160, y, 240, ID_SENHA2, 1);
    y += 40;
    rotulo(a, h, PG_SENHA, "Use pelo menos 4 caracteres.", X0 + 160, y, 400);

    /* --- posto --- */
    y = Y_CONTEUDO + 10;
    rotulo(a, h, PG_POSTO, "Razão social:", X0, y, 130);
    a->razao = edit(a, h, PG_POSTO, X0 + 140, y, 450, ID_RAZAO, 0);
    y += 36;
    rotulo(a, h, PG_POSTO, "CNPJ:", X0, y, 130);
    a->cnpj = edit(a, h, PG_POSTO, X0 + 140, y, 200, ID_CNPJ, 0);
    y += 36;
    rotulo(a, h, PG_POSTO, "Endereço:", X0, y, 130);
    a->endereco = edit(a, h, PG_POSTO, X0 + 140, y, 450, ID_ENDERECO, 0);
    y += 36;
    rotulo(a, h, PG_POSTO, "Bairro:", X0, y, 130);
    a->bairro = edit(a, h, PG_POSTO, X0 + 140, y, 260, ID_BAIRRO, 0);
    y += 36;
    rotulo(a, h, PG_POSTO, "Cidade/UF:", X0, y, 130);
    a->cidade = edit(a, h, PG_POSTO, X0 + 140, y, 260, ID_CIDADE, 0);
    y += 30;
    rotulo(a, h, PG_POSTO, "Exemplo: SÃO PAULO/SP", X0 + 140, y, 300);

    /* --- produtos --- */
    a->n_catalogo = store_catalogo_n();
    if (a->n_catalogo > MAX_PRODUTOS) a->n_catalogo = MAX_PRODUTOS;
    y = Y_CONTEUDO + 6;
    for (i = 0; i < a->n_catalogo; i++) {
        Produto p;
        char resumo[200];
        store_catalogo_produto(i, &p);
        a->produto[i] = guarda(a, PG_PRODUTOS,
                               cria_check(h, p.nome, X0, y + 2, 190, 22,
                                          ID_PRODUTO + i, a->fonte));
        resumo_produto(&p, resumo, sizeof resumo);
        rotulo(a, h, PG_PRODUTOS, resumo, X0 + 200, y, 400);
        y += 34;
    }
    rotulo(a, h, PG_PRODUTOS, "Outros produtos podem ser incluídos depois em "
                              "Administração > Produtos > Novo...",
           X0, y + 10, 590);

    /* --- listas --- */
    monta_pagina_lista(a, h, PG_ANALISTAS, 0);
    monta_pagina_lista(a, h, PG_DISTRIBUIDORES, 1);
    monta_pagina_lista(a, h, PG_TRANSPORTADORES, 1);

    /* --- pasta e impressora --- */
    y = Y_CONTEUDO + 10;
    rotulo(a, h, PG_SAIDA, "Pasta dos PDFs:", X0, y, 130);
    a->pasta = edit(a, h, PG_SAIDA, X0 + 140, y, 340, ID_PASTA, 0);
    guarda(a, PG_SAIDA, cria_botao(h, "Procurar...", X0 + 490, y - 1, 100, 26,
                                   ID_PROCURAR, a->fonte));
    pasta_saida_resolvida("", doc, sizeof doc);
    ctl_define_texto(a->pasta, doc);
    y += 44;

    rotulo(a, h, PG_SAIDA, "Impressora:", X0, y, 130);
    a->impressora = guarda(a, PG_SAIDA, cria_combo(h, X0 + 140, y, 450, 300,
                                                   ID_IMPRESSORA, a->fonte));
    a->n_impressoras = raq_lista_impressoras(a->impressoras, 32);
    if (raq_impressora_em_uso("", em_uso, sizeof em_uso)) {
        char rot[320];
        snprintf(rot, sizeof rot, "(usar a padrão do Windows — hoje: %s)", em_uso);
        combo_add(a->impressora, rot);
    } else {
        combo_add(a->impressora, "(usar a padrão do Windows — nenhuma encontrada)");
    }
    for (i = 0; i < a->n_impressoras; i++) combo_add(a->impressora, a->impressoras[i]);
    combo_define_sel(a->impressora, 0);
    y += 44;
    rotulo(a, h, PG_SAIDA, "Escolher uma impressora pelo nome evita que o "
                           "formulário saia em outra se alguém mudar a padrão "
                           "do Windows.", X0, y, 590);
    y += 50;
    rotulo(a, h, PG_SAIDA, "Clique em Concluir para gravar a configuração. "
                           "Tudo pode ser alterado depois no botão "
                           "Administração, com a senha criada no primeiro passo.",
           X0, y, 590);

    /* --- navegação --- */
    a->btn_voltar  = cria_botao(h, "< Voltar", LARG_CLIENTE - X0 - 330,
                                ALT_CLIENTE - 50, 100, 30, ID_VOLTAR, a->fonte);
    a->btn_avancar = cria_ctl(h, L"BUTTON", "Avançar >",
                              BS_DEFPUSHBUTTON | WS_TABSTOP,
                              LARG_CLIENTE - X0 - 220, ALT_CLIENTE - 50, 110, 30,
                              ID_AVANCAR, a->fonte);
    cria_botao(h, "Cancelar", LARG_CLIENTE - X0 - 100, ALT_CLIENTE - 50, 100, 30,
               ID_CANCELAR, a->fonte);
}

static void mostra_pagina(Assistente *a, int pg)
{
    int p, i;
    char t[160];

    a->pagina = pg;
    for (p = 0; p < PG_N; p++)
        for (i = 0; i < a->n_ctl[p]; i++)
            ShowWindow(a->ctl[p][i], p == pg ? SW_SHOW : SW_HIDE);

    snprintf(t, sizeof t, "Passo %d de %d — %s", pg + 1, PG_N, TITULOS[pg]);
    ctl_define_texto(a->titulo, t);
    ctl_define_texto(a->descricao, DESCRICOES[pg]);
    EnableWindow(a->btn_voltar, pg > 0);
    ctl_define_texto(a->btn_avancar, pg == PG_N - 1 ? "Concluir" : "Avançar >");

    switch (pg) {
    case PG_SENHA:  SetFocus(a->senha1); break;
    case PG_POSTO:  SetFocus(a->razao);  break;
    case PG_ANALISTAS: case PG_DISTRIBUIDORES: case PG_TRANSPORTADORES:
        SetFocus(a->lista[pg].nome); break;
    default: break;
    }
}

/* ---------- listas: adicionar e remover ---------- */

static void lista_adiciona(HWND h, Assistente *a, int pg)
{
    Store *s = a->store;
    PaginaLista *l = &a->lista[pg];
    char nome[MAX_NOME], cnpj[MAX_CNPJ] = "";
    int r;

    ctl_texto(l->nome, nome, sizeof nome);
    if (str_empty(nome)) { SetFocus(l->nome); return; }

    if (pg == PG_ANALISTAS) {
        r = store_add_analista(s, nome);
    } else {
        ctl_texto(l->cnpj, cnpj, sizeof cnpj);
        if (!str_empty(cnpj) && !cnpj_valido(cnpj)
            && !pergunta(h, TITULOS[pg],
                         "O CNPJ informado não passou na verificação do "
                         "dígito.\n\nAdicionar assim mesmo?")) {
            SetFocus(l->cnpj);
            return;
        }
        r = pg == PG_DISTRIBUIDORES
          ? store_add_empresa(s->distribuidores, &s->n_distribuidores, nome, cnpj)
          : store_add_empresa(s->transportadores, &s->n_transportadores, nome, cnpj);
    }
    if (r < 0) {
        aviso(h, TITULOS[pg], "Esse nome já está na lista.");
        return;
    }
    ctl_define_texto(l->nome, "");
    if (l->cnpj) ctl_define_texto(l->cnpj, "");
    recarrega_lista(a, pg);
    SetFocus(l->nome);
}

static void lista_remove(Assistente *a, int pg)
{
    Store *s = a->store;
    int i = (int)SendMessageW(a->lista[pg].lista, LB_GETCURSEL, 0, 0);
    if (i < 0) return;
    switch (pg) {
    case PG_ANALISTAS:
        store_remove(s->analistas, &s->n_analistas, sizeof(Analista), i); break;
    case PG_DISTRIBUIDORES:
        store_remove(s->distribuidores, &s->n_distribuidores, sizeof(Empresa), i); break;
    case PG_TRANSPORTADORES:
        store_remove(s->transportadores, &s->n_transportadores, sizeof(Empresa), i); break;
    }
    recarrega_lista(a, pg);
}

/* ---------- validação de cada página ---------- */

static int valida(HWND h, Assistente *a, int pg)
{
    switch (pg) {
    case PG_SENHA: {
        char s1[128], s2[128];
        ctl_texto(a->senha1, s1, sizeof s1);
        ctl_texto(a->senha2, s2, sizeof s2);
        if (strlen(s1) < 4) {
            aviso(h, TITULOS[pg], "A senha precisa ter pelo menos 4 caracteres.");
            SetFocus(a->senha1);
            return 0;
        }
        if (strcmp(s1, s2)) {
            aviso(h, TITULOS[pg], "As duas senhas não são iguais. Digite de novo.");
            ctl_define_texto(a->senha2, "");
            SetFocus(a->senha2);
            return 0;
        }
        return 1;
    }
    case PG_POSTO: {
        char t[MAX_NOME];
        ctl_texto(a->razao, t, sizeof t);
        if (str_empty(t)) {
            aviso(h, TITULOS[pg], "Informe a razão social do posto.");
            SetFocus(a->razao);
            return 0;
        }
        ctl_texto(a->cnpj, t, sizeof t);
        if (!cnpj_valido(t)
            && !pergunta(h, TITULOS[pg],
                         "O CNPJ do posto está vazio ou não passou na "
                         "verificação do dígito.\n\nContinuar assim mesmo?")) {
            SetFocus(a->cnpj);
            return 0;
        }
        return 1;
    }
    case PG_PRODUTOS: {
        int i;
        for (i = 0; i < a->n_catalogo; i++)
            if (SendMessageW(a->produto[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
                return 1;
        aviso(h, TITULOS[pg], "Marque ao menos um produto.");
        return 0;
    }
    case PG_ANALISTAS:
    case PG_DISTRIBUIDORES:
    case PG_TRANSPORTADORES: {
        /* Texto digitado e não adicionado é o esquecimento mais comum. */
        char t[MAX_NOME];
        ctl_texto(a->lista[pg].nome, t, sizeof t);
        if (!str_empty(t)) {
            lista_adiciona(h, a, pg);
            ctl_texto(a->lista[pg].nome, t, sizeof t);
            if (!str_empty(t)) return 0;    /* não entrou (repetido etc.) */
        }
        return 1;
    }
    default:
        return 1;
    }
}

/* ---------- gravação final ---------- */

static int conclui(HWND h, Assistente *a)
{
    Store *s = a->store;
    Config *c = &s->config;
    char senha[128], cnpj[MAX_CNPJ];
    int i, sel;

    ctl_texto(a->senha1, senha, sizeof senha);
    senha_gera_hash(senha, c->senha_hash, sizeof c->senha_hash);
    SecureZeroMemory(senha, sizeof senha);

    ctl_texto(a->razao,    c->razao_social,  sizeof c->razao_social);
    ctl_texto(a->endereco, c->endereco,      sizeof c->endereco);
    ctl_texto(a->bairro,   c->bairro,        sizeof c->bairro);
    ctl_texto(a->cidade,   c->cidade_estado, sizeof c->cidade_estado);
    ctl_texto(a->cnpj, cnpj, sizeof cnpj);
    cnpj_formata(cnpj, c->cnpj, sizeof c->cnpj);

    ctl_texto(a->pasta, c->pasta_saida, sizeof c->pasta_saida);
    if (garante_pasta(c->pasta_saida) != 0) {
        char m[800];
        snprintf(m, sizeof m, "Não foi possível criar a pasta:\n\n%s\n\n"
                              "Escolha outra pasta.", c->pasta_saida);
        erro(h, TITULOS[PG_SAIDA], m);
        return 0;
    }

    sel = combo_sel(a->impressora);
    if (sel > 0 && sel <= a->n_impressoras)
        str_copy(c->impressora, sizeof c->impressora, a->impressoras[sel - 1]);
    else
        c->impressora[0] = '\0';

    s->n_produtos = 0;
    for (i = 0; i < a->n_catalogo; i++) {
        if (SendMessageW(a->produto[i], BM_GETCHECK, 0, 0) != BST_CHECKED) continue;
        store_catalogo_produto(i, &s->produtos[s->n_produtos]);
        s->produtos[s->n_produtos].id = s->n_produtos + 1;
        s->n_produtos++;
    }

    /* O primeiro de cada lista vem escolhido no formulário. */
    if (s->n_distribuidores && store_indice_padrao(s->distribuidores, s->n_distribuidores) == 0)
        s->distribuidores[0].padrao = 1;
    if (s->n_transportadores && store_indice_padrao(s->transportadores, s->n_transportadores) == 0)
        s->transportadores[0].padrao = 1;

    if (store_salva_tudo(s) != 0) {
        char m[900];
        snprintf(m, sizeof m,
                 "Não foi possível gravar a configuração em:\n\n%s\n\n"
                 "Verifique se a pasta do programa permite gravação.", s->dir);
        erro(h, "Erro ao salvar", m);
        return 0;
    }
    return 1;
}

/* ---------- janela ---------- */

static int confirma_cancelar(HWND h)
{
    return pergunta(h, "Configuração inicial",
                    "Sair sem terminar a configuração?\n\n"
                    "Nada será gravado e o programa fecha. A configuração "
                    "recomeça na próxima vez que ele for aberto.");
}

static LRESULT CALLBACK setup_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    Assistente *a = (Assistente *)GetWindowLongPtrW(h, GWLP_USERDATA);

    switch (m) {
    case WM_CREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lp;
        a = (Assistente *)cs->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)a);
        monta(a, h);
        recarrega_lista(a, PG_ANALISTAS);
        recarrega_lista(a, PG_DISTRIBUIDORES);
        recarrega_lista(a, PG_TRANSPORTADORES);
        mostra_pagina(a, PG_SENHA);
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wp);
        HWND foco = GetFocus();

        if (id >= ID_LISTA_ADD && id < ID_LISTA_ADD + PG_N) {
            lista_adiciona(h, a, id - ID_LISTA_ADD); return 0;
        }
        if (id >= ID_LISTA_DEL && id < ID_LISTA_DEL + PG_N) {
            lista_remove(a, id - ID_LISTA_DEL); return 0;
        }

        switch (id) {
        case ID_AVANCAR: {
            int pg = a->pagina;
            /* Enter dentro dos campos de uma lista adiciona o item, em vez de
               pular de página com o nome ainda na caixa. */
            if ((pg == PG_ANALISTAS || pg == PG_DISTRIBUIDORES
                 || pg == PG_TRANSPORTADORES)
                && foco && (foco == a->lista[pg].nome || foco == a->lista[pg].cnpj)) {
                char t[MAX_NOME];
                ctl_texto(a->lista[pg].nome, t, sizeof t);
                if (!str_empty(t)) { lista_adiciona(h, a, pg); return 0; }
            }
            if (!valida(h, a, pg)) return 0;
            if (pg < PG_N - 1) { mostra_pagina(a, pg + 1); return 0; }
            if (conclui(h, a)) { a->concluido = 1; DestroyWindow(h); }
            return 0;
        }
        case ID_VOLTAR:
            if (a->pagina > 0) mostra_pagina(a, a->pagina - 1);
            return 0;
        case ID_CANCELAR:
            if (confirma_cancelar(h)) DestroyWindow(h);
            return 0;
        case ID_PROCURAR: {
            char p[600];
            ctl_texto(a->pasta, p, sizeof p);
            if (escolhe_pasta(h, "Pasta onde os formulários em PDF serão salvos:",
                              p, sizeof p))
                ctl_define_texto(a->pasta, p);
            return 0;
        }
        }
        break;
    }

    case WM_CLOSE:
        if (confirma_cancelar(h)) DestroyWindow(h);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int ui_setup_executa(HINSTANCE inst, Store *store)
{
    static Assistente a;   /* grande demais para a pilha */
    WNDCLASSW wc;
    RECT r;
    HWND h;
    int lt, at;

    memset(&a, 0, sizeof a);
    a.store = store;
    a.fonte = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                          0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                          L"Segoe UI");
    a.fonte_titulo = CreateFontW(-20, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                                 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                                 L"Segoe UI");

    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc   = setup_proc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"RaqAssistente";
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassW(&wc);

    r.left = 0; r.top = 0; r.right = LARG_CLIENTE; r.bottom = ALT_CLIENTE;
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    lt = r.right - r.left;
    at = r.bottom - r.top;

    h = CreateWindowExW(0, L"RaqAssistente", L"Configuração inicial — RAQ",
                        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                        (GetSystemMetrics(SM_CXSCREEN) - lt) / 2,
                        (GetSystemMetrics(SM_CYSCREEN) - at) / 2,
                        lt, at, NULL, NULL, inst, &a);
    if (h) roda_modal(h, NULL);

    DeleteObject(a.fonte);
    DeleteObject(a.fonte_titulo);
    return a.concluido;
}
