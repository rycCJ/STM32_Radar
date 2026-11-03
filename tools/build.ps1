param([string]$Keil = 'D:\APPS\Keil539\UV4\UV4.exe', [switch]$Rebuild)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildDir = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
$flag = if ($Rebuild) { '-r' } else { '-b' }
$projects = @(
    @('controller', 'firmware\controller\Project\MDK-ARM(uV4)\project.uvprojx'),
    @('radar', 'firmware\radar\proj\V1.1.2_chirpmean_44+86\Jupiter.uvprojx')
)
foreach ($entry in $projects) {
    $projectPath = Join-Path $projectRoot $entry[1]
    $logPath = Join-Path $buildDir ($entry[0] + '.log')
    $arguments = "$flag `"$projectPath`" -j0 -o `"$logPath`""
    $process = Start-Process -FilePath $Keil -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    $log = Get-Content -LiteralPath $logPath -Raw
    Write-Output ($entry[0] + ':')
    Get-Content -LiteralPath $logPath -Tail 8
    if ($process.ExitCode -gt 1 -or $log -notmatch '0 Error\(s\)') {
        throw "Keil build failed; see $logPath"
    }
}
