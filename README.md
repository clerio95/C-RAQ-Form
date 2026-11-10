# RAQ Form Filler

Programa para Windows que preenche e gera o **Formulário para Registro da
Análise da Qualidade** em PDF, pronto para imprimir e assinar. Serve para
qualquer posto revendedor: nada de um posto específico vem embutido. Os
dados do posto, os produtos, os analistas, os distribuidores e os
transportadores são cadastrados na primeira execução.

Feito para ser usado por quem não tem prática com computador: abre já na tela
do formulário, quase tudo vem preenchido, e o que falta é marcar os produtos e
digitar os números.

## Instalação

No computador do posto, abra o **PowerShell** (menu Iniciar → digite
"PowerShell") e cole:

```powershell
irm https://raw.githubusercontent.com/clerio95/C-RAQ-Form/main/install.ps1 | iex
```

Não precisa ser administrador. O instalador:

- baixa o `RAQ.exe` do release mais recente do GitHub e confere o SHA-256;
- instala em `%LOCALAPPDATA%\Programs\RAQ` (pasta do próprio usuário);
- cria atalhos na Área de Trabalho e no Menu Iniciar;
- registra o programa em *Configurações → Aplicativos*, para desinstalar por lá;
- abre o programa no fim.

O `RAQ.exe` é um executável único: não depende de .NET, de bibliotecas nem de
leitor de PDF para gerar o arquivo.

**Atualizar** é rodar o mesmo comando de novo. Cadastros, senha e histórico
ficam onde estão.

Sem acesso ao PowerShell, também funciona copiar o `RAQ.exe` (página de
*Releases* do GitHub) para uma pasta com permissão de escrita, como `C:\RAQ`, e
abrir dali.

## Primeira execução

Na primeira vez que o programa abre, um assistente pede, em 7 passos:

1. **Senha do administrador** — protege a tela de Administração. Não existe
   senha padrão.
2. **Dados do posto** — razão social, CNPJ, endereço, bairro, cidade/UF. Vão no
   cabeçalho de todo formulário.
3. **Produtos** — marque os que o posto vende, entre gasolina comum,
   aditivada e premium, etanol hidratado e aditivado, diesel S10 e S500.
4. **Analistas** — quem faz a análise no recebimento.
5. **Distribuidores** — de quem o posto compra (nome e CNPJ).
6. **Transportadores** — quem faz o transporte (nome e CNPJ).
7. **Pasta dos PDFs e impressora** — a pasta padrão é
   `Documentos\Formularios RAQ`.

Os passos 4 a 6 podem ficar em branco: os cadastros também podem ser feitos
depois, no botão **Novo...** do formulário ou em **Administração**. Cancelar o
assistente não grava nada, e ele recomeça na próxima abertura.

### Faixas dos produtos

As faixas que o assistente sugere são um ponto de partida e **precisam ser
conferidas com a legislação vigente**. Os teores de mistura mudam por decisão
do governo:

| Produto | Massa específica | Teor sugerido |
|---|---|---|
| Gasolina comum / aditivada | 735,00 a 775,00 kg/m³ | álcool 31% a 33% (E32, desde 01/08/2026) |
| Gasolina premium | 735,00 a 775,00 kg/m³ | álcool 24% a 26% |
| Etanol hidratado / aditivado | 805,20 a 811,20 kg/m³ | teor alcoólico 92,5% a 94,6% |
| Diesel S10 / S500 | 817,00 a 865,00 kg/m³ | biodiesel 14,5% a 15,5% (B15) |

Quando a mistura mudar, ajuste em **Administração → Produtos → Editar...**. Um
valor fora da faixa só deixa o campo vermelho e pede confirmação, mas nunca
impede de gerar o formulário.

## Administração

Botão **Administração**, no canto do formulário, com a senha criada na
primeira execução. Ali se altera:

- dados do posto, pasta dos PDFs e impressora;
- produtos: nome, aspecto e cor padrão, faixas, quais teores cada um exige;
- cadastros: distribuidores, transportadores, placas, motoristas e analistas.
  *Definir padrão* escolhe o distribuidor/transportador que já vem
  selecionado no formulário;
- a senha.

Incluir, excluir ou renomear produto atualiza a tela do formulário na hora.

**Impressora:** `(usar a padrão do Windows)` segue a impressora padrão do
Windows. Escolher uma pelo nome **fixa** essa impressora, o que vale a pena
numa rede para o formulário não ir parar no "Microsoft Print to PDF". Antes de
imprimir, o programa sempre mostra o nome da impressora na pergunta.

### Esqueci a senha

