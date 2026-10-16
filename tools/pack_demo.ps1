<#
  S11.5「送得出去」打包脚本 —— 把静态 Release exe 打成一个"别人能双击"的 zip

  用法（在仓库根，或任何地方）：
      powershell -File tools\pack_demo.ps1
      powershell -File tools\pack_demo.ps1 -Version 0.4.0 -NoBuild    # 只重新打包

  它做什么（顺序即步骤，任一步失败就停）：
      1. 配置 ms-msvc-release-static（静态 CRT ⇒ 零运行库依赖，S11.5 第 1 步）
      2. 构建 release-static
      3. 断言 exe 存在
      4. ★ 依赖自检：dumpbin 里不许出现 VCRUNTIME / MSVCP（"零依赖"的机器判据）
      5. 组目录 dist\<名字>\ ：exe + 启动器 + 玩家卡
      6. 压缩成 dist\<名字>.zip，并打印 路径 / 文件数 / 体积

  ⚠️ 三条纪律：
      · 只读 build-demo\、只写 dist\ —— **绝不碰** build\（开发构建）
      · 不引入第三方工具：只用 cmake + dumpbin（可选）+ Compress-Archive（PowerShell 自带）
      · 找不到 dumpbin 时**打印警告并继续**，但明确标注"依赖**未验证**" —— 不许当成通过

  ⚠️ 本文件存成 **UTF-8 带 BOM**：Windows PowerShell 5.1 读"无 BOM 的 UTF-8"会按
     ANSI(936) 解析 ⇒ 里面的中文会变乱码（甚至把引号吃掉）。这是 .ps1 这类脚本的常识，
     与 C++ 源码"UTF-8 无 BOM"的约定不冲突（那条是给 MSVC 看的）。
#>
[CmdletBinding()]
param(
    [string] $Version = "0.4.0",
    [switch] $NoBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot          # 脚本住在 tools\ 里 ⇒ 上一级是仓库根
$exePath  = Join-Path $repoRoot "build-demo\bin\Release\MiniEngine.exe"
$name     = "MiniEngine-Demo-v$Version-windows-x64"
$distDir  = Join-Path $repoRoot "dist"
$stageDir = Join-Path $distDir $name
$zipPath  = Join-Path $distDir "$name.zip"

Set-Location -LiteralPath $repoRoot                   # 让 --preset 解析到本仓库

Write-Host "== S11.5 打包 demo ==" -ForegroundColor Cyan
Write-Host "   仓库根 : $repoRoot"
Write-Host "   版本   : $Version"
Write-Host "   产物   : $zipPath"
Write-Host ""

# ---- 1 / 2：配置与构建（静态 CRT）----
if ($NoBuild) {
    Write-Host "[1/6] 跳过配置（-NoBuild）"
    Write-Host "[2/6] 跳过构建（-NoBuild）"
} else {
    Write-Host "[1/6] 配置 ms-msvc-release-static"
    & cmake --preset ms-msvc-release-static
    if ($LASTEXITCODE -ne 0) { throw "配置失败（exit=$LASTEXITCODE）" }

    Write-Host "[2/6] 构建 release-static"
    & cmake --build --preset release-static
    if ($LASTEXITCODE -ne 0) { throw "构建失败（exit=$LASTEXITCODE）" }
}

# ---- 3：产物存在？----
Write-Host "[3/6] 校验产物"
if (-not (Test-Path -LiteralPath $exePath)) {
    throw "找不到 exe：$exePath（先不带 -NoBuild 跑一次）"
}

# ---- 4：依赖自检（"零依赖"的机器判据）----
Write-Host "[4/6] 依赖自检"
$depsVerified = $false
$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if ($null -eq $dumpbin) {
    Write-Warning "找不到 dumpbin ⇒ **依赖未验证**！请用 VS 开发者命令提示符重跑，或手工确认 exe 不依赖 VCRUNTIME140/MSVCP140"
} else {
    $deps = & $dumpbin.Source /dependents $exePath
    $bad = $deps | Select-String -Pattern "VCRUNTIME|MSVCP"
    if ($bad) {
        Write-Host "    x 发现动态 CRT 依赖：" -ForegroundColor Red
        $bad | ForEach-Object { Write-Host ("      " + $_.Line) }
        throw "这不是静态 CRT 构建：exe 仍依赖 MSVC 运行库（检查 MINI_STATIC_CRT 是否生效、build-demo 是不是新的）"
    }
    $depsVerified = $true
    Write-Host "    v 依赖表里没有 VCRUNTIME / MSVCP" -ForegroundColor Green
}

# ---- 5：组目录 ----
Write-Host "[5/6] 组目录"
if (Test-Path -LiteralPath $stageDir) { Remove-Item -LiteralPath $stageDir -Recurse -Force }
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null

Copy-Item -LiteralPath $exePath -Destination $stageDir
Copy-Item -Path (Join-Path $PSScriptRoot "demo\*") -Destination $stageDir

$files = @(Get-ChildItem -LiteralPath $stageDir -File)

# ---- 6：压缩 ----
Write-Host "[6/6] 压缩"
if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
Compress-Archive -Path $stageDir -DestinationPath $zipPath -CompressionLevel Optimal

$zip    = Get-Item -LiteralPath $zipPath
$sizeKb = [math]::Round($zip.Length / 1KB, 1)

Write-Host ""
Write-Host "== 完成 ==" -ForegroundColor Cyan
Write-Host ("   zip  : " + $zip.FullName)
Write-Host ("   体积 : " + $sizeKb + " KB / " + $files.Count + " 个文件")
if ($depsVerified) {
    Write-Host "   依赖 : v 已验（无 VCRUNTIME / MSVCP）" -ForegroundColor Green
} else {
    Write-Host "   依赖 : ! **未验证**（本机没有 dumpbin）" -ForegroundColor Yellow
}
Write-Host ""
Write-Host "下一步：把 zip 拷到另一台机器 / 另一个 Windows 账户，解压后双击 开始试玩.cmd"
