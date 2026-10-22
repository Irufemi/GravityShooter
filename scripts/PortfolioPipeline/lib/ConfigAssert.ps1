function Assert-Config {
    param([hashtable]$Config)
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
    }
}
