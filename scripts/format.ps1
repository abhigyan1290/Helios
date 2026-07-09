$ErrorActionPreference = "Stop"

$files = Get-ChildItem -Path include, src, apps, tests -Recurse -Include *.cpp, *.hpp, *.h

if ($files.Count -eq 0) {
    Write-Host "No C++ files to format."
    exit 0
}

clang-format -i @($files.FullName)

