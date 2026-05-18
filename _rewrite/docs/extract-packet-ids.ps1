$ErrorActionPreference = 'Stop'

$bases = @{
    'SM_BASE'    = 0x1581
    'MW_BASE'    = 0x9001
    'DM_BASE'    = 0x5891
    'CS_LOGIN'   = 0x1987
    'CS_MAP'     = 0x5280
    'CT_CONTROL' = 0x9301
    'CT_PATCH'   = 0x4201
    'RW_RELAY'   = 0x9999
    'CS_CUSTOM'  = 0x3312
}

$files = @(
    'SSProtocol.h',
    'CTProtocol.h',
    'DMProtocol.h',
    'MWProtocol.h',
    'CSProtocol.h'
)

$root = Resolve-Path '..\..\Lib\Own\TProtocol\include'
$results = New-Object System.Collections.ArrayList

foreach ($f in $files) {
    $path = Join-Path $root $f
    $lineNo = 0
    Get-Content -Path $path -Encoding Default | ForEach-Object {
        $lineNo++
        $line = $_
        if ($line -match '^\s*#define\s+(\w+)\s+\((\w+)\s*\+\s*0x([0-9A-Fa-f]+)\)') {
            $name   = $matches[1]
            $base   = $matches[2]
            $offset = [Convert]::ToInt32($matches[3], 16)
            if ($bases.ContainsKey($base)) {
                $absId = $bases[$base] + $offset
                [void]$results.Add([pscustomobject]@{
                    Id      = '0x' + $absId.ToString('X4')
                    IdDec   = $absId
                    Name    = $name
                    Base    = $base
                    Offset  = '0x' + $offset.ToString('X4')
                    File    = $f
                    Line    = $lineNo
                })
            }
        }
    }
}

# Emit CSV
$csv = Join-Path (Split-Path $PSCommandPath -Parent) 'packet-ids.csv'
$results | Sort-Object IdDec | Export-Csv -Path $csv -NoTypeInformation -Encoding UTF8

# Stats by namespace prefix
$byPrefix = $results | Group-Object { ($_.Name -split '_')[0] } | Sort-Object Count -Descending
$byBase = $results | Group-Object Base | Sort-Object Count -Descending

"Total IDs: $($results.Count)"
""
"By namespace prefix:"
$byPrefix | Select-Object Name, Count | Format-Table -AutoSize | Out-String
""
"By base:"
$byBase | Select-Object Name, Count | Format-Table -AutoSize | Out-String
""
"Range overlap check:"
$results | Group-Object Id | Where-Object { $_.Count -gt 1 } | ForEach-Object {
    "  COLLISION at $($_.Name): " + (($_.Group | ForEach-Object { $_.Name }) -join ', ')
}
"  (none if no output above)"
""
"CSV written: $csv"
