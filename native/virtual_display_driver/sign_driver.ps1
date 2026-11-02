# Generate self-signed certificate if not already present
$certSubject = "CN=DuetView Development Driver"
$cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq $certSubject } | Select-Object -First 1

if (-not $cert) {
    Write-Host "Creating new self-signed code signing certificate..."
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $certSubject -CertStoreLocation "Cert:\CurrentUser\My" -NotAfter (Get-Date).AddYears(5)
}

$driverDir = "D:\Second Display\native\virtual_display_driver"
$cerPath = Join-Path $driverDir "SecondScreenIdd.cer"
$catPath = Join-Path $driverDir "secondscreenidd.cat"
$dllPath = Join-Path $driverDir "SecondScreenIdd.dll"

Write-Host "Exporting certificate to $cerPath..."
Export-Certificate -Cert $cert -FilePath $cerPath -Force | Out-Null

$signtool = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x64\signtool.exe"

Write-Host "Signing $dllPath..."
& $signtool sign /v /s My /n "DuetView Development Driver" /fd SHA256 $dllPath

Write-Host "Generating catalog file..."
$inf2cat = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x86\Inf2Cat.exe"
& $inf2cat /driver:$driverDir /os:10_X64

Write-Host "Signing $catPath..."
& $signtool sign /v /s My /n "DuetView Development Driver" /fd SHA256 $catPath

Write-Host "Driver signing complete!"

