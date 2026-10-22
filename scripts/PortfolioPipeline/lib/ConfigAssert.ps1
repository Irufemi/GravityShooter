function Assert-Config {
    param(
        [hashtable]$Config,
        [string]$BaseDir = ""
    )
    if ($null -eq $Config.output) {
        throw "output is missing in config.psd1."
    }
    foreach ($field in @('zip_name', 'generated_dir')) {
        if ([string]::IsNullOrEmpty($Config.output.$field)) {
            throw "Required output field '$field' is missing or empty."
        }
    }
    if ($null -eq $Config.works -or $Config.works.Count -eq 0) { return }
    foreach ($work in $Config.works) {
        if ([string]::IsNullOrEmpty($work.work_folder_name)) {
            throw "work_folder_name is missing or empty in works."
        }

        # Source exclusivity check (Git vs Local)
        $hasGit = -not [string]::IsNullOrEmpty($work.git_repository_url)
        $hasLocal = -not [string]::IsNullOrEmpty($work.local_source_path)
        $sourceCount = [int]$hasGit + [int]$hasLocal

        if ($sourceCount -eq 0) {
            throw "No source specified for work '$($work.work_folder_name)'. Specify either git_repository_url or local_source_path."
        }
        if ($sourceCount -gt 1) {
            throw "Specify only one of git_repository_url or local_source_path for work '$($work.work_folder_name)'."
        }
        if ($hasLocal) {
            $checkPath = if ([System.IO.Path]::IsPathRooted($work.local_source_path) -or [string]::IsNullOrEmpty($BaseDir)) {
                $work.local_source_path
            } else {
                Join-Path $BaseDir $work.local_source_path
            }
            if (-not (Test-Path $checkPath)) {
                throw "local_source_path does not exist: $($work.local_source_path)"
            }
        }
    }
}
