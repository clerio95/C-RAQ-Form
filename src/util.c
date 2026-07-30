#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#  include <wchar.h>
#else
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <errno.h>
#endif

#ifdef _WIN32

/* Converte UTF-8 -> UTF-16 para entregar às APIs -W. */
static int para_wide(const char *utf8, wchar_t *dst, int dst_chars)
{
    if (!utf8 || !dst || dst_chars <= 0) return 0;
    return MultiByteToWideChar(CP_UTF8, 0, utf8, -1, dst, dst_chars) != 0;
}

FILE *arquivo_abre(const char *caminho_utf8, const char *modo)
{
    wchar_t wc[1024], wm[8];
    if (!para_wide(caminho_utf8, wc, 1024)) return NULL;
    if (!para_wide(modo, wm, 8)) return NULL;
    return _wfopen(wc, wm);
}

int arquivo_existe(const char *caminho_utf8)
{
    wchar_t wc[1024];
    if (!para_wide(caminho_utf8, wc, 1024)) return 0;
    return GetFileAttributesW(wc) != INVALID_FILE_ATTRIBUTES;
}

int pasta_cria(const char *caminho_utf8)
{
    wchar_t wc[1024];
    if (!para_wide(caminho_utf8, wc, 1024)) return -1;
    if (CreateDirectoryW(wc, NULL)) return 0;
    return (GetLastError() == ERROR_ALREADY_EXISTS) ? 0 : -1;
}

void remove_arquivo(const char *caminho_utf8)
{
    wchar_t wc[1024];
    if (para_wide(caminho_utf8, wc, 1024)) DeleteFileW(wc);
}

#else

FILE *arquivo_abre(const char *caminho_utf8, const char *modo)
{
    return fopen(caminho_utf8, modo);
}

