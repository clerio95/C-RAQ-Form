/* util.h — strings, codificação, datas e CNPJ. */
#ifndef RAQ_UTIL_H
#define RAQ_UTIL_H

#include <stddef.h>
#include <stdio.h>

/* Acesso a arquivo com caminho em UTF-8.

   No Windows é obrigatório passar por aqui: fopen/_mkdir são as versões ANSI
   e interpretam os bytes na página de código da máquina, não em UTF-8. Um
   caminho como C:\Users\João\... falha em todas elas, e o programa fica sem
   ler nem gravar nada. Estas funções convertem para UTF-16 e usam _wfopen /
   _wmkdir. Fora do Windows são fopen/mkdir diretos. */
FILE *arquivo_abre(const char *caminho_utf8, const char *modo);
int   arquivo_existe(const char *caminho_utf8);
int   pasta_cria(const char *caminho_utf8);   /* 0 = existe ou foi criada */
void  remove_arquivo(const char *caminho_utf8);

/* Cópia truncante sempre terminada em '\0'. */
void  str_copy(char *dst, size_t dstsz, const char *src);
void  str_trim(char *s);
int   str_empty(const char *s);

/* UTF-8 -> WinAnsi (CP1252). Caracteres fora da tabela viram '?'. */
void  utf8_to_winansi(const char *src, char *dst, size_t dstsz);

/* Data de hoje em dd/mm/aaaa. */
void  data_hoje(char *dst, size_t dstsz);
/* Carimbo aaaa-mm-dd para nome de arquivo. */
void  data_hoje_iso(char *dst, size_t dstsz);
/* Carimbo aaaa-mm-dd hh:mm:ss para o histórico. */
void  agora_iso(char *dst, size_t dstsz);
int   data_valida(const char *ddmmaaaa);

/* Mantém só dígitos. */
void  so_digitos(const char *src, char *dst, size_t dstsz);
/* Formata 14 dígitos como 00.000.000/0000-00. */
void  cnpj_formata(const char *src, char *dst, size_t dstsz);
/* 1 se os dígitos verificadores conferem. */
int   cnpj_valido(const char *src);

/* Converte "1.234,56" ou "1234.56" em double. 0 se não for número. */
int   num_parse(const char *s, double *out);

/* Senha do administrador.

   O hash novo é PBKDF2-HMAC-SHA256 com sal aleatório, gravado como
   "pbkdf2-sha256$<iterações>$<sal hex>$<hash hex>". Versões anteriores
   gravavam um FNV-1a sem sal (16 dígitos hex); senha_confere ainda aceita
   esse formato para que a atualização não tranque ninguém do lado de fora, e
   senha_hash_legado avisa que vale regravar no formato novo. */
#define SENHA_HASH_MAX 160
void  senha_gera_hash(const char *senha, char *dst, size_t dstsz);
int   senha_confere(const char *senha, const char *hash);
int   senha_hash_legado(const char *hash);

/* Bytes aleatórios do sistema. Devolve 0 em caso de sucesso. */
int   bytes_aleatorios(unsigned char *dst, size_t n);

#endif /* RAQ_UTIL_H */
