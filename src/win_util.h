/* win_util.h — ponte entre o modelo (UTF-8) e a API do Windows (UTF-16).

   O programa inteiro guarda texto em UTF-8; a conversão acontece só aqui, na
   fronteira com o Win32. Usamos as APIs -W para que acentos em nomes de
   motoristas e razões sociais nunca dependam da página de código da máquina. */
#ifndef RAQ_WIN_UTIL_H
#define RAQ_WIN_UTIL_H

#include <windows.h>

/* Conversões. Os buffers de saída são sempre terminados. */
void  u8_para_w(const char *utf8, wchar_t *dst, int dst_chars);
void  w_para_u8(const wchar_t *w, char *dst, int dst_bytes);

/* Texto de um controle, já em UTF-8. */
void  ctl_texto(HWND ctl, char *dst, int dst_bytes);
void  ctl_define_texto(HWND ctl, const char *utf8);

/* Criação de controles com texto em UTF-8. */
HWND  cria_ctl(HWND pai, const wchar_t *classe, const char *texto,
               DWORD estilo, int x, int y, int w, int h, int id, HFONT fonte);
HWND  cria_rotulo(HWND pai, const char *texto, int x, int y, int w, int h,
                  HFONT fonte);
HWND  cria_edit(HWND pai, int x, int y, int w, int h, int id, HFONT fonte);
HWND  cria_combo(HWND pai, int x, int y, int w, int h_aberto, int id, HFONT fonte);
HWND  cria_botao(HWND pai, const char *texto, int x, int y, int w, int h,
                 int id, HFONT fonte);
HWND  cria_check(HWND pai, const char *texto, int x, int y, int w, int h,
                 int id, HFONT fonte);
HWND  cria_grupo(HWND pai, const char *texto, int x, int y, int w, int h,
                 HFONT fonte);

/* Combos com texto UTF-8. */
void  combo_limpa(HWND combo);
void  combo_add(HWND combo, const char *utf8);
int   combo_sel(HWND combo);
void  combo_define_sel(HWND combo, int i);
/* Seleciona o item com este texto; se não existir, acrescenta e seleciona.
   Necessário porque os combos são só-lista: SetWindowText num deles não
   mostra um valor que não esteja na lista (ex.: cor padrão personalizada). */
void  combo_seleciona_texto(HWND combo, const char *utf8);

/* Caixas de diálogo. */
void  aviso(HWND pai, const char *titulo, const char *msg);
void  erro(HWND pai, const char *titulo, const char *msg);
int   pergunta(HWND pai, const char *titulo, const char *msg);   /* 1 = Sim */
/* Sim / Não / Cancelar: devolve 1, 0 ou -1. */
int   pergunta3(HWND pai, const char *titulo, const char *msg);

/* Pede uma linha de texto ao usuário. Devolve 1 se confirmou. */
int   pede_texto(HWND pai, const char *titulo, const char *rotulo,
                 char *valor, int valor_bytes, int senha);

/* Roda um laço de mensagens próprio até `janela` ser destruída, mantendo
   `pai` desabilitado. É assim que as janelas modais do programa funcionam —
   nenhuma delas pode chamar PostQuitMessage, que encerraria o laço principal. */
void  roda_modal(HWND janela, HWND pai);

/* Pasta do executável, em UTF-8, sem barra final. */
void  pasta_do_exe(char *dst, int dst_bytes);

/* Cria a pasta (e as intermediárias) se não existir. 0 = existe/criada. */
int   garante_pasta(const char *caminho_utf8);

/* Pasta "Documentos" do usuário, em UTF-8. Devolve 0 se não achou. */
int   pasta_documentos(char *dst, int dst_bytes);

/* Pasta onde os PDFs são gravados: vazia = Documentos\Formularios RAQ;
   caminho relativo = relativo à pasta do executável; absoluto = como está. */
void  pasta_saida_resolvida(const char *configurada, char *dst, int dst_bytes);

/* Caixa "Procurar pasta". `dst` traz a pasta inicial e recebe a escolhida.
   Devolve 1 se o usuário escolheu. */
int   escolhe_pasta(HWND pai, const char *titulo, char *dst, int dst_bytes);

/* Abre a pasta no Explorador de Arquivos. */
void  abre_pasta(const char *caminho_utf8);

#endif /* RAQ_WIN_UTIL_H */
