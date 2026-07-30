# RAQ Form Filler

Programa em C puro que preenche e gera o **Formulário para Registro da Análise
da Qualidade** em PDF, pronto para imprimir e assinar.

Feito para ser usado por quem não tem prática com computador: abre já na tela
do formulário, quase tudo vem preenchido, e o que falta é marcar os produtos e
digitar os números.

## Instalação

Copie a pasta `dist` inteira para o computador do posto. É só isso — o `RAQ.exe`
não depende de nada instalado, nem de .NET, nem de leitor de PDF para gerar o
arquivo.

Na primeira execução o programa cria sozinho, ao lado do `.exe`:

```
dados/            cadastros e configuração (arquivos de texto)
Formularios/      os PDFs gerados
```

Para fazer backup, copie a pasta `dados`. Para levar tudo para outro
computador, copie a pasta inteira.

## Primeiro uso (administrador)

1. Abra o `RAQ.exe`.
2. Clique em **Administração**. A senha inicial é `admin`.
3. Preencha os **dados fixos do posto** (razão social, CNPJ, endereço, bairro,
   cidade/estado) e clique em *Salvar dados do posto*. Esses dados vão no
   cabeçalho de todo formulário.
4. Em **Produtos**, confira os padrões e as faixas de cada produto (aspecto,
   cor, massa específica mínima/máxima, teores). Já vem preenchido com
   GASOLINA C, GASOLINA ADV, DIESEL S10 e ETANOL ADV.

   Cada produto diz quais teores exige, e cada teor tem sua própria faixa:

   | Produto | Massa Específica | Teor Gasolina | Teor AEHC | Teor Biodiesel |
   |---|---|---|---|---|
   | GASOLINA C / ADV | 735,00 a 775,00 kg/m³ | 26% a 28% | — | — |
   | DIESEL S10 | 817,00 a 865,00 kg/m³ | — | — | 14,5% a 15,5% |
   | ETANOL ADV | 807,60 a 811,20 kg/m³ | — | 99,3% a 100% | — |

   A massa específica é digitada e impressa em **kg/m³** (`751,20`, não
   `0,7512`) e os teores em **%**. A unidade sai junto do valor no formulário.
   As linhas que não se aplicam ao produto saem com `-`.
5. Em **Cadastros**, cadastre os distribuidores e transportadores. Selecione o
   mais usado e clique em *Definir padrão* — ele virá escolhido sozinho no
   formulário, poupando tempo do operador.
6. Troque a senha em *Alterar senha*.

> Ao **incluir ou excluir um produto**, feche e abra o programa para a tela de
> preenchimento se ajustar.

## Uso no dia a dia (operador)

1. Marque os produtos que chegaram. Ao marcar, aparece o campo de volume e a
   coluna do produto no bloco de análise.
2. A **data da coleta** já vem com a data de hoje.
3. Escolha distribuidor, transportador, placa, motorista e analista nas listas.
   O CNPJ e o RG aparecem sozinhos. O botão **Novo...** cadastra na hora quem
   ainda não está na lista, e o cadastro fica salvo para os próximos
   formulários.
4. Digite a nota fiscal e, no bloco de análise, os valores medidos. A faixa
   esperada aparece embaixo de cada campo; **valor fora da faixa fica com o
   fundo vermelho**, mas não impede nada — só pede confirmação antes de gerar.
5. **Responsável pelo Preenchimento** vem com o nome do analista e pode ser
   trocado.
6. Clique em **Gerar formulário**. O PDF é salvo e o programa pergunta se você
   quer imprimir agora.

A assinatura é feita à mão, no papel.

## Onde ficam os dados

Tudo em `dados/`, em arquivos de texto separados por `;` que abrem no Excel:

| Arquivo | Conteúdo |
|---|---|
| `config.ini` | dados do posto, pasta de saída, senha (em hash) |
| `produtos.csv` | produtos, padrões de aspecto/cor e faixas |
| `distribuidores.csv` / `transportadores.csv` | nome, CNPJ, qual é o padrão |
| `placas.csv`, `motoristas.csv`, `analistas.csv` | cadastros reutilizáveis |
| `historico.csv` | uma linha por formulário gerado |

O `historico.csv` serve para consultar depois sem abrir PDF: registra data,
arquivo, nota fiscal, distribuidor, transportador, placa, motorista, analista,
produtos e volumes.

## Compilação

Ambiente de desenvolvimento: Debian com `mingw-w64`.

```fish
make                # gera dist/RAQ.exe (cross-compile para Windows)
make test           # roda os testes no Linux (PDF de exemplo + cadastros)
make widths         # regenera as larguras da fonte a partir dos AFM
make clean
```

O `make test` grava PDFs de exemplo em `build/` — útil para conferir o layout
sem precisar de Windows.

## Como o código está organizado

```
src/
  main.c          entrada, laço de mensagens
  ui_form.c       tela do formulário
  ui_admin.c      tela de administração e editor de produto
  win_util.c      ponte UTF-8 <-> UTF-16 e helpers de controle Win32
  model.h         estruturas do formulário e dos cadastros
  store.c         leitura/gravação dos CSV
  render.c        desenho do formulário (layout, tabela, texto)
  backend_pdf.c   desenho -> arquivo PDF
  backend_gdi.c   desenho -> impressora
  pdf.c           escritor de PDF mínimo
  util.c          strings, datas, CNPJ, números no formato brasileiro
```

Duas decisões que explicam o resto:

**Um desenho, dois destinos.** `render.c` desenha o formulário através de uma
interface pequena (`linha`, `texto`, `largura`). O PDF e a impressora
implementam essa interface. Não existe um layout para o arquivo e outro para o
papel, então eles não têm como divergir. O backend da impressora inclusive mede
o texto pela tabela Helvetica do PDF, e não pelo GDI, para que centralização e
encolhimento decidam a mesma coisa nos dois.

**UTF-8 por dentro, UTF-16 só na borda.** Todo o modelo guarda texto em UTF-8;
a conversão acontece em `win_util.c` (para a API do Windows) e em `util.c` (para
o WinAnsi do PDF). Assim acentos em nomes de motoristas e razões sociais não
dependem da página de código da máquina.

A geometria da tabela (`X_ESQ`, `X_ROTULO_FIM`, alturas de linha) foi extraída
das coordenadas reais do `RAQ-TEMPLATE.pdf` original, por isso o resultado sai
igual ao modelo em papel. A diferença é que o número de colunas acompanha
quantos produtos foram marcados, em vez das 5 fixas do template.
