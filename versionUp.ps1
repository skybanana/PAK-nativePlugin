param(
    [string]$Root,
    [string]$Version
)

$ErrorActionPreference = 'Stop'

$main = Join-Path $Root 'src\main.cpp'
$resource = Join-Path $Root 'src\PAKNativePlugin.rc'
$quote = [char]34

$mainContent = Get-Content -Raw $main
$mainContent = $mainContent -replace 'return \x22\d+\.\d+\.\d+\x22;', ('return ' + $quote + $Version + $quote + ';')
[IO.File]::WriteAllText($main, $mainContent, [Text.UTF8Encoding]::new($false))

$resourceVersion = $Version -replace '\.', ','
$resourceContent = Get-Content -Raw $resource
$resourceContent = $resourceContent -replace '(FILEVERSION|PRODUCTVERSION) \d+,\d+,\d+,0', ('$1 ' + $resourceVersion + ',0')
$resourceContent = $resourceContent -replace '(VALUE \x22(?:File|Product)Version\x22, )\x22\d+\.\d+\.\d+\x22', ('$1' + $quote + $Version + $quote)
[IO.File]::WriteAllText($resource, $resourceContent, [Text.UTF8Encoding]::new($false))
