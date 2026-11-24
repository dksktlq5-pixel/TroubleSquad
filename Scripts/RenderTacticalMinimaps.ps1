param([string[]]$Names=@('Lab'))
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
foreach($name in $Names){
 $path=Join-Path $PSScriptRoot "../ArtSource/TacticalMinimaps/$name.json"
 $data=Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
 $bitmap=[System.Drawing.Bitmap]::new([int]$data.size,[int]$data.size)
 $g=[System.Drawing.Graphics]::FromImage($bitmap)
 $g.SmoothingMode=[System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
 $g.Clear([System.Drawing.ColorTranslator]::FromHtml($data.background))
 foreach($o in $data.operations){
  $color=[System.Drawing.ColorTranslator]::FromHtml($o.color)
  $brush=New-Object System.Drawing.SolidBrush($color)
  if($o.kind -eq 'ellipse'){$r=$o.rect;$g.FillEllipse($brush,[single]$r[0],[single]$r[1],[single]$r[2],[single]$r[3])}
  else {
   [System.Drawing.PointF[]]$pts=@($o.points | ForEach-Object {New-Object System.Drawing.PointF([single]$_[0],[single]$_[1])})
   if($o.kind -eq 'line'){$pen=New-Object System.Drawing.Pen($color,[single]$o.width);$g.DrawLines($pen,$pts);$pen.Dispose()}
   else {$g.FillPolygon($brush,$pts);if($o.stroke -and $o.width -gt 0){$pen=New-Object System.Drawing.Pen([System.Drawing.ColorTranslator]::FromHtml($o.stroke),[single]$o.width);$g.DrawPolygon($pen,$pts);$pen.Dispose()}}
  }
  $brush.Dispose()
 }
 $bitmap.Save([System.IO.Path]::ChangeExtension([System.IO.Path]::GetFullPath($path),'.png'),[System.Drawing.Imaging.ImageFormat]::Png)
 $g.Dispose();$bitmap.Dispose();Write-Output "RASTERIZED $name"
}
