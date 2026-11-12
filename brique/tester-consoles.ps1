# Fait jouer des jeux Berry sur les consoles branchees (firmware c3-berry) : le meme jeu part sur toutes en meme temps (multi),
# boutons simules par le cable (BTN), mesures STAT chaque seconde. Une ligne JSON par jeu et par console.
# powershell -File tester-consoles.ps1 bench\ext\medium\x2-1.be [autres.be...] [-Secondes 20] [-Ports COM12,COM23]
# Le jeu est garde sur la console sous le nom "bench" (ecrase a chaque essai : une seule place prise).
[CmdletBinding(PositionalBinding = $false)]           # tous les noms de fichiers vont dans $Jeux (pas dans -Ports)
param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Jeux, [int]$Secondes = 20, [string[]]$Ports)
$ErrorActionPreference = 'Stop'
if (-not $Ports) { $Ports = Get-CimInstance Win32_PnPEntity | Where-Object { $_.DeviceID -like 'USB\VID_303A*' } | ForEach-Object { if ($_.Name -match '\((COM\d+)\)') { $Matches[1] } } }   # ESP32-C3 en USB natif
$sp = foreach ($c in $Ports) { $p = New-Object System.IO.Ports.SerialPort $c, 115200; $p.DtrEnable = $false; $p.RtsEnable = $false; $p.Open(); $p }   # DTR/RTS bas : pas de redemarrage
$dire = { param($t) foreach ($p in $sp) { $p.Write("$t`n") } }
# boutons G D P3 au fil du temps : rien, P3 tape (menu / pret), P3 tenu, P3 + G, P3 + D, P3 tape, G seul, D seul
$plan = @(@(0, '000'), @(2, '001'), @(2.3, '000'), @(4, '001'), @(9, '101'), @(12, '011'), @(15, '000'), @(16, '001'), @(16.3, '000'), @(17, '100'), @(18.5, '010'), @(19.5, '000'))
# jeu a plusieurs (envoie / recois) : une sonde compte les messages radio envoyes / recus et les ecrit chaque seconde ("RADIO e r")
$sonde = "`n# --- sonde du banc ---`nvar _rc0 = recois var _en0 = envoie var _nr = 0 var _ne = 0 var _tt = 0.0 var _m0 = maj`ndef recois() var l = _rc0() _nr += size(l) return l end`ndef envoie(s) _ne += 1 return _en0(s) end`ndef maj(dt) _m0(dt) _tt += dt if _tt >= 1 _tt = 0.0 print('RADIO ' + str(_ne) + ' ' + str(_nr)) end end`n"
$planMulti = @(@(0, '000'), @(1, '010'), @(1.3, '000'), @(2, '001'), @(2.3, '000'), @(3, '001'), @(3.3, '000'), @(4, '001'), @(9, '101'), @(12, '011'), @(15, '000'), @(16, '001'), @(16.3, '000'), @(17, '100'), @(18.5, '010'), @(19.5, '000'))   # D (menu : 2 joueurs ?), P3, P3 (pret), puis comme seul
foreach ($jeu in $Jeux) {
  $src = [IO.File]::ReadAllText((Resolve-Path $jeu)); $multi = $src -match '\b(recois|envoie)\s*\('
  $octets = [Text.Encoding]::UTF8.GetBytes($(if ($multi) { $src + $sonde } else { $src })); $pl = if ($multi) { $planMulti } else { $plan }
  foreach ($p in $sp) { $p.DiscardInBuffer() }
  & $dire 'BTN 000'; & $dire 'STAT 1'
  & $dire "SCRIPT $($octets.Length) bench"
  for ($k = 0; $k -lt $octets.Length; $k += 256) { foreach ($p in $sp) { $p.Write($octets, $k, [Math]::Min(256, $octets.Length - $k)) }; Start-Sleep -Milliseconds 10 }   # par 256 octets comme la page : d'un bloc, la console en perd
  $lu = @{}; foreach ($p in $sp) { $lu[$p.PortName] = '' }
  $t0 = [DateTime]::Now; $i = 0
  while (($t = ([DateTime]::Now - $t0).TotalSeconds) -lt $Secondes) {
    while ($i -lt $pl.Count -and $pl[$i][0] -le $t) { & $dire ('BTN ' + $pl[$i][1]); $i++ }
    foreach ($p in $sp) { $lu[$p.PortName] += $p.ReadExisting() }
    Start-Sleep -Milliseconds 50
  }
  & $dire 'BTN 000'; & $dire 'STAT 0'
  foreach ($p in $sp) {
    $l = $lu[$p.PortName] -split "`r?`n" | Where-Object { $_ }
    $charge = $l | Where-Object { $_ -match '^(OK|ERR) ' } | Select-Object -First 1
    $st = $l | Where-Object { $_ -match '^STAT ' }
    $ims = @($st | ForEach-Object { if ($_ -match '^STAT (\d+) img/s') { [int]$Matches[1] } } | Sort-Object)
    $tas = @($st | ForEach-Object { if ($_ -match 'tas (\d+)') { [int]$Matches[1] } } | Sort-Object)
    [pscustomobject]@{ jeu = $jeu; console = $p.PortName; charge = "$charge"
      erreurs = @($l | Where-Object { $_ -match '^ERR ' -and $_ -ne $charge } | Select-Object -Unique -First 3) -join ' | '
      img_s = if ($ims.Count) { $ims[[int]($ims.Count / 2)] } else { $null }; img_s_min = if ($ims.Count) { $ims[0] } else { $null }
      tas_min = if ($tas.Count) { $tas[0] } else { $null }
      radio = if ($multi) { "$($l | Where-Object { $_ -match '^RADIO ' } | Select-Object -Last 1)" -replace '^RADIO ', '' } else { $null } } | ConvertTo-Json -Compress   # "envoyes recus" en fin d'essai   # la radio, une fois allumee, garde ~50 Ko jusqu'au redemarrage : comparer au tas d'un jeu vide dans le meme etat
  }
}
foreach ($p in $sp) { $p.Close() }
