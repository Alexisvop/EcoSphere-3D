[CmdletBinding()]
param(
    [switch]$Build,
    [switch]$Test,
    [switch]$Help
)

$ErrorActionPreference = 'Stop'
# Los codigos nativos se comprueban explicitamente tambien en PowerShell 7.
$PSNativeCommandUseErrorActionPreference = $false

if ($Help) {
    Write-Output @'
Uso: .\run-windows.ps1 [-Build] [-Test] [-Help]
Sin opciones: abre la plaza; compila si falta el ejecutable.
  -Build  Configura y compila sin abrir la aplicacion.
  -Test   Configura, compila y ejecuta CTest sin abrir la aplicacion.
  -Help   Muestra esta ayuda sin configurar ni compilar.
Requisitos: CMake 3.24+, Ninja, MSVC C++17, GLFW 3.4.0 y Python 3.10+.
Windows sigue pendiente de validacion nativa.
'@
    exit 0
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    [Console]::Error.WriteLine('Este lanzador requiere Windows y MSVC. Usa run-linux.sh en Linux.')
    exit 1
}

try {
    foreach ($tool in @('cmake.exe', 'ninja.exe', 'cl.exe')) {
        if (-not (Get-Command $tool -CommandType Application -ErrorAction SilentlyContinue)) {
            throw "Falta $tool. Usa Developer PowerShell for Visual Studio con las herramientas C++ instaladas. No se instalaran paquetes automaticamente."
        }
    }
    if ($Test -and -not (Get-Command ctest.exe -CommandType Application -ErrorAction SilentlyContinue)) {
        throw 'Falta ctest.exe; revisa la instalacion de CMake y PATH.'
    }
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}

Push-Location -LiteralPath $PSScriptRoot
try {
    $buildDir = Join-Path $PSScriptRoot 'build\windows-dev'
    $binary = Join-Path $buildDir 'ecosphere.exe'
    & cmake.exe --preset windows-dev '-DCMAKE_C_COMPILER=cl.exe' '-DCMAKE_CXX_COMPILER=cl.exe'
    if ($LASTEXITCODE -ne 0) {
        $code = $LASTEXITCODE
        [Console]::Error.WriteLine('Error de configuracion. Revisa MSVC C++17, GLFW 3.4.0 exacta, Python 3.10+ y docs/BUILD.md.')
        exit $code
    }
    if ($Build -or $Test -or -not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        & cmake.exe --build --preset windows-dev --parallel 2
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        throw 'No se genero ecosphere.exe en build/windows-dev.'
    }
    if ($Test) {
        & ctest.exe --preset windows-dev --output-on-failure
        exit $LASTEXITCODE
    }
    if ($Build) { exit 0 }
    foreach ($resource in @('shaders', 'assets')) {
        if (-not (Test-Path -LiteralPath (Join-Path $buildDir $resource) -PathType Container)) {
            throw 'Faltan shaders o modelos junto al ejecutable. Ejecuta -Build.'
        }
    }
    Set-Location -LiteralPath $buildDir
    & $binary --plaza
    exit $LASTEXITCODE
}
catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
finally {
    Pop-Location
}
