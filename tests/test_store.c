/* test_store.c — ida e volta dos cadastros em disco.

   Roda no Linux, sem GUI: é o que dá para verificar automaticamente da parte
   que a tela do Windows usa. Confere que gravar e reler devolve os mesmos
   dados, incluindo acentos e números com vírgula. */
#include "sha256.h"
#include "store.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;

static void checa(int cond, const char *msg)
{
    if (!cond) { fprintf(stderr, "FALHOU: %s\n", msg); falhas++; }
}

static void checa_str(const char *obtido, const char *esperado, const char *msg)
{
    if (strcmp(obtido, esperado)) {
        fprintf(stderr, "FALHOU: %s — obtido \"%s\", esperado \"%s\"\n",
                msg, obtido, esperado);
        falhas++;
    }
}

int main(void)
{
    const char *dir = "build/dados-teste";
    Store s, r;
    char cmd[256];

    snprintf(cmd, sizeof cmd, "rm -rf %s", dir);
    if (system(cmd) != 0) { /* pasta pode não existir ainda */ }

    /* Primeira carga: cria a pasta e semeia o catálogo de produtos. */
    checa(store_carrega(&s, dir) == 0, "store_carrega inicial");
    checa(s.n_produtos == store_catalogo_n(), "catálogo semeado");
    checa_str(s.produtos[0].nome, "GASOLINA COMUM", "primeiro produto");
    checa_str(s.produtos[0].cor_padrao, "AMARELADA", "cor da gasolina comum");
    checa(s.produtos[0].gas_min > 30.99 && s.produtos[0].gas_max < 33.01,
          "gasolina comum na faixa do E32");
    checa(s.produtos[3].usa_teor_aehc == 1, "etanol hidratado usa teor AEHC");
    checa(s.produtos[5].usa_teor_gas == 0, "diesel não usa teor de gasolina");
    checa_str(s.produtos[5].nome, "DIESEL S10", "diesel S10 no catálogo");
    checa(s.produtos[5].usa_teor_bio == 1, "diesel usa teor de biodiesel");
    checa(s.produtos[5].bio_min > 14.49 && s.produtos[5].bio_min < 14.51,
          "biodiesel mínimo 14,5");
    checa(s.produtos[5].bio_max > 15.49 && s.produtos[5].bio_max < 15.51,
          "biodiesel máximo 15,5");
    checa(s.produtos[0].usa_teor_bio == 0, "gasolina não usa teor de biodiesel");
    checa(s.produtos[0].massa_min > 734.99 && s.produtos[0].massa_min < 735.01,
          "massa da gasolina semeada em kg/m³");
    checa(s.produtos[5].massa_min > 816.99 && s.produtos[5].massa_min < 817.01,
          "massa do diesel semeada em kg/m³");

    /* Não existe mais senha padrão: a primeira execução cria a senha. */
    checa(!store_configurado(&s), "instalação nova sem senha");
    checa(str_empty(s.config.senha_hash), "hash vazio na instalação nova");
    checa(!senha_confere("admin", s.config.senha_hash), "'admin' não entra sem senha criada");
    checa(!senha_confere("", s.config.senha_hash), "senha vazia não entra");

    /* Hash novo: confere, recusa a errada, e o sal muda a cada geração. */
    {
        char h1[SENHA_HASH_MAX], h2[SENHA_HASH_MAX];
        senha_gera_hash("Posto#2026", h1, sizeof h1);
        senha_gera_hash("Posto#2026", h2, sizeof h2);
        checa(strncmp(h1, "pbkdf2-sha256$", 14) == 0, "formato do hash novo");
        checa(senha_confere("Posto#2026", h1), "senha certa confere");
        checa(!senha_confere("posto#2026", h1), "senha errada não confere");
        checa(strcmp(h1, h2) != 0, "sal aleatório muda o hash");
        checa(!senha_hash_legado(h1), "hash novo não é legado");
        checa(strlen(h1) < sizeof s.config.senha_hash, "hash cabe no config");
        str_copy(s.config.senha_hash, sizeof s.config.senha_hash, h1);
        checa(store_configurado(&s), "configurado após criar senha");
    }

    /* Hash FNV da versão 1.0 ("admin") ainda entra, e é reconhecido como
       legado para ser regravado. */
    checa(senha_confere("admin", "11ea048a30a155f2"), "hash legado confere");
    checa(!senha_confere("outra", "11ea048a30a155f2"), "hash legado recusa errada");
    checa(senha_hash_legado("11ea048a30a155f2"), "hash legado reconhecido");

    /* Vetores conhecidos de SHA-256 e PBKDF2-HMAC-SHA256 (RFC 7914 / 6070). */
    {
        unsigned char d[32];
        char hex[65];
        int k;
        sha256("abc", 3, d);
        for (k = 0; k < 32; k++) sprintf(hex + 2 * k, "%02x", d[k]);
        checa_str(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                  "sha256(abc)");
        pbkdf2_sha256((const unsigned char *)"password", 8,
                      (const unsigned char *)"salt", 4, 1, d);
        for (k = 0; k < 32; k++) sprintf(hex + 2 * k, "%02x", d[k]);
        checa_str(hex, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b",
                  "pbkdf2 1 iteração");
        pbkdf2_sha256((const unsigned char *)"password", 8,
                      (const unsigned char *)"salt", 4, 4096, d);
        for (k = 0; k < 32; k++) sprintf(hex + 2 * k, "%02x", d[k]);
        checa_str(hex, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a",
                  "pbkdf2 4096 iterações");
    }

    /* Grava dados com acento e vírgula decimal. */
    str_copy(s.config.razao_social, sizeof s.config.razao_social,
             "AUTO POSTO SÃO JOSÉ LTDA");
    str_copy(s.config.cidade_estado, sizeof s.config.cidade_estado, "RECIFE/PE");
    str_copy(s.config.pasta_saida, sizeof s.config.pasta_saida, "Formularios");
    str_copy(s.config.impressora, sizeof s.config.impressora,
             "HP LaserJet da Rede");
    checa(store_salva_config(&s) == 0, "salva config");

    checa(store_add_empresa(s.distribuidores, &s.n_distribuidores,
                            "DISTRIBUIDORA EXEMPLO DE PETRÓLEO S.A.",
                            "11222333000181") == 0, "add distribuidor");
    s.distribuidores[0].padrao = 1;
    checa(store_salva_distribuidores(&s) == 0, "salva distribuidores");

    checa(store_add_motorista(&s, "JOSÉ ANTÔNIO DA SILVA", "1234567") == 0,
          "add motorista");
    checa(store_salva_motoristas(&s) == 0, "salva motoristas");

    checa(store_add_placa(&s, "ABC1D23") == 0, "add placa");
    checa(store_add_placa(&s, "ABC1D23") == -1, "placa repetida é recusada");
    checa(store_salva_placas(&s) == 0, "salva placas");

    s.produtos[0].massa_min = 735.00;
    s.produtos[0].massa_max = 775.00;
    checa(store_salva_produtos(&s) == 0, "salva produtos");

    /* Relê tudo do zero. */
    checa(store_carrega(&r, dir) == 0, "store_carrega releitura");
    checa_str(r.config.razao_social, "AUTO POSTO SÃO JOSÉ LTDA", "razão social com acento");
    checa_str(r.config.cidade_estado, "RECIFE/PE", "cidade/estado");
    checa_str(r.config.impressora, "HP LaserJet da Rede",
              "impressora fixada preservada");
    checa(r.n_distribuidores == 1, "um distribuidor");
    checa_str(r.distribuidores[0].nome, "DISTRIBUIDORA EXEMPLO DE PETRÓLEO S.A.",
              "nome do distribuidor com acento");
    checa_str(r.distribuidores[0].cnpj, "11.222.333/0001-81", "CNPJ formatado");
    checa(r.distribuidores[0].padrao == 1, "distribuidor padrão preservado");
    checa(store_indice_padrao(r.distribuidores, r.n_distribuidores) == 0,
          "índice do padrão");
    checa(r.n_motoristas == 1, "um motorista");
    checa_str(r.motoristas[0].nome, "JOSÉ ANTÔNIO DA SILVA", "nome do motorista");
    checa_str(r.motoristas[0].rg, "1234567", "RG do motorista");
    checa(r.n_placas == 1, "uma placa");
    checa(r.n_produtos == store_catalogo_n(), "produtos relidos");
    checa(store_configurado(&r), "senha criada sobrevive à releitura");
    checa(r.produtos[0].massa_min > 734.99 && r.produtos[0].massa_min < 735.01,
          "massa mínima preservada");
    checa(r.produtos[0].massa_max > 774.99 && r.produtos[0].massa_max < 775.01,
          "massa máxima preservada");
    checa(r.produtos[5].usa_teor_bio == 1, "teor de biodiesel preservado");
    checa(r.produtos[5].bio_min > 14.49 && r.produtos[5].bio_min < 14.51,
          "biodiesel mínimo preservado");
    checa(r.produtos[5].bio_max > 15.49 && r.produtos[5].bio_max < 15.51,
          "biodiesel máximo preservado");

    /* Remoção desloca a lista corretamente. */
    store_add_placa(&r, "XYZ4E56");
    store_add_placa(&r, "QRS7T89");
    checa(r.n_placas == 3, "três placas");
    store_remove(r.placas, &r.n_placas, sizeof(Placa), 1);
    checa(r.n_placas == 2, "duas placas após remover");
    checa_str(r.placas[0].placa, "ABC1D23", "placa 0 após remoção");
    checa_str(r.placas[1].placa, "QRS7T89", "placa 1 após remoção");

    /* Campo com ';' não pode quebrar o CSV. */
    {
        Store t;
        store_add_analista(&r, "MARIA; DA SILVA");
        checa(store_salva_analistas(&r) == 0, "salva analistas");
        checa(store_carrega(&t, dir) == 0, "recarrega após ';'");
        checa(t.n_analistas == 1, "um analista mesmo com ';' no nome");
        checa_str(t.analistas[0].nome, "MARIA, DA SILVA",
                  "';' trocado por ',' na gravação");
    }

    /* Um produtos.csv gravado no formato antigo (g/cm³) precisa se converter
       sozinho na leitura, senão a faixa apareceria como "0,74 kg/m³". */
    {
        Store t;
        FILE *fp;
        const char *velho = "build/dados-antigo";
        char cam[256];

        snprintf(cam, sizeof cam, "rm -rf %s && mkdir -p %s", velho, velho);
        if (system(cam) != 0) { /* segue */ }
        snprintf(cam, sizeof cam, "%s/produtos.csv", velho);
        fp = fopen(cam, "wb");
        checa(fp != NULL, "cria produtos.csv antigo");
        if (fp) {
            fputs("id;nome;aspecto_padrao;cor_padrao;massa_min;massa_max;"
                  "usa_teor_gas;gas_min;gas_max;usa_teor_aehc;aehc_min;"
                  "aehc_max;ativo\n", fp);
            fputs("1;GASOLINA C;L.I.I.;AMARELADA;0.7350;0.7750;1;26.0;28.0;"
                  "0;0.0;0.0;1\n", fp);
            fputs("3;DIESEL S10;L.I.I.;AMARELADA;0.8170;0.8650;0;0.0;0.0;"
                  "0;0.0;0.0;1\n", fp);
            fputs("9;OLEO ESPECIAL;L.I.I.;AMARELADA;0.8500;0.9000;0;0.0;0.0;"
                  "0;0.0;0.0;1\n", fp);
            fclose(fp);
        }

        checa(store_carrega(&t, velho) == 0, "carrega cadastro antigo");
        checa(t.n_produtos == 3, "tres produtos no arquivo antigo");
        checa(t.produtos[0].massa_min > 734.99 && t.produtos[0].massa_min < 735.01,
              "massa mínima convertida para kg/m³");
        checa(t.produtos[0].massa_max > 774.99 && t.produtos[0].massa_max < 775.01,
              "massa máxima convertida para kg/m³");
        checa(t.produtos[0].usa_teor_gas == 1, "teor de gasolina preservado na migração");
        checa(t.produtos[1].massa_min > 816.99 && t.produtos[1].massa_min < 817.01,
              "massa do diesel convertida");

        /* Arquivo anterior ao biodiesel: o DIESEL S10 adota o padrão em vez de
           ficar sem exigência, e um produto desconhecido não ganha nada. */
        checa(t.produtos[1].usa_teor_bio == 1, "diesel antigo ganha teor de biodiesel");
        checa(t.produtos[1].bio_min > 14.49 && t.produtos[1].bio_min < 14.51,
              "faixa de biodiesel adotada no diesel antigo");
        checa(t.produtos[0].usa_teor_bio == 0, "gasolina antiga segue sem biodiesel");
        checa(t.produtos[2].usa_teor_bio == 0, "produto desconhecido segue sem biodiesel");

        /* A conversão é gravada, então reler não converte de novo. */
        {
            Store t2;
            checa(store_carrega(&t2, velho) == 0, "recarrega já migrado");
            checa(t2.produtos[0].massa_min > 734.99 && t2.produtos[0].massa_min < 735.01,
                  "migração não se repete");
            checa(t2.produtos[1].usa_teor_bio == 1,
                  "biodiesel do diesel persiste após regravação");
        }
    }

    /* Caminho com acento: no Windows era exatamente isto que quebrava tudo,
       porque fopen/_mkdir são ANSI e recebiam bytes UTF-8. */
    {
        const char *acent = "build/dados-João-Ação";
        Store t, t2;
        char cmd2[256];

        snprintf(cmd2, sizeof cmd2, "rm -rf '%s'", acent);
        if (system(cmd2) != 0) { /* segue */ }

        checa(store_carrega(&t, acent) == 0, "carrega em pasta com acento");
        str_copy(t.config.razao_social, sizeof t.config.razao_social,
                 "POSTO AÇÃO LTDA");
        checa(store_salva_config(&t) == 0, "grava config em pasta com acento");
        checa(store_add_placa(&t, "ABC1D23") == 0, "add placa em pasta com acento");
        checa(store_salva_placas(&t) == 0, "grava placas em pasta com acento");

        checa(store_carrega(&t2, acent) == 0, "relê de pasta com acento");
        checa_str(t2.config.razao_social, "POSTO AÇÃO LTDA",
                  "config relida de pasta com acento");
        checa(t2.n_placas == 1, "placa relida de pasta com acento");
        checa(t2.n_produtos == store_catalogo_n(), "produtos semeados em pasta com acento");
    }

    if (falhas) { fprintf(stderr, "%d verificação(ões) falharam\n", falhas); return 1; }
    printf("ok: cadastros gravam e releem corretamente\n");
    return 0;
}
