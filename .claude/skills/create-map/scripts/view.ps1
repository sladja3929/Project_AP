param([string]$Src, [string]$Name, [string]$OutDir = $env:TEMP, [int]$W = 1400)
#CaptureViewport 결과(base64 PNG가 든 tool-results 파일)를 디코딩해 축소 JPG로 저장, JPG 경로 출력
#사용: view.ps1 <tool-results 파일> <이름> [출력 폴더(기본 TEMP, 세션 scratchpad 권장)] [가로 픽셀]
$s = Split-Path -Parent $MyInvocation.MyCommand.Path
python "$s\cap.py" $Src "$OutDir\$Name.png" | Out-Null
& "$s\shrink.ps1" "$OutDir\$Name.png" "$OutDir\$Name.jpg" $W
