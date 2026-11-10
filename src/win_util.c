#include "win_util.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <shlobj.h>
#include <shellapi.h>

void u8_para_w(const char *utf8, wchar_t *dst, int dst_chars)
{
    if (!dst || dst_chars <= 0) return;
    dst[0] = L'\0';
    if (!utf8) return;
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, dst, dst_chars) == 0)
        dst[dst_chars - 1] = L'\0';
}

void w_para_u8(const wchar_t *w, char *dst, int dst_bytes)
{
    if (!dst || dst_bytes <= 0) return;
    dst[0] = '\0';
    if (!w) return;
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1, dst, dst_bytes, NULL, NULL) == 0)
        dst[dst_bytes - 1] = '\0';
}

void ctl_texto(HWND ctl, char *dst, int dst_bytes)
{
    wchar_t buf[1024];
    int n;
    if (!dst || dst_bytes <= 0) return;
    dst[0] = '\0';
    if (!ctl) return;
    n = GetWindowTextW(ctl, buf, (int)(sizeof buf / sizeof buf[0]));
    buf[n < 0 ? 0 : n] = L'\0';
    w_para_u8(buf, dst, dst_bytes);
    str_trim(dst);
}

void ctl_define_texto(HWND ctl, const char *utf8)
{
    wchar_t buf[1024];
    if (!ctl) return;
    u8_para_w(utf8 ? utf8 : "", buf, (int)(sizeof buf / sizeof buf[0]));
    SetWindowTextW(ctl, buf);
}

HWND cria_ctl(HWND pai, const wchar_t *classe, const char *texto, DWORD estilo,
              int x, int y, int w, int h, int id, HFONT fonte)
{
    wchar_t t[512];
    HWND c;
    u8_para_w(texto ? texto : "", t, (int)(sizeof t / sizeof t[0]));
    c = CreateWindowExW(0, classe, t, estilo | WS_CHILD | WS_VISIBLE,
                        x, y, w, h, pai, (HMENU)(INT_PTR)id,
                        (HINSTANCE)GetWindowLongPtrW(pai, GWLP_HINSTANCE), NULL);
    if (c && fonte) SendMessageW(c, WM_SETFONT, (WPARAM)fonte, TRUE);
    return c;
}

HWND cria_rotulo(HWND pai, const char *texto, int x, int y, int w, int h,
                 HFONT fonte)
{
    return cria_ctl(pai, L"STATIC", texto, SS_LEFT, x, y, w, h, -1, fonte);
}

HWND cria_edit(HWND pai, int x, int y, int w, int h, int id, HFONT fonte)
{
    return cria_ctl(pai, L"EDIT", "", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                    x, y, w, h, id, fonte);
}

HWND cria_combo(HWND pai, int x, int y, int w, int h_aberto, int id, HFONT fonte)
{
    return cria_ctl(pai, L"COMBOBOX", "",
                    CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
                    x, y, w, h_aberto, id, fonte);
}

HWND cria_botao(HWND pai, const char *texto, int x, int y, int w, int h,
                int id, HFONT fonte)
{
    return cria_ctl(pai, L"BUTTON", texto, BS_PUSHBUTTON | WS_TABSTOP,
                    x, y, w, h, id, fonte);
}

HWND cria_check(HWND pai, const char *texto, int x, int y, int w, int h,
                int id, HFONT fonte)
{
    return cria_ctl(pai, L"BUTTON", texto, BS_AUTOCHECKBOX | WS_TABSTOP,
                    x, y, w, h, id, fonte);
}

HWND cria_grupo(HWND pai, const char *texto, int x, int y, int w, int h,
                HFONT fonte)
{
    return cria_ctl(pai, L"BUTTON", texto, BS_GROUPBOX, x, y, w, h, -1, fonte);
}

void combo_limpa(HWND combo)
{
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
}

void combo_add(HWND combo, const char *utf8)
{
    wchar_t t[512];
    u8_para_w(utf8, t, (int)(sizeof t / sizeof t[0]));
    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)t);
}

int combo_sel(HWND combo)
{
    return (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
}

void combo_define_sel(HWND combo, int i)
{
    SendMessageW(combo, CB_SETCURSEL, (WPARAM)i, 0);
}

void combo_seleciona_texto(HWND combo, const char *utf8)
{
    wchar_t t[512];
    LRESULT i;
    u8_para_w(utf8 ? utf8 : "", t, (int)(sizeof t / sizeof t[0]));
    if (!t[0]) { SendMessageW(combo, CB_SETCURSEL, (WPARAM)-1, 0); return; }
    i = SendMessageW(combo, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)t);
    if (i == CB_ERR) i = SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)t);
    SendMessageW(combo, CB_SETCURSEL, (WPARAM)i, 0);
}

