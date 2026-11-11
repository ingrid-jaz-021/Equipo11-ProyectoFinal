$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$taskRoot = Split-Path -Parent $PSScriptRoot
$imagePath = Join-Path $taskRoot 'images\OfrendaInfo.png'
$title = 'OFRENDA DE LAS MIL LUCES'
$message = 'Las luces representan la guía y el camino luminoso que ayuda a las almas a retornar desde el más allá y a transitar por las etapas rumbo al Mictlán.'
$bitmap = [System.Drawing.Bitmap]::new(1600,260,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$graphics.Clear([System.Drawing.Color]::Transparent)
$shape = [System.Drawing.Drawing2D.GraphicsPath]::new()
$shape.AddArc(4,4,48,48,180,90)
$shape.AddArc(1548,4,48,48,270,90)
$shape.AddArc(1548,208,48,48,0,90)
$shape.AddArc(4,208,48,48,90,90)
$shape.CloseFigure()
$background = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(242,10,18,31))
$gold = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255,255,215,95))
$white = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255,250,249,243))
$border = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255,205,166,63),3)
$divider = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(140,205,166,63),1)
$titleFont = [System.Drawing.Font]::new('Segoe UI',34,[System.Drawing.FontStyle]::Bold,[System.Drawing.GraphicsUnit]::Pixel)
$bodyFont = [System.Drawing.Font]::new('Segoe UI',38,[System.Drawing.FontStyle]::Regular,[System.Drawing.GraphicsUnit]::Pixel)
$format = [System.Drawing.StringFormat]::new()
$format.Trimming = [System.Drawing.StringTrimming]::None
$format.FormatFlags = [System.Drawing.StringFormatFlags]::LineLimit
try {
    $graphics.FillPath($background,$shape)
    $graphics.DrawPath($border,$shape)
    $graphics.DrawString($title,$titleFont,$gold,[System.Drawing.PointF]::new(48,24))
    $graphics.DrawLine($divider, 48, 72, 1552, 72)
    $bodyRect = [System.Drawing.RectangleF]::new(48,94,1504,150)
    $graphics.DrawString($message,$bodyFont,$white,$bodyRect,$format)
    $bitmap.Save($imagePath,[System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output "Panel ofrenda generado: $imagePath"
} finally {
    $format.Dispose();$titleFont.Dispose();$bodyFont.Dispose();$border.Dispose();$divider.Dispose()
    $background.Dispose();$gold.Dispose();$white.Dispose();$shape.Dispose();$graphics.Dispose();$bitmap.Dispose()
}

