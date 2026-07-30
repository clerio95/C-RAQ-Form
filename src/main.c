/* main.c — ponto de entrada.

   Carrega os cadastros da pasta "dados/" ao lado do executável (criando-a com
   valores padrão na primeira execução), abre a janela do formulário e roda o
   laço de mensagens. */
#include "store.h"
#include "ui_form.h"
#include "util.h"
#include "win_util.h"

#include <stdio.h>
#include <string.h>

#include <commctrl.h>

static Store g_store;

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE anterior, PWSTR cmdline, int mostrar)
{
    INITCOMMONCONTROLSEX icc;
    char exe[512], dados[600];
    HWND janela;
    MSG msg;

    (void)anterior; (void)cmdline; (void)mostrar;

    icc.dwSize = sizeof icc;
    icc.dwICC  = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    pasta_do_exe(exe, sizeof exe);
    snprintf(dados, sizeof dados, "%s\\dados", exe);

    if (store_carrega(&g_store, dados) != 0) {
        erro(NULL, "Erro",
             "Não foi possível ler a pasta de dados ao lado do programa.");
        return 1;
    }

    janela = ui_form_cria(inst, &g_store);
    if (!janela) {
        erro(NULL, "Erro", "Não foi possível criar a janela do programa.");
        return 1;
    }

    /* Se o posto ainda não foi configurado, o admin precisa saber disso. */
    if (str_empty(g_store.config.razao_social))
        aviso(janela, "Primeira execução",
              "Os dados do posto ainda não foram configurados.\n\n"
              "Clique em \"Administração\" (senha inicial: admin) e preencha "
              "a razão social, o CNPJ e o endereço.");

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(janela, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}