static int caixa(HWND pai, const char *titulo, const char *msg, UINT flags)
{
    wchar_t t[256], m[2048];
    u8_para_w(titulo, t, (int)(sizeof t / sizeof t[0]));
    u8_para_w(msg,    m, (int)(sizeof m / sizeof m[0]));
    return MessageBoxW(pai, m, t, flags);
}

void aviso(HWND pai, const char *titulo, const char *msg)
{
    caixa(pai, titulo, msg, MB_OK | MB_ICONWARNING);
}

void erro(HWND pai, const char *titulo, const char *msg)
{
    caixa(pai, titulo, msg, MB_OK | MB_ICONERROR);
}

int pergunta(HWND pai, const char *titulo, const char *msg)
{
    return caixa(pai, titulo, msg, MB_YESNO | MB_ICONQUESTION) == IDYES;
}

int pergunta3(HWND pai, const char *titulo, const char *msg)
{
    int r = caixa(pai, titulo, msg, MB_YESNOCANCEL | MB_ICONQUESTION);
    return r == IDYES ? 1 : r == IDNO ? 0 : -1;
}

/* ---------- caixa de entrada de uma linha ---------- */

typedef struct {
    const char *rotulo;
    char       *valor;
    int         valor_bytes;
    int         senha;
    int         confirmou;
    HWND        edit;
} PedeTexto;

#define ID_PT_EDIT 2001

static LRESULT CALLBACK pede_texto_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    PedeTexto *pt = (PedeTexto *)GetWindowLongPtrW(h, GWLP_USERDATA);

    switch (m) {
    case WM_CREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lp;
        HFONT fonte = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        pt = (PedeTexto *)cs->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)pt);

        cria_rotulo(h, pt->rotulo, 12, 12, 380, 18, fonte);
        pt->edit = cria_ctl(h, L"EDIT", "",
                            WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL
                            | (pt->senha ? ES_PASSWORD : 0),
                            12, 34, 380, 24, ID_PT_EDIT, fonte);
        cria_ctl(h, L"BUTTON", "OK", BS_DEFPUSHBUTTON | WS_TABSTOP,
                 212, 70, 85, 26, IDOK, fonte);
        cria_botao(h, "Cancelar", 307, 70, 85, 26, IDCANCEL, fonte);
        SetFocus(pt->edit);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            ctl_texto(pt->edit, pt->valor, pt->valor_bytes);
            pt->confirmou = 1;
            DestroyWindow(h);
            return 0;
        }
        if (LOWORD(wp) == IDCANCEL) { DestroyWindow(h); return 0; }
        break;
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int pede_texto(HWND pai, const char *titulo, const char *rotulo,
               char *valor, int valor_bytes, int senha)
{
    static int registrado = 0;
    HINSTANCE inst = GetModuleHandleW(NULL);
    PedeTexto pt;
    wchar_t wt[256];
    HWND h;
    char inicial[512];

    if (!registrado) {
        WNDCLASSW wc;
        memset(&wc, 0, sizeof wc);
        wc.lpfnWndProc   = pede_texto_proc;
        wc.hInstance     = inst;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"RaqPedeTexto";
        RegisterClassW(&wc);
        registrado = 1;
    }

    str_copy(inicial, sizeof inicial, valor);
    pt.rotulo = rotulo;
    pt.valor = valor;
    pt.valor_bytes = valor_bytes;
    pt.senha = senha;
    pt.confirmou = 0;
    pt.edit = NULL;
    valor[0] = '\0';

    u8_para_w(titulo, wt, (int)(sizeof wt / sizeof wt[0]));
    h = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RaqPedeTexto", wt,
                        WS_POPUPWINDOW | WS_CAPTION,
                        CW_USEDEFAULT, CW_USEDEFAULT, 420, 145,
                        pai, NULL, inst, &pt);
    if (!h) return 0;

    ctl_define_texto(pt.edit, inicial);
    SendMessageW(pt.edit, EM_SETSEL, 0, -1);

    roda_modal(h, pai);
    return pt.confirmou && !str_empty(valor);
}

