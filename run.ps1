param (
    [switch]$debug,
    [switch]$gdb,
    [switch]$gcc
)

function log {
    param (
        [string]$text,
        [string]$color = "Green"
    )

    Write-Host "DRUGMAN COMPUTER PROGRAM ( " -NoNewLine -ForegroundColor Yellow
    Write-Host $text -NoNewLine -ForegroundColor $color
    Write-Host " )" -ForegroundColor Yellow
}

$output_exe = "./build/deathbird.exe"
$input_c = "./main.c"

$args = @()
if ($debug -or $gdb) {
    $args += @(
        "-g",
        "-DDEBUG",
        "-Wall",
        "-O0"
    )
} else {
    $args += @(
        "-O2"
    )
}

$args += @(
    "-o", $output_exe,
    $input_c,
    "-std=c99",
    "-I./raylib/include/",
    "-L./raylib/lib/",
    "-lraylib",
    "-lopengl32",
    "-lgdi32",
    "-lwinmm"
)

log "Building $input_c"

# Write-Host '====GCC===='
# & gcc @args
# Write-Host '====CLANG===='
& clang @args

# exit 0

if ($LASTEXITCODE -ne 0) {
    log "You are a horrible person" "Red"
    exit $LASTEXITCODE
}

log "Unexpected non-failure"

if ($gdb) {
    & gdb $output_exe
    exit
}

& $output_exe

