$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$taskRoot = Split-Path -Parent $PSScriptRoot
function New-DialoguePanel($file,$message,$heading) {
$imagePath = Join-Path $taskRoot ('images\'+$file)
$bitmap = [System.Drawing.Bitmap]::new(1600,340,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$graphics.Clear([System.Drawing.Color]::Transparent)
$shape = [System.Drawing.Drawing2D.GraphicsPath]::new()
$shape.AddArc(2,2,52,52,180,90)
$shape.AddArc(1545,2,52,52,270,90)
$shape.AddArc(1545,285,52,52,0,90)
$shape.AddArc(2,285,52,52,90,90)
$shape.CloseFigure()
$background = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(242,10,18,31))
$gold = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255,255,215,95))
$white = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255,250,249,243))
$border = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(255,205,166,63),3)
$titleFont = [System.Drawing.Font]::new('Segoe UI',36,[System.Drawing.FontStyle]::Bold,[System.Drawing.GraphicsUnit]::Pixel)
$bodyFont = [System.Drawing.Font]::new('Segoe UI',44,[System.Drawing.FontStyle]::Regular,[System.Drawing.GraphicsUnit]::Pixel)
$format = [System.Drawing.StringFormat]::new()
$format.Trimming = [System.Drawing.StringTrimming]::None
$format.FormatFlags = [System.Drawing.StringFormatFlags]::LineLimit
try {
    $graphics.FillPath($background,$shape)
    $graphics.DrawPath($border,$shape)
    $graphics.DrawString($heading,$titleFont,$gold,[System.Drawing.PointF]::new(44,25))
    $bodyRect = [System.Drawing.RectangleF]::new(44,102,1512,202)
    $measured = $graphics.MeasureString($message,$bodyFont,[System.Drawing.SizeF]::new(1512,202),$format)
    if ($measured.Height -gt 202) { throw 'El dialogo no cabe completo en el panel.' }
    $graphics.DrawString($message,$bodyFont,$white,$bodyRect,$format)
    $bitmap.Save($imagePath,[System.Drawing.Imaging.ImageFormat]::Png)
    Write-Output "Dialogo completo: $imagePath"
} finally {
    $format.Dispose();$titleFont.Dispose();$bodyFont.Dispose();$border.Dispose()
    $background.Dispose();$gold.Dispose();$white.Dispose();$shape.Dispose();$graphics.Dispose();$bitmap.Dispose()
}

}
New-DialoguePanel 'AxoloteDialogo0.png' 'Acércate y presiona G para conversar con el axolote.' 'FESTIVAL DE LA LUZ Y DE LA VIDA · G: HABLAR'
New-DialoguePanel 'AxoloteDialogo1.png' '¡Bienvenidos a Chignahuapan! Acércate, estamos celebrando el Festival de la Luz y de la Vida.' 'AXOLOTE · 1 / 3 · G: CONTINUAR'
New-DialoguePanel 'AxoloteDialogo2.png' 'Esta noche, iluminamos el camino de las almas con antorchas desde el centro hasta la laguna.' 'AXOLOTE · 2 / 3 · G: CONTINUAR'
New-DialoguePanel 'AxoloteDialogo3.png' 'Sigue el pasillo hacia el agua para presenciar el ritual de la purificación. ¡Disfruta el recorrido!' 'AXOLOTE · 3 / 3 · G: CERRAR'