Abra a pasta de dados (`%LOCALAPPDATA%\Programs\RAQ\dados`), edite o
`config.ini` no Bloco de Notas e apague a linha que começa com `senha_hash=`.
Na próxima vez que clicar em Administração, o programa pede uma senha nova.
Nenhum cadastro é perdido.

A senha é guardada como hash PBKDF2-SHA256 com sal. Ela impede o operador de
mexer na configuração, mas não substitui as permissões do Windows: quem tem
acesso à pasta de dados consegue redefini-la, como descrito acima.

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
   quer imprimir agora. O botão **Abrir pasta dos PDFs** mostra os formulários
   já gerados.

A assinatura é feita à mão, no papel.

## Onde ficam os dados

Tudo em `dados\`, ao lado do `RAQ.exe` (instalado:
`%LOCALAPPDATA%\Programs\RAQ\dados`). São arquivos de texto separados por
`;` que abrem no Excel:

| Arquivo | Conteúdo |
|---|---|
| `config.ini` | dados do posto, pasta de saída, impressora, senha (em hash) |
| `produtos.csv` | produtos, padrões de aspecto/cor e faixas |
| `distribuidores.csv` / `transportadores.csv` | nome, CNPJ, qual é o padrão |
| `placas.csv`, `motoristas.csv`, `analistas.csv` | cadastros reutilizáveis |
| `historico.csv` | uma linha por formulário gerado |

O `historico.csv` serve para consultar depois sem abrir PDF: registra data,
arquivo, nota fiscal, distribuidor, transportador, placa, motorista, analista,
produtos e volumes.

**Backup:** copie a pasta `dados`. Para levar tudo para outro computador,
instale lá e substitua a pasta `dados` pela cópia.

Nada disso vai para o repositório: o `.gitignore` exclui `dados/` e os PDFs.

## Atualizando de uma versão anterior

Nada precisa ser reconfigurado. Os arquivos antigos são lidos e, quando faz
falta, convertidos uma única vez:

| Mudança | O que acontece |
|---|---|
| Massa específica de g/cm³ para kg/m³ | `0,7350` vira `735,00` e o arquivo é regravado convertido |
| Linha "Teor de Biodiesel" | produto sem a coluna adota o padrão do catálogo pelo nome (ex.: DIESEL S10 → 14,5% a 15,5%) |
| Senha em formato antigo | continua valendo e é regravada no formato novo no próximo acesso à Administração |
| Senha ainda `admin` | o programa exige criar uma senha nova no próximo acesso |
| Faixas já cadastradas | **não são alteradas**. Confira o teor de álcool da gasolina (E32) |

## Desinstalar

*Configurações → Aplicativos → RAQ → Desinstalar*, ou no PowerShell:

```powershell
& ([scriptblock]::Create((irm https://raw.githubusercontent.com/clerio95/C-RAQ-Form/main/install.ps1))) -Desinstalar
```

O desinstalador pergunta antes de apagar os dados. Os PDFs nunca são apagados.

## Compilação

Ambiente de desenvolvimento: Debian com `mingw-w64`.

```fish
make                    # gera dist/RAQ.exe (cross-compile para Windows)
make VERSION=1.2.0      # idem, carimbando a versão no .exe
make test               # roda os testes no Linux (PDF de exemplo + cadastros)
make widths             # regenera as larguras da fonte a partir dos AFM
make clean
```

O `make test` grava PDFs de exemplo em `build/`, úteis para conferir o layout
sem precisar de Windows.

## Publicando uma versão

O GitHub Actions compila e testa a cada push (`.github/workflows/build.yml`).
Para publicar uma versão que o instalador vai baixar:

```fish
git tag v1.1.0
git push origin v1.1.0
```

O workflow `release.yml` roda os testes, compila o `RAQ.exe` com a versão da
tag, calcula o SHA-256 e cria o release com os dois arquivos. O comando de
instalação sempre pega o release mais recente. O repositório precisa ser
**público** para o `irm` funcionar sem login.

## Como o código está organizado

```
src/
  main.c          entrada, laço de mensagens
  ui_form.c       tela do formulário
  ui_admin.c      tela de administração e editor de produto
  ui_setup.c      assistente da primeira execução
  win_util.c      ponte UTF-8 <-> UTF-16 e helpers de controle Win32
  model.h         estruturas do formulário e dos cadastros
  store.c         leitura/gravação dos CSV
  render.c        desenho do formulário (layout, tabela, texto)
  backend_pdf.c   desenho -> arquivo PDF
  backend_gdi.c   desenho -> impressora
  pdf.c           escritor de PDF mínimo
  util.c          strings, datas, CNPJ, números no formato brasileiro, senha
  sha256.c        SHA-256 / PBKDF2 para o hash da senha
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
