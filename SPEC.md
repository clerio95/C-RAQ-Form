# RAQ Form Filler — Specification

Programa em C puro para preencher o *Formulário para Registro da Análise da Qualidade*
e gerar o PDF pronto para impressão e assinatura.

## 1. Decisões fechadas

| Tema | Decisão |
|---|---|
| Plataforma alvo | Windows (GUI Win32 nativa, `.exe` único, sem dependências) |
| Compilação | Cross-compile em Debian com `mingw-w64`, link estático |
| Saída | PDF gerado pelo próprio programa (escritor de PDF próprio, sem bibliotecas) |
| Pós-geração | Salva em pasta configurável pelo admin; pergunta "Deseja imprimir?" ao final |
| Registro | `historico.csv` com uma linha por formulário gerado |
| Armazenamento | Arquivos CSV em texto puro, ao lado do `.exe` |
| Colunas de produto | Dinâmicas: a tabela é desenhada com N colunas = nº de produtos marcados |
| Lista de produtos | Editável pelo admin (pré-carregada com os 4 atuais) |
| Faixas min/máx | Por produto, definidas pelo admin |
| Fora da faixa | Avisa (campo vermelho + confirmação), mas permite gerar |
| Aspecto / Cor | Padrão pré-selecionado, editável pelo analista |
| Admin | Botão discreto + senha, dentro do mesmo `.exe` (senha com hash no config) |

## 2. Layout da tabela gerada

Linhas **por produto** (uma célula por coluna):

- Produto
- Volume recebido (litros)
- Aspecto
- Cor
- Massa Específica a 20 °C
- Teor de álcool na Gasolina
- Teor Alcoólico no AEHC
- Teor de Biodiesel

Linhas **mescladas** (uma célula única atravessando todas as colunas):

- Data da coleta
- Distribuidor
- CNPJ do Distribuidor
- Transportador
- CNPJ do Transportador
- Nota Fiscal do Produto
- Placa do Caminhão/Reboque
- Nome Motorista
- RG Motorista
- Nome do Analista

Aplicabilidade das linhas de análise:

| Produto | Massa Esp. | Teor Gasolina | Teor AEHC | Teor Biodiesel |
|---|---|---|---|---|
| GASOLINA C | ✔ | ✔ | — | — |
| GASOLINA ADV | ✔ | ✔ | — | — |
| DIESEL S10 | ✔ | — | — | ✔ (14,5% a 15,5%) |
| ETANOL ADV | ✔ | — | ✔ | — |

Os três teores são percentuais; a faixa exibida ao lado do campo sai com `%`.
Toda a aplicabilidade e as faixas são editáveis pelo admin, por produto.

Células não aplicáveis são impressas com `-`.

Cores padrão: `AMARELADA` para GASOLINA C e DIESEL S10; `ESVERDEADA` para
GASOLINA ADV e ETANOL ADV. Aspecto padrão: `L.I.I.` para todos.

## 3. Arquivos de dados (CSV, `;` como separador, UTF-8)

```
dados/
  config.ini          posto (razão social, CNPJ, endereço, bairro, cidade/UF),
                      hash da senha admin, pasta de saída
  historico.csv       data_hora;arquivo;nf;data_coleta;distribuidor;transportador;
                      placa;motorista;analista;produtos;volumes
  produtos.csv        id;nome;aspecto_padrao;cor_padrao;massa_min;massa_max;
                      usa_teor_gas;gas_min;gas_max;usa_teor_aehc;aehc_min;aehc_max;ativo
  distribuidores.csv  id;nome;cnpj;padrao
  transportadores.csv id;nome;cnpj;padrao
  placas.csv          id;placa
  motoristas.csv      id;nome;rg
  analistas.csv       id;nome
```

Cadastros novos criados pelo usuário são gravados na hora e ficam disponíveis
nos formulários seguintes. O admin pode editar e excluir qualquer registro.

## 4. Fluxo do usuário

1. Abre o programa → já está na tela do formulário.
2. Cabeçalho do posto vem preenchido do `config.ini` (somente leitura).
3. Marca os produtos recebidos; cada marcação revela o campo de volume e a
   coluna correspondente no bloco de análise.
4. Data da coleta já vem com a data de hoje (editável).
5. Distribuidor / Transportador: seleciona na lista (o marcado como padrão já
   vem escolhido); o CNPJ preenche sozinho. Botão "+" cadastra um novo.
6. Nota Fiscal: digitada.
7. Placa e Motorista: seleciona da lista ou cadastra nova. O RG vem junto do
   motorista escolhido.
8. Analista: seleciona da lista.
9. Bloco de análise: Aspecto e Cor já vêm no padrão; Massa Específica e teores
   são digitados com a faixa esperada exibida ao lado do campo.
10. "Responsável pelo Preenchimento" vem com o nome do analista, editável.
11. Botão **Gerar formulário** → valida, avisa sobre valores fora da faixa,
    salva o PDF e registra a linha no `historico.csv`.
12. Pergunta **"Formulário salvo em ... Deseja imprimir agora?"** → se sim, manda
    para a impressora padrão.

## 5. Arquitetura do código

```
src/
  main.c          ponto de entrada, loop de mensagens Win32
  ui_form.c       janela principal do formulário
  ui_admin.c      janela de administração (senha, cadastros, faixas, config)
  ui_widgets.c    helpers de criação/layout de controles Win32
  model.c         structs do formulário + validação
  store.c         leitura/escrita dos CSV e do config.ini
  render.c        desenho do formulário (layout, tabela, texto) — independente de backend
  backend_pdf.c   backend de desenho que escreve um PDF
  backend_gdi.c   backend de desenho que pinta num DC de impressora
  pdf.c           escritor de PDF mínimo (objetos, xref, Helvetica/WinAnsi)
  util.c          strings, UTF-8→WinAnsi, datas, CNPJ
Makefile          alvos: windows (mingw-w64) e um alvo de teste em Linux
```

`render.c` desenha uma vez através de uma pequena interface (`draw_line`,
`draw_text`, `measure_text`); o mesmo desenho alimenta o PDF e a impressora, então
papel e arquivo nunca divergem.

Acentuação: fonte base Helvetica com `WinAnsiEncoding` cobre todo o português
(Ç, Ã, Ó, É…) sem precisar embutir fonte no PDF.

## 6. Suposições (me corrija se alguma estiver errada)

1. **Placa** — uma placa por entrega, escolhida da lista ou cadastrada na hora.
2. **Data** — formato `dd/mm/aaaa`.
3. **Volume** — número inteiro em litros.
4. **Massa Específica** — decimal com 4 casas (ex.: `0,7512`); a unidade é a que
   o admin usar na faixa.
5. **Teores de álcool** — percentual com 1 casa (ex.: `27,0`).
6. **Assinatura** — linha deixada em branco para assinar à mão.
7. **CNPJ** — formatado automaticamente e conferido pelo dígito verificador;
   dígito inválido gera aviso, não bloqueio.
8. **Nome do arquivo** — `RAQ_2026-07-30_NF123456.pdf`, com sufixo `_2`, `_3`…
   se já existir.
9. **Senha admin** — padrão `admin`, alterável na tela de administração.
10. **Histórico** — gravado em `historico.csv`; abrível em Excel. Sem tela de
    consulta dentro do programa por enquanto.
