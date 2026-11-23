param([string]$Src, [string]$Dst, [int]$W = 1400)
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($Src)
$h = [int]($img.Height * $W / $img.Width)
$bmp = New-Object System.Drawing.Bitmap $W, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = 'HighQualityBicubic'
$g.DrawImage($img, 0, 0, $W, $h)
$bmp.Save($Dst, [System.Drawing.Imaging.ImageFormat]::Jpeg)
$g.Dispose(); $bmp.Dispose(); $img.Dispose()
$Dst
