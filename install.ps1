# Instalador do RAQ (Formulario para Registro da Analise da Qualidade).
#
# Uso, no PowerShell do Windows (nao precisa ser administrador):
#
#   irm https://raw.githubusercontent.com/clerio95/C-RAQ-Form/main/install.ps1 | iex
#
# Rodar de novo atualiza o programa e mantem cadastros, senha e historico.
#
# Para desinstalar: "Configuracoes > Aplicativos > RAQ" ou
#   & ([scriptblock]::Create((irm https://raw.githubusercontent.com/clerio95/C-RAQ-Form/main/install.ps1))) -Desinstalar
#
# Este arquivo e ASCII puro de proposito: o Windows PowerShell 5.1 le
# scripts sem BOM na pagina de codigo local e estragaria acentos.

param(
    [switch]$Desinstalar,
    [string]$Versao = ''
)

$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'   # deixa o download do PS 5.1 rapido

$Repo      = 'clerio95/C-RAQ-Form'
$Nome      = 'RAQ - Analise da Qualidade'
$Pasta     = Join-Path $env:LOCALAPPDATA 'Programs\RAQ'
$Exe       = Join-Path $Pasta 'RAQ.exe'
$ChaveApps = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\RAQ'

function Escreve([string]$t, [string]$cor = 'Gray') { Write-Host $t -ForegroundColor $cor }

function Atalhos {
    @(
        (Join-Path ([Environment]::GetFolderPath('Desktop'))  "$Nome.lnk"),
        (Join-Path ([Environment]::GetFolderPath('Programs')) "$Nome.lnk")
    )
}

function Espera-Fechar {
    while ($true) {
        $rodando = @(Get-Process -Name 'RAQ' -ErrorAction SilentlyContinue |
                     Where-Object { $_.Path -and ($_.Path -ieq $Exe) })
        if ($rodando.Count -eq 0) { return }
        Escreve 'O RAQ esta aberto. Feche o programa (salve o que estiver fazendo)' 'Yellow'
        Read-Host 'e pressione Enter para continuar' | Out-Null
    }
}

