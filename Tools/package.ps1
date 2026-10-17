# Empaqueta el juego para Windows (Win64) con BuildCookRun en Saved/Package/Windows.
# Uso: Tools\package.ps1 [-Configuration Shipping|Development]
param(
    [ValidateSet('Development', 'Shipping')] [string]$Configuration = 'Shipping'
)
. "$PSScriptRoot\common.ps1"

$archive = Join-Path $ProjectRoot 'Saved\Package'
$cmdArgs = @('BuildCookRun', "-project=$UProject", '-noP4', '-platform=Win64', "-clientconfig=$Configuration",
    '-build', '-cook', '-stage', '-pak', '-iostore', '-package', '-archive', "-archivedirectory=$archive", '-unattended', '-utf8output')
$code = Invoke-Native $RunUAT $cmdArgs '(Error|error C|BUILD SUCCESSFUL|BUILD FAILED|AutomationTool exiting)'
if ($code -ne 0) { throw "BuildCookRun falló con código $code" }
Write-Output (Join-Path $archive 'Windows')