int arquivo_existe(const char *caminho_utf8)
{
    FILE *f = fopen(caminho_utf8, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int pasta_cria(const char *caminho_utf8)
{
    if (mkdir(caminho_utf8, 0775) == 0) return 0;
    return arquivo_existe(caminho_utf8) || errno == EEXIST ? 0 : -1;
}

void remove_arquivo(const char *caminho_utf8)
{
    remove(caminho_utf8);
}

#endif

void str_copy(char *dst, size_t dstsz, const char *src)
{
    size_t n;
    if (!dst || dstsz == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    n = strlen(src);
    if (n >= dstsz) n = dstsz - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void str_trim(char *s)
{
    char *ini = s, *fim;
    if (!s || !*s) return;
    while (*ini && isspace((unsigned char)*ini)) ini++;
    fim = ini + strlen(ini);
    while (fim > ini && isspace((unsigned char)fim[-1])) fim--;
    memmove(s, ini, (size_t)(fim - ini));
    s[fim - ini] = '\0';
}

int str_empty(const char *s)
{
    if (!s) return 1;
    while (*s) { if (!isspace((unsigned char)*s)) return 0; s++; }
    return 1;
}

/* Pontos de código Latin-1 mapeiam 1:1 em WinAnsi. Fora disso só precisamos
   das aspas e travessões que o Word costuma injetar em texto colado. */
static int cp_para_winansi(unsigned cp)
{
    if (cp < 0x80) return (int)cp;
    if (cp >= 0xA0 && cp <= 0xFF) return (int)cp;
    switch (cp) {
    case 0x20AC: return 0x80;  /* € */
    case 0x201A: return 0x82;
    case 0x0192: return 0x83;
    case 0x201E: return 0x84;
    case 0x2026: return 0x85;  /* … */
    case 0x2020: return 0x86;
    case 0x2021: return 0x87;
    case 0x2030: return 0x89;
    case 0x0160: return 0x8A;
    case 0x2039: return 0x8B;
    case 0x0152: return 0x8C;
    case 0x2018: return 0x91;
    case 0x2019: return 0x92;  /* ' */
    case 0x201C: return 0x93;
    case 0x201D: return 0x94;  /* " */
    case 0x2022: return 0x95;
    case 0x2013: return 0x96;  /* – */
    case 0x2014: return 0x97;  /* — */
    case 0x0161: return 0x9A;
    case 0x203A: return 0x9B;
    case 0x0153: return 0x9C;
    case 0x0178: return 0x9F;
    default:     return -1;
    }
}

void utf8_to_winansi(const char *src, char *dst, size_t dstsz)
{
    size_t o = 0;
    const unsigned char *p = (const unsigned char *)src;

    if (!dst || dstsz == 0) return;
    if (!src) { dst[0] = '\0'; return; }

    while (*p && o + 1 < dstsz) {
        unsigned cp;
        int extra, i, c;

        if (*p < 0x80)             { cp = *p;        extra = 0; }
        else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1Fu; extra = 1; }
        else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0Fu; extra = 2; }
        else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07u; extra = 3; }
        else { p++; continue; }    /* byte de continuação solto: descarta */
        p++;

        for (i = 0; i < extra; i++) {
            if ((*p & 0xC0) != 0x80) break;
            cp = (cp << 6) | (unsigned)(*p & 0x3F);
            p++;
        }
        if (i != extra) continue;  /* sequência truncada */

        c = cp_para_winansi(cp);
        dst[o++] = (char)(c < 0 ? '?' : c);
    }
    dst[o] = '\0';
}

static void formata_tm(const char *fmt, char *dst, size_t dstsz)
{
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    tmv = *localtime(&t);
#endif
    strftime(dst, dstsz, fmt, &tmv);
}

void data_hoje(char *dst, size_t dstsz)     { formata_tm("%d/%m/%Y", dst, dstsz); }
void data_hoje_iso(char *dst, size_t dstsz) { formata_tm("%Y-%m-%d", dst, dstsz); }
void agora_iso(char *dst, size_t dstsz)     { formata_tm("%Y-%m-%d %H:%M:%S", dst, dstsz); }

int data_valida(const char *s)
{
    int d, m, a, i;
    static const int dias[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

    if (!s || strlen(s) != 10) return 0;
    for (i = 0; i < 10; i++) {
        if (i == 2 || i == 5) { if (s[i] != '/') return 0; }
        else if (!isdigit((unsigned char)s[i])) return 0;
    }
    d = (s[0]-'0')*10 + (s[1]-'0');
    m = (s[3]-'0')*10 + (s[4]-'0');
    a = (s[6]-'0')*1000 + (s[7]-'0')*100 + (s[8]-'0')*10 + (s[9]-'0');

    if (m < 1 || m > 12 || d < 1 || a < 1900 || a > 2999) return 0;
    if (m == 2 && ((a % 4 == 0 && a % 100 != 0) || a % 400 == 0))
        return d <= 29;
    return d <= dias[m - 1];
}

void so_digitos(const char *src, char *dst, size_t dstsz)
{
    size_t o = 0;
    if (!dst || dstsz == 0) return;
    for (; src && *src && o + 1 < dstsz; src++)
        if (isdigit((unsigned char)*src)) dst[o++] = *src;
    dst[o] = '\0';
}

void cnpj_formata(const char *src, char *dst, size_t dstsz)
{
    char d[32];
    so_digitos(src, d, sizeof d);
    if (strlen(d) != 14) { str_copy(dst, dstsz, src); return; }
    snprintf(dst, dstsz, "%.2s.%.3s.%.3s/%.4s-%.2s",
             d, d + 2, d + 5, d + 8, d + 12);
}

int cnpj_valido(const char *src)
{
    static const int p1[12] = { 5,4,3,2,9,8,7,6,5,4,3,2 };
    static const int p2[13] = { 6,5,4,3,2,9,8,7,6,5,4,3,2 };
    char d[32];
    int i, soma, dv1, dv2, iguais = 1;

    so_digitos(src, d, sizeof d);
    if (strlen(d) != 14) return 0;
    for (i = 1; i < 14; i++) if (d[i] != d[0]) { iguais = 0; break; }
    if (iguais) return 0;

    for (soma = 0, i = 0; i < 12; i++) soma += (d[i] - '0') * p1[i];
    dv1 = soma % 11; dv1 = (dv1 < 2) ? 0 : 11 - dv1;
    if (d[12] - '0' != dv1) return 0;

    for (soma = 0, i = 0; i < 13; i++) soma += (d[i] - '0') * p2[i];
    dv2 = soma % 11; dv2 = (dv2 < 2) ? 0 : 11 - dv2;
    return d[13] - '0' == dv2;
}

int num_parse(const char *s, double *out)
{
    char buf[64];
    size_t o = 0;
    int viu_digito = 0;
    int ponto_e_decimal = 0;
    char *fim;
    double v;

    if (!s) return 0;
    while (*s && isspace((unsigned char)*s)) s++;

    /* No Brasil ',' é decimal e '.' é milhar — mas muita gente digita
       "0.7512" no teclado numérico. Sem tratar isso, o ponto seria descartado
       e 0.7512 viraria 7512 silenciosamente.

       Regra: havendo vírgula, o ponto é sempre milhar. Não havendo, um único
       ponto vale como decimal, exceto quando separa exatamente 3 dígitos
       finais — que é o caso de "5.000" litros. */
    if (!strchr(s, ',')) {
        const char *p1 = strchr(s, '.');
        if (p1 && !strchr(p1 + 1, '.')) {
            size_t casas = strspn(p1 + 1, "0123456789");
            if (p1[1 + casas] == '\0' && casas != 3)
                ponto_e_decimal = 1;
        }
    }

    for (; *s && o + 1 < sizeof buf; s++) {
        if (isdigit((unsigned char)*s)) { buf[o++] = *s; viu_digito = 1; }
        else if (*s == ',')             { buf[o++] = '.'; }
        else if (*s == '-' && o == 0)   { buf[o++] = '-'; }
        else if (*s == '.')             { if (ponto_e_decimal) buf[o++] = '.'; }
        else break;
    }
    buf[o] = '\0';
    if (!viu_digito) return 0;

    v = strtod(buf, &fim);
    if (fim == buf) return 0;
    if (out) *out = v;
    return 1;
}

void senha_hash(const char *senha, char *dst, size_t dstsz)
{
    unsigned long long h = 1469598103934665603ULL;
    const unsigned char *p = (const unsigned char *)(senha ? senha : "");
    for (; *p; p++) {
        h ^= (unsigned long long)*p;
        h *= 1099511628211ULL;
    }
    snprintf(dst, dstsz, "%016llx", h);
}