# ---------------------------------------------------------------- desinstalar
if ($Desinstalar) {
    Espera-Fechar
    foreach ($a in Atalhos) { Remove-Item -LiteralPath $a -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $ChaveApps -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $Exe -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath (Join-Path $Pasta 'desinstalar.ps1') -Force -ErrorAction SilentlyContinue

    $dados = Join-Path $Pasta 'dados'
    if (Test-Path -LiteralPath $dados) {
        Escreve ''
        Escreve "Os cadastros, a senha e o historico estao em: $dados" 'Yellow'
        $r = Read-Host 'Apagar tambem esses dados? Digite SIM para apagar (Enter mantem)'
        if ($r -ceq 'SIM') {
            Remove-Item -LiteralPath $Pasta -Recurse -Force
            Escreve 'Dados apagados.'
        } else {
            Escreve 'Dados mantidos. Reinstalar depois aproveita tudo.'
        }
    } elseif ((Test-Path -LiteralPath $Pasta) -and -not (Get-ChildItem -LiteralPath $Pasta)) {
        Remove-Item -LiteralPath $Pasta -Force
    }
    Escreve 'RAQ desinstalado. Os PDFs gerados nao foram apagados.' 'Green'
    return
}

# ------------------------------------------------------------------- instalar
Escreve ''
Escreve 'Instalando o RAQ - Formulario para Registro da Analise da Qualidade' 'Cyan'
Escreve ''

# PowerShell 5.1 ainda oferece TLS 1.0 por padrao; o GitHub exige 1.2.
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor 3072

if ($env:RAQ_URL) {
    $Url = $env:RAQ_URL                      # para testar um build local
} elseif ($Versao) {
    $Url = "https://github.com/$Repo/releases/download/$Versao/RAQ.exe"
} else {
    $Url = "https://github.com/$Repo/releases/latest/download/RAQ.exe"
}

New-Item -ItemType Directory -Force -Path $Pasta | Out-Null
$Novo = Join-Path $Pasta 'RAQ.exe.novo'

Escreve "Baixando $Url"
try {
    Invoke-WebRequest -Uri $Url -OutFile $Novo -UseBasicParsing
} catch {
    Remove-Item -LiteralPath $Novo -Force -ErrorAction SilentlyContinue
    Escreve "Nao foi possivel baixar o programa: $($_.Exception.Message)" 'Red'
    Escreve 'Confira a conexao com a internet e tente de novo.' 'Red'
    return
}

# Confere que veio um executavel de verdade (e nao uma pagina de erro).
$cabeca = [byte[]]::new(2)
$fs = [IO.File]::OpenRead($Novo)
try { [void]$fs.Read($cabeca, 0, 2) } finally { $fs.Close() }
if ($cabeca[0] -ne 0x4D -or $cabeca[1] -ne 0x5A) {
    Remove-Item -LiteralPath $Novo -Force
    Escreve 'O arquivo baixado nao e um programa valido. Nada foi alterado.' 'Red'
    return
}

# Confere o SHA-256 publicado junto do release, quando existir.
if (-not $env:RAQ_URL) {
    try {
        $esperado = (Invoke-WebRequest -Uri "$Url.sha256" -UseBasicParsing).Content
        if ($esperado -is [byte[]]) { $esperado = [Text.Encoding]::ASCII.GetString($esperado) }
        $esperado = ($esperado -split '\s+')[0].Trim().ToLower()
        $obtido   = (Get-FileHash -LiteralPath $Novo -Algorithm SHA256).Hash.ToLower()
        if ($esperado -and $esperado -ne $obtido) {
            Remove-Item -LiteralPath $Novo -Force
            Escreve 'O arquivo baixado nao confere com o publicado (SHA-256). Nada foi alterado.' 'Red'
            return
        }
    } catch {
        Escreve 'Aviso: nao foi possivel conferir o SHA-256 do download.' 'Yellow'
    }
}

$atualizacao = Test-Path -LiteralPath $Exe
Espera-Fechar
Move-Item -LiteralPath $Novo -Destination $Exe -Force
Unblock-File -LiteralPath $Exe -ErrorAction SilentlyContinue

# Atalhos na Area de Trabalho e no Menu Iniciar.
$shell = New-Object -ComObject WScript.Shell
foreach ($a in Atalhos) {
    $lnk = $shell.CreateShortcut($a)
    $lnk.TargetPath       = $Exe
    $lnk.WorkingDirectory = $Pasta
    $lnk.Description      = 'Formulario para Registro da Analise da Qualidade'
    $lnk.Save()
}

# Desinstalador local + entrada em "Aplicativos instalados" (so do usuario).
$desinst = Join-Path $Pasta 'desinstalar.ps1'
$codigoDesinst = @"
`$u = 'https://raw.githubusercontent.com/$Repo/main/install.ps1'
try {
    & ([scriptblock]::Create((Invoke-RestMethod `$u -UseBasicParsing))) -Desinstalar
} catch {
    Write-Host "Nao foi possivel baixar o desinstalador: `$(`$_.Exception.Message)"
}
Read-Host 'Pressione Enter para fechar' | Out-Null
"@
Set-Content -LiteralPath $desinst -Value $codigoDesinst -Encoding ASCII

New-Item -Path $ChaveApps -Force | Out-Null
$versaoExe = (Get-Item -LiteralPath $Exe).VersionInfo.ProductVersion
Set-ItemProperty -Path $ChaveApps -Name DisplayName     -Value 'RAQ - Analise da Qualidade'
Set-ItemProperty -Path $ChaveApps -Name DisplayVersion  -Value "$versaoExe"
Set-ItemProperty -Path $ChaveApps -Name Publisher       -Value 'RAQ Form Filler'
Set-ItemProperty -Path $ChaveApps -Name InstallLocation -Value $Pasta
Set-ItemProperty -Path $ChaveApps -Name DisplayIcon     -Value $Exe
Set-ItemProperty -Path $ChaveApps -Name NoModify        -Value 1 -Type DWord
Set-ItemProperty -Path $ChaveApps -Name NoRepair        -Value 1 -Type DWord
Set-ItemProperty -Path $ChaveApps -Name UninstallString `
    -Value "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$desinst`""

Escreve ''
if ($atualizacao) {
    Escreve "RAQ atualizado para a versao $versaoExe. Cadastros e senha foram mantidos." 'Green'
} else {
    Escreve "RAQ $versaoExe instalado em $Pasta" 'Green'
    Escreve 'Na primeira abertura o programa pede a senha do administrador e os dados do posto.'
}
Escreve "Atalho criado na Area de Trabalho: $Nome"
Escreve ''

Start-Process -FilePath $Exe -WorkingDirectory $Pasta
