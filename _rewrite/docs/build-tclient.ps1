#requires -Version 5.1
<#
.SYNOPSIS
    Builds the legacy TClient.sln using Visual Studio 2026 with project retargeting.

.DESCRIPTION
    The original solution targets v141 (VS 2017) toolset + Windows 10 SDK 10.0.17763.0.
    Neither is installed by default on VS 2026 (v18). We retarget on the command line
    via MSBuild properties so we don't have to modify the original .vcxproj files.

    Required VS 2026 components: NativeDesktop workload + C++ MFC + ATL (v143+).
.NOTES
    Component IDs to ensure installed:
        Microsoft.VisualStudio.Workload.NativeDesktop
        Microsoft.VisualStudio.Component.VC.ATL
        Microsoft.VisualStudio.Component.VC.ATLMFC
        Microsoft.VisualStudio.Component.Windows11SDK.26100
#>
param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [ValidateSet('x86','x64')][string]$Platform = 'x86'
)

$ErrorActionPreference = 'Stop'

$msbuild = 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe'
if (-not (Test-Path $msbuild)) {
    throw "MSBuild not found at $msbuild — install VS 2026 with C++ workload"
}

$sln = Resolve-Path (Join-Path $PSScriptRoot '..\..\Client\TClient.sln')
$logDir = $PSScriptRoot
$log = Join-Path $logDir 'tclient-build.log'

Write-Output "Building $sln ($Configuration|$Platform), log -> $log"
& $msbuild $sln `
    "/p:Configuration=$Configuration" `
    "/p:Platform=$Platform" `
    '/p:PlatformToolset=v143' `
    '/p:WindowsTargetPlatformVersion=10.0' `
    /m /verbosity:minimal /nologo /clp:Summary `
    2>&1 | Tee-Object -FilePath $log
