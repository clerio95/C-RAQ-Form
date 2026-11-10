/* model.h — estruturas de dados do formulário RAQ e dos cadastros. */
#ifndef RAQ_MODEL_H
#define RAQ_MODEL_H

#include <stddef.h>

#define MAX_PRODUTOS        12
#define MAX_CADASTROS      256
#define MAX_NOME           128
#define MAX_CNPJ            24
#define MAX_CAMPO           64

/* ---------- cadastros ---------- */

typedef struct {
    int  id;
    char nome[MAX_NOME];
    char cnpj[MAX_CNPJ];
    int  padrao;                 /* 1 = pré-selecionado no formulário */
} Empresa;                       /* distribuidor ou transportador */

typedef struct {
    int  id;
    char placa[MAX_CAMPO];
} Placa;

typedef struct {
    int  id;
    char nome[MAX_NOME];
    char rg[MAX_CAMPO];
} Motorista;

typedef struct {
    int  id;
    char nome[MAX_NOME];
} Analista;

typedef struct {
    int   id;
    char  nome[MAX_NOME];
    char  aspecto_padrao[MAX_CAMPO];   /* "L.I.I." */
    char  cor_padrao[MAX_CAMPO];       /* "AMARELADA" / "ESVERDEADA" */
    double massa_min, massa_max;
    int   usa_teor_gas;
    double gas_min, gas_max;
    int   usa_teor_aehc;
    double aehc_min, aehc_max;
    int   usa_teor_bio;
    double bio_min, bio_max;
    int   ativo;
} Produto;

/* ---------- configuração do posto ---------- */

typedef struct {
    char razao_social[MAX_NOME];
    char cnpj[MAX_CNPJ];
    char endereco[MAX_NOME];
    char bairro[MAX_CAMPO];
    char cidade_estado[MAX_CAMPO];
    char pasta_saida[512];        /* vazio = Documentos\\Formularios RAQ */
    char senha_hash[160];         /* vazio = senha ainda não criada */
    /* Vazio = usa a impressora padrão do Windows. Preenchido = fixa esta,
       para o formulário não seguir o padrão do Windows quando alguém troca
       (o "Microsoft Print to PDF" costuma virar padrão sozinho). */
    char impressora[256];
} Config;

/* ---------- catálogo completo carregado do disco ---------- */

typedef struct {
    Config    config;
    Produto   produtos[MAX_PRODUTOS];       int n_produtos;
    Empresa   distribuidores[MAX_CADASTROS];int n_distribuidores;
    Empresa   transportadores[MAX_CADASTROS];int n_transportadores;
    Placa     placas[MAX_CADASTROS];        int n_placas;
    Motorista motoristas[MAX_CADASTROS];    int n_motoristas;
    Analista  analistas[MAX_CADASTROS];     int n_analistas;
    char      dir[512];                     /* pasta "dados/" */
} Store;

/* ---------- formulário preenchido ---------- */

/* Uma coluna do formulário: um produto recebido. */
typedef struct {
    const Produto *produto;
    char volume[MAX_CAMPO];      /* litros, como digitado */
    char aspecto[MAX_CAMPO];
    char cor[MAX_CAMPO];
    char massa[MAX_CAMPO];
    char teor_gas[MAX_CAMPO];
    char teor_aehc[MAX_CAMPO];
    char teor_bio[MAX_CAMPO];
} ColunaProduto;

typedef struct {
    const Config *config;

    ColunaProduto colunas[MAX_PRODUTOS];
    int  n_colunas;

    /* linhas mescladas (valor único para todas as colunas) */
    char data_coleta[MAX_CAMPO];
    char distribuidor[MAX_NOME];
    char distribuidor_cnpj[MAX_CNPJ];
    char transportador[MAX_NOME];
    char transportador_cnpj[MAX_CNPJ];
    char nota_fiscal[MAX_CAMPO];
    char placa[MAX_CAMPO];
    char motorista[MAX_NOME];
    char motorista_rg[MAX_CAMPO];
    char analista[MAX_NOME];

    char responsavel[MAX_NOME];
} Formulario;

/* Texto a imprimir numa célula de análise; devolve "-" quando não se aplica. */
const char *coluna_valor_analise(const ColunaProduto *c, int linha);

/* Índices das linhas de análise. */
enum {
    ANALISE_ASPECTO = 0,
    ANALISE_COR,
    ANALISE_MASSA,
    ANALISE_TEOR_GAS,
    ANALISE_TEOR_AEHC,
    ANALISE_TEOR_BIO,
    ANALISE_N
};

#endif /* RAQ_MODEL_H */
