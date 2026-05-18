$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName 'System.Data'

$outDir = Join-Path (Split-Path $PSCommandPath -Parent) 'schema'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

function Open-Conn([string]$db) {
    $cs = "Server=localhost;Database=$db;Integrated Security=True;TrustServerCertificate=True;"
    $c = New-Object System.Data.SqlClient.SqlConnection $cs
    $c.Open()
    $c
}

function Read-Rows($conn, [string]$sql) {
    $cmd = $conn.CreateCommand()
    $cmd.CommandText = $sql
    $cmd.CommandTimeout = 120
    $reader = $cmd.ExecuteReader()
    $rows = New-Object System.Collections.ArrayList
    while ($reader.Read()) {
        $row = [ordered]@{}
        for ($i = 0; $i -lt $reader.FieldCount; $i++) {
            $name = $reader.GetName($i)
            $val = if ($reader.IsDBNull($i)) { $null } else { $reader.GetValue($i) }
            $row[$name] = $val
        }
        [void]$rows.Add([pscustomobject]$row)
    }
    $reader.Close()
    $rows
}

$tableQuery = @'
SELECT
    t.name                                          AS table_name,
    c.column_id                                     AS ordinal,
    c.name                                          AS column_name,
    TYPE_NAME(c.user_type_id)                       AS type_name,
    c.max_length,
    c.precision,
    c.scale,
    CAST(c.is_nullable AS INT)                      AS is_nullable,
    CAST(c.is_identity AS INT)                      AS is_identity,
    CAST(CASE WHEN ic.column_id IS NOT NULL THEN 1 ELSE 0 END AS INT) AS is_pk,
    pk.name                                         AS pk_name,
    df.definition                                   AS default_definition
FROM sys.tables t
INNER JOIN sys.columns c ON c.object_id = t.object_id
LEFT JOIN sys.key_constraints pk ON pk.parent_object_id = t.object_id AND pk.type = 'PK'
LEFT JOIN sys.index_columns ic ON ic.object_id = t.object_id AND ic.column_id = c.column_id AND pk.unique_index_id IS NOT NULL AND ic.index_id = pk.unique_index_id
LEFT JOIN sys.default_constraints df ON df.parent_object_id = t.object_id AND df.parent_column_id = c.column_id
WHERE t.is_ms_shipped = 0
ORDER BY t.name, c.column_id;
'@

$indexQuery = @'
SELECT
    OBJECT_NAME(i.object_id) AS table_name,
    i.name                   AS index_name,
    i.type_desc,
    CAST(i.is_unique AS INT)      AS is_unique,
    CAST(i.is_primary_key AS INT) AS is_primary_key,
    STUFF((SELECT ',' + c2.name
           FROM sys.index_columns ic2
           INNER JOIN sys.columns c2 ON c2.object_id = ic2.object_id AND c2.column_id = ic2.column_id
           WHERE ic2.object_id = i.object_id AND ic2.index_id = i.index_id AND ic2.is_included_column = 0
           ORDER BY ic2.key_ordinal
           FOR XML PATH('')), 1, 1, '') AS columns
FROM sys.indexes i
INNER JOIN sys.tables t ON t.object_id = i.object_id
WHERE i.type > 0 AND t.is_ms_shipped = 0;
'@

$fkQuery = @'
SELECT
    fk.name                                    AS fk_name,
    OBJECT_NAME(fk.parent_object_id)           AS parent_table,
    pc.name                                    AS parent_column,
    OBJECT_NAME(fk.referenced_object_id)       AS referenced_table,
    rc.name                                    AS referenced_column,
    fk.delete_referential_action_desc          AS delete_action,
    fk.update_referential_action_desc          AS update_action
FROM sys.foreign_keys fk
INNER JOIN sys.foreign_key_columns fkc ON fkc.constraint_object_id = fk.object_id
INNER JOIN sys.columns pc ON pc.object_id = fkc.parent_object_id AND pc.column_id = fkc.parent_column_id
INNER JOIN sys.columns rc ON rc.object_id = fkc.referenced_object_id AND rc.column_id = fkc.referenced_column_id;
'@

