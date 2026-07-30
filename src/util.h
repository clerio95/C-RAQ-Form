/* util.h — strings, codificação, datas e CNPJ. */
#ifndef RAQ_UTIL_H
#define RAQ_UTIL_H

#include <stddef.h>

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

/* Hash FNV-1a em hexadecimal — suficiente para a senha do admin,
   que só protege contra o usuário curioso, não contra ataque real. */
void  senha_hash(const char *senha, char *dst, size_t dstsz);

#endif /* RAQ_UTIL_H */
