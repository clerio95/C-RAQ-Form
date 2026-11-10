/* store.h — leitura e gravação dos cadastros em CSV (separador ';', UTF-8).

   Tudo mora numa pasta "dados/" ao lado do executável. São arquivos de texto
   de propósito: o admin consegue abrir no Excel, corrigir e fazer backup
   copiando a pasta. */
#ifndef RAQ_STORE_H
#define RAQ_STORE_H

#include "model.h"

/* Carrega tudo de `dir`, criando os arquivos com valores padrão na primeira
   execução. Devolve 0 em caso de sucesso. */
int store_carrega(Store *s, const char *dir);

/* Cada função grava um arquivo. Devolvem 0 em caso de sucesso. */
int store_salva_config(const Store *s);
int store_salva_produtos(const Store *s);
int store_salva_distribuidores(const Store *s);
int store_salva_transportadores(const Store *s);
int store_salva_placas(const Store *s);
int store_salva_motoristas(const Store *s);
int store_salva_analistas(const Store *s);

/* Acrescenta uma linha ao historico.csv. */
int store_registra_historico(const Store *s, const Formulario *f,
                             const char *arquivo_pdf);

/* Inclusões — devolvem o índice do novo registro ou -1 se a lista estiver
   cheia ou o nome já existir. */
int store_add_empresa(Empresa *lista, int *n, const char *nome, const char *cnpj);
int store_add_placa(Store *s, const char *placa);
int store_add_motorista(Store *s, const char *nome, const char *rg);
int store_add_analista(Store *s, const char *nome);

/* Remoções por índice; deslocam o restante da lista. */
void store_remove(void *lista, int *n, size_t tam_item, int indice);

/* 1 se a senha do administrador já foi criada (primeira execução feita). */
int store_configurado(const Store *s);

/* Próximo id livre na lista de produtos. */
int store_proximo_id_produto(const Store *s);

/* Grava todos os arquivos. Devolve 0 se todos gravaram. */
int store_salva_tudo(const Store *s);

/* Catálogo de produtos comuns, com faixas sugeridas, oferecido na primeira
   execução. */
int  store_catalogo_n(void);
void store_catalogo_produto(int i, Produto *dst);

/* Índice do registro marcado como padrão, ou 0 se nenhum. */
int store_indice_padrao(const Empresa *lista, int n);

#endif /* RAQ_STORE_H */