$viewQuery = @'
SELECT v.name AS view_name, m.definition
FROM sys.views v
INNER JOIN sys.sql_modules m ON m.object_id = v.object_id
WHERE v.is_ms_shipped = 0;
'@

$procQuery = @'
SELECT p.name AS proc_name, m.definition
FROM sys.procedures p
INNER JOIN sys.sql_modules m ON m.object_id = p.object_id
WHERE p.is_ms_shipped = 0
ORDER BY p.name;
'@

$triggerQuery = @'
SELECT tr.name AS trigger_name, OBJECT_NAME(tr.parent_id) AS parent_table, m.definition
FROM sys.triggers tr
INNER JOIN sys.sql_modules m ON m.object_id = tr.object_id
WHERE tr.is_ms_shipped = 0;
'@

foreach ($db in @('TGLOBAL_RAGEZONE','TGAME_RAGEZONE')) {
    Write-Output "=== Extracting $db ==="
    $c = Open-Conn $db
    try {
        $tbl = Read-Rows $c $tableQuery
        $tbl | Export-Csv -Path (Join-Path $outDir "$db.tables.csv") -NoTypeInformation -Encoding UTF8 -Delimiter "`t"
        Write-Output "  Tables: $(($tbl | Select-Object table_name -Unique).Count) ($($tbl.Count) columns)"

        $idx = Read-Rows $c $indexQuery
        $idx | Export-Csv -Path (Join-Path $outDir "$db.indexes.csv") -NoTypeInformation -Encoding UTF8 -Delimiter "`t"
        Write-Output "  Indexes: $($idx.Count)"

        $fks = Read-Rows $c $fkQuery
        $fks | Export-Csv -Path (Join-Path $outDir "$db.fks.csv") -NoTypeInformation -Encoding UTF8 -Delimiter "`t"
        Write-Output "  Foreign keys: $($fks.Count)"

        $triggers = Read-Rows $c $triggerQuery
        if ($triggers.Count -gt 0) {
            $sb = New-Object System.Text.StringBuilder
            foreach ($t in $triggers) {
                [void]$sb.AppendLine("-- TRIGGER: $($t.trigger_name) ON $($t.parent_table)")
                [void]$sb.AppendLine($t.definition)
                [void]$sb.AppendLine('GO')
                [void]$sb.AppendLine()
            }
            Set-Content -Path (Join-Path $outDir "$db.triggers.sql") -Value $sb.ToString() -Encoding UTF8
            Write-Output "  Triggers: $($triggers.Count)"
        }

        $views = Read-Rows $c $viewQuery
        if ($views.Count -gt 0) {
            $sb = New-Object System.Text.StringBuilder
            foreach ($v in $views) {
                [void]$sb.AppendLine("-- VIEW: $($v.view_name)")
                [void]$sb.AppendLine($v.definition)
                [void]$sb.AppendLine('GO')
                [void]$sb.AppendLine()
            }
            Set-Content -Path (Join-Path $outDir "$db.views.sql") -Value $sb.ToString() -Encoding UTF8
            Write-Output "  Views: $($views.Count)"
        }

        $procDir = Join-Path $outDir "procs\$db"
        New-Item -ItemType Directory -Force -Path $procDir | Out-Null
        Get-ChildItem -Path $procDir -Filter '*.sql' | Remove-Item -Force
        $procs = Read-Rows $c $procQuery
        foreach ($p in $procs) {
            $safe = $p.proc_name -replace '[<>:"/\\|?*]', '_'
            Set-Content -Path (Join-Path $procDir "$safe.sql") -Value $p.definition -Encoding UTF8
        }
        Write-Output "  Procs: $($procs.Count) -> $procDir"
    }
    finally {
        $c.Close()
    }
}

Write-Output ''
Write-Output 'Done.'