void roda_modal(HWND janela, HWND pai)
{
    MSG msg;

    if (!janela) return;

    /* Centraliza sobre a janela pai antes de aparecer. */
    if (pai) {
        RECT rp, rf;
        GetWindowRect(pai, &rp);
        GetWindowRect(janela, &rf);
        SetWindowPos(janela, NULL,
                     rp.left + ((rp.right - rp.left) - (rf.right - rf.left)) / 2,
                     rp.top  + ((rp.bottom - rp.top) - (rf.bottom - rf.top)) / 2,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);
        EnableWindow(pai, FALSE);
    }
    ShowWindow(janela, SW_SHOW);
    UpdateWindow(janela);

    /* Sem PostQuitMessage: o laço termina quando a janela some. */
    while (IsWindow(janela)) {
        BOOL r = GetMessageW(&msg, NULL, 0, 0);
        if (r <= 0) {
            /* WM_QUIT (ex.: Windows desligando) pertence ao laço principal:
               devolve para a fila em vez de engolir. */
            if (r == 0) PostQuitMessage((int)msg.wParam);
            break;
        }
        if (!IsDialogMessageW(janela, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (pai) {
        EnableWindow(pai, TRUE);
        SetActiveWindow(pai);
    }
}

void pasta_do_exe(char *dst, int dst_bytes)
{
    wchar_t p[MAX_PATH];
    DWORD n = GetModuleFileNameW(NULL, p, MAX_PATH);
    wchar_t *barra;

    if (n == 0) { str_copy(dst, (size_t)dst_bytes, "."); return; }
    p[n] = L'\0';
    barra = wcsrchr(p, L'\\');
    if (barra) *barra = L'\0';
    w_para_u8(p, dst, dst_bytes);
}

int garante_pasta(const char *caminho_utf8)
{
    wchar_t w[MAX_PATH];
    int r;
    u8_para_w(caminho_utf8, w, MAX_PATH);
    if (GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES) return 0;
    r = SHCreateDirectoryExW(NULL, w, NULL);
    return (r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS
            || r == ERROR_FILE_EXISTS) ? 0 : -1;
}

int pasta_documentos(char *dst, int dst_bytes)
{
    wchar_t w[MAX_PATH];
    dst[0] = '\0';
    if (SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, w) != S_OK)
        return 0;
    w_para_u8(w, dst, dst_bytes);
    return dst[0] != '\0';
}

void pasta_saida_resolvida(const char *configurada, char *dst, int dst_bytes)
{
    char base[600];

    if (!configurada || str_empty(configurada)) {
        if (pasta_documentos(base, sizeof base)) {
            snprintf(dst, (size_t)dst_bytes, "%s\\Formularios RAQ", base);
            return;
        }
        configurada = "Formularios";
    }

    /* Caminho relativo é resolvido a partir da pasta do executável. */
    if (!strchr(configurada, ':') && configurada[0] != '\\' && configurada[0] != '/') {
        pasta_do_exe(base, sizeof base);
        snprintf(dst, (size_t)dst_bytes, "%s\\%s", base, configurada);
        return;
    }
    str_copy(dst, (size_t)dst_bytes, configurada);
}

static int CALLBACK escolhe_pasta_cb(HWND h, UINT m, LPARAM lp, LPARAM dados)
{
    (void)lp;
    if (m == BFFM_INITIALIZED && dados)
        SendMessageW(h, BFFM_SETSELECTIONW, TRUE, dados);
    return 0;
}

int escolhe_pasta(HWND pai, const char *titulo, char *dst, int dst_bytes)
{
    BROWSEINFOW bi;
    PIDLIST_ABSOLUTE pidl;
    wchar_t t[256], inicial[MAX_PATH], escolhida[MAX_PATH];
    int ok = 0;

    u8_para_w(titulo, t, (int)(sizeof t / sizeof t[0]));
    u8_para_w(dst, inicial, MAX_PATH);

    memset(&bi, 0, sizeof bi);
    bi.hwndOwner = pai;
    bi.lpszTitle = t;
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn      = escolhe_pasta_cb;
    bi.lParam    = (LPARAM)(inicial[0] ? inicial : NULL);

    pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        if (SHGetPathFromIDListW(pidl, escolhida)) {
            w_para_u8(escolhida, dst, dst_bytes);
            ok = 1;
        }
        CoTaskMemFree(pidl);
    }
    return ok;
}

void abre_pasta(const char *caminho_utf8)
{
    wchar_t w[MAX_PATH];
    u8_para_w(caminho_utf8, w, MAX_PATH);
    ShellExecuteW(NULL, L"open", w, NULL, NULL, SW_SHOWNORMAL);
}
