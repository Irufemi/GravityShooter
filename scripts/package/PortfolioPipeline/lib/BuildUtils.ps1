function Find-MSBuildPath {
    $msbuildPath = $null
    $vswherePath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswherePath) {
        $vsInstallPath = & $vswherePath -latest -requires Microsoft.Component.MSBuild -property installationPath
        if (-not [string]::IsNullOrEmpty($vsInstallPath)) {
            $msbuildPath = Join-Path $vsInstallPath "MSBuild\Current\Bin\MSBuild.exe"
        }
    }

    if ($null -eq $msbuildPath -or -not (Test-Path $msbuildPath)) {
        $defaultPaths = @(
            'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe',
            'C:\Program Files\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\MSBuild.exe',
            'C:\Program Files\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\MSBuild.exe',
            'C:\Program Files\Microsoft Visual Studio\2026\Community\MSBuild\Current\Bin\MSBuild.exe',
            'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
            'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe',
            'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe',
            'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe'
        )
        foreach ($path in $defaultPaths) {
            if (Test-Path $path) {
                $msbuildPath = $path
                break
            }
        }
    }

    if ($null -eq $msbuildPath -or -not (Test-Path $msbuildPath)) {
        throw "MSBuild.exe was not found on this system."
    }
    return $msbuildPath
}
