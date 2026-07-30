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

    /* Confere de verdade que dá para gravar, em vez de descobrir isso só
       quando o operador perder o primeiro cadastro. Falha aqui quase sempre
       é pasta somente leitura: RAQ.exe dentro de "Arquivos de Programas",
       numa pasta de rede sem permissão, ou ainda dentro do .zip baixado. */
    {
        char teste[700];
        FILE *f;
        snprintf(teste, sizeof teste, "%s\\gravacao.tmp", dados);
        f = arquivo_abre(teste, "wb");
        if (!f) {
            char m[1200];
            snprintf(m, sizeof m,
                     "O programa não consegue gravar em:\n\n%s\n\n"
                     "Nada que você cadastrar será salvo.\n\n"
                     "Mova a pasta do RAQ para um lugar com permissão de "
                     "escrita — o Desktop ou C:\\RAQ resolvem. Se o programa "
                     "veio num .zip, extraia a pasta antes de abrir.",
                     dados);
            erro(NULL, "Sem permissão para gravar", m);
        } else {
            fclose(f);
            remove_arquivo(teste);
        }
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
