/* main.c — ponto de entrada.

   Carrega os cadastros da pasta "dados/" ao lado do executável. Na primeira
   execução (sem senha de administrador e sem dados do posto) roda o
   assistente de configuração antes; depois abre a janela do formulário e roda
   o laço de mensagens. */
#include "store.h"
#include "ui_form.h"
#include "ui_setup.h"
#include "util.h"
#include "win_util.h"

#include <stdio.h>
#include <string.h>

#include <commctrl.h>
#include <objbase.h>

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

    /* A caixa "Procurar pasta" (SHBrowseForFolder) exige COM iniciado. */
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

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
                     "Reinstale com o comando de instalação (ele usa uma "
                     "pasta do seu usuário) ou mova a pasta do RAQ para um "
                     "lugar com permissão de escrita, como C:\\RAQ. Se o "
                     "programa veio num .zip, extraia a pasta antes de abrir.",
                     dados);
            erro(NULL, "Sem permissão para gravar", m);
        } else {
            fclose(f);
            remove_arquivo(teste);
        }
    }

    /* Instalação nova: nada de senha padrão nem de posto em branco — o
       assistente cria a senha e coleta os dados antes do primeiro uso. Quem
       atualiza de uma versão anterior já tem posto configurado e não passa
       por aqui. */
    if (!store_configurado(&g_store) && str_empty(g_store.config.razao_social)) {
        if (!ui_setup_executa(inst, &g_store)) return 0;
    }

    janela = ui_form_cria(inst, &g_store);
    if (!janela) {
        erro(NULL, "Erro", "Não foi possível criar a janela do programa.");
        return 1;
    }

    /* Senha existe mas o posto ficou em branco (ex.: config.ini editado). */
    if (str_empty(g_store.config.razao_social))
        aviso(janela, "Dados do posto",
              "Os dados do posto ainda não foram configurados.\n\n"
              "Clique em \"Administração\" e preencha a razão social, o "
              "CNPJ e o endereço.");

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(janela, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    CoUninitialize();
    return (int)msg.wParam;
}
