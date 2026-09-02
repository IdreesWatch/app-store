param(
    [string]$Revision = '2d1fb35'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$storeRoot = Split-Path -Parent $PSScriptRoot
$repoRoot = Split-Path -Parent $storeRoot
$side = 44
$legacy = @(
    @{ id = 'calculator'; stem = 'app_calculator_112_112'; color = '#00c2ff' },
    @{ id = 'weather'; stem = 'app_weather_112_112'; color = '#ffd700' },
    @{ id = 'translate'; stem = 'app_translate_112_112'; color = '#00f0ff' },
    @{ id = 'music'; stem = 'app_music_112_112'; color = '#ff4f87' },
    @{ id = 'clock'; stem = 'app_clock_112_112'; color = '#00c2ff' },
    @{ id = 'fitness'; stem = 'app_fitness_112_112'; color = '#00ff9d' },
    @{ id = 'notifications'; stem = 'app_notifications_112_112'; color = '#ff4f87' },
    @{ id = 'assistant'; stem = 'icon_gemini_112_112'; color = '#ff4fd8' },
    @{ id = 'watchfaces'; stem = 'icon_watchfaces_112_112'; color = '#00ff9d' },
    @{ id = 'files'; stem = 'icon_file_manager_112_112'; color = '#ffb800' },
    @{ id = 'gallery'; stem = 'icon_gallery_112_112'; color = '#a78bfa' },
    @{ id = 'news'; stem = 'icon_news_112_112'; color = '#ff4f87' },
    @{ id = 'gamepad'; stem = 'icon_game_boy_112_112'; color = '#00ff9d' },
    @{ id = 'padlock'; stem = 'icon_padlock_112_112'; color = '#a78bfa' }
)

function New-TransparentBitmap([int]$width, [int]$height) {
    return [System.Drawing.Bitmap]::new(
        $width, $height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
}

function Get-LegacyBitmap([string]$stem) {
    $path = "components/assets/src/$stem.c"
    $source = (& git -C $repoRoot show "${Revision}:$path") -join "`n"
    if ($LASTEXITCODE -ne 0 -or !$source) {
        throw "Could not read $path from $Revision"
    }
    $map = [regex]::Match($source, '_map\[\]\s*=\s*\{(?<data>[\s\S]*?)\};')
    $format = [regex]::Match($source, 'LV_COLOR_FORMAT_(?<format>[A-Z0-9]+)')
    $widthMatch = [regex]::Match($source, '\.w\s*=\s*(?<value>\d+)')
    $heightMatch = [regex]::Match($source, '\.h\s*=\s*(?<value>\d+)')
    if (!$map.Success -or !$format.Success -or
        !$widthMatch.Success -or !$heightMatch.Success) {
        throw "Unrecognised LVGL asset: $path"
    }
    $bytes = [regex]::Matches($map.Groups['data'].Value, '0x([0-9a-fA-F]{2})') |
        ForEach-Object { [Convert]::ToByte($_.Groups[1].Value, 16) }
    $width = [int]$widthMatch.Groups['value'].Value
    $height = [int]$heightMatch.Groups['value'].Value
    $bitmap = New-TransparentBitmap $width $height

    if ($format.Groups['format'].Value -eq 'ARGB8888') {
        if ($bytes.Count -ne $width * $height * 4) {
            throw "$stem has $($bytes.Count) bytes, expected $($width * $height * 4)"
        }
        for ($y = 0; $y -lt $height; ++$y) {
            for ($x = 0; $x -lt $width; ++$x) {
                $offset = ($y * $width + $x) * 4
                $bitmap.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(
                    $bytes[$offset + 3], $bytes[$offset + 2],
                    $bytes[$offset + 1], $bytes[$offset]))
            }
        }
    } elseif ($format.Groups['format'].Value -eq 'RGB565') {
        if ($bytes.Count -ne $width * $height * 2) {
            throw "$stem has $($bytes.Count) bytes, expected $($width * $height * 2)"
        }
        for ($y = 0; $y -lt $height; ++$y) {
            for ($x = 0; $x -lt $width; ++$x) {
                $offset = ($y * $width + $x) * 2
                $pixel = [int]$bytes[$offset] -bor ([int]$bytes[$offset + 1] -shl 8)
                if ($pixel -eq 0) {
                    $color = [System.Drawing.Color]::Transparent
                } else {
                    $red = [Math]::Round((($pixel -shr 11) -band 31) * 255 / 31)
                    $green = [Math]::Round((($pixel -shr 5) -band 63) * 255 / 63)
                    $blue = [Math]::Round(($pixel -band 31) * 255 / 31)
                    $color = [System.Drawing.Color]::FromArgb(255, $red, $green, $blue)
                }
                $bitmap.SetPixel($x, $y, $color)
            }
        }
    } else {
        throw "Unsupported legacy format $($format.Groups['format'].Value)"
    }
    return $bitmap
}

function Resize-Contained([System.Drawing.Image]$source) {
    $result = New-TransparentBitmap $side $side
    $graphics = [System.Drawing.Graphics]::FromImage($result)
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $scale = [Math]::Min(($side - 2) / $source.Width, ($side - 2) / $source.Height)
        $width = [Math]::Max(1, [int][Math]::Round($source.Width * $scale))
        $height = [Math]::Max(1, [int][Math]::Round($source.Height * $scale))
        $x = [int](($side - $width) / 2)
        $y = [int](($side - $height) / 2)
        $graphics.DrawImage($source, $x, $y, $width, $height)
    } finally {
        $graphics.Dispose()
    }
    return $result
}

function Get-BgraBytes([System.Drawing.Bitmap]$bitmap) {
    $bytes = [byte[]]::new($side * $side * 4)
    for ($y = 0; $y -lt $side; ++$y) {
        for ($x = 0; $x -lt $side; ++$x) {
            $color = $bitmap.GetPixel($x, $y)
            $offset = ($y * $side + $x) * 4
            $bytes[$offset] = $color.B
            $bytes[$offset + 1] = $color.G
            $bytes[$offset + 2] = $color.R
            $bytes[$offset + 3] = $color.A
        }
    }
    return $bytes
}

function Write-LvglAsset([string]$path, [string]$symbol, [byte[]]$bytes,
                         [string]$sourceNote) {
    $lines = [System.Collections.Generic.List[string]]::new()
    $lines.Add('/*')
    $lines.Add(' * IdreesWatch full-colour launcher icon')
    $lines.Add(" * Source: $sourceNote")
    $lines.Add(' * Format: 44x44 LVGL ARGB8888 (BGRA byte order)')
    $lines.Add(' */')
    $lines.Add('')
    $lines.Add('#include "lvgl.h"')
    $lines.Add('')
    $lines.Add('#ifndef LV_ATTRIBUTE_MEM_ALIGN')
    $lines.Add('#define LV_ATTRIBUTE_MEM_ALIGN')
    $lines.Add('#endif')
    $lines.Add('')
    $lines.Add("LV_ATTRIBUTE_MEM_ALIGN static const uint8_t ${symbol}_map[] = {")
    for ($offset = 0; $offset -lt $bytes.Length; $offset += 16) {
        $end = [Math]::Min($offset + 16, $bytes.Length)
        $chunk = for ($index = $offset; $index -lt $end; ++$index) {
            '0x{0:x2}' -f $bytes[$index]
        }
        $lines.Add('    ' + ($chunk -join ', ') + ',')
    }
    $lines.Add('};')
    $lines.Add('')
    $lines.Add("const lv_image_dsc_t $symbol = {")
    $lines.Add('    .header = {')
    $lines.Add('        .magic = LV_IMAGE_HEADER_MAGIC,')
    $lines.Add('        .cf = LV_COLOR_FORMAT_ARGB8888,')
    $lines.Add('        .w = 44,')
    $lines.Add('        .h = 44,')
    $lines.Add('        .stride = 176,')
    $lines.Add('    },')
    $lines.Add("    .data_size = sizeof(${symbol}_map),")
    $lines.Add("    .data = ${symbol}_map,")
    $lines.Add('};')
    [System.IO.File]::WriteAllLines($path, $lines)
}

$rendered = [ordered]@{}
foreach ($entry in $legacy) {
    $source = Get-LegacyBitmap $entry.stem
    try {
        $scaled = Resize-Contained $source
        $rendered[$entry.id] = @{
            bitmap = $scaled
            color = $entry.color
        }
    } finally {
        $source.Dispose()
    }
}

$doomSource = [System.Drawing.Image]::FromFile(
    (Join-Path $repoRoot 'components/assets/source/online/doom-classic-logo.png'))
$windowsSource = [System.Drawing.Image]::FromFile(
    (Join-Path $repoRoot 'components/assets/source/online/windows-1992-2001-flag.png'))
try {
    $rendered['doom'] = @{
        bitmap = Resize-Contained $doomSource
        color = '#ff7a20'
    }
    $rendered['tiny386'] = @{
        bitmap = Resize-Contained $windowsSource
        color = '#5aa7ff'
    }
} finally {
    $doomSource.Dispose()
    $windowsSource.Dispose()
}

$packDirectory = Join-Path $storeRoot 'icon-packs/org.idreeswatch.icons.og'
New-Item -ItemType Directory -Force -Path $packDirectory | Out-Null
$icons = [ordered]@{}
foreach ($id in $rendered.Keys) {
    $icons[$id] = [ordered]@{
        color = $rendered[$id].color
        data = [Convert]::ToBase64String((Get-BgraBytes $rendered[$id].bitmap))
    }
}
$payload = [ordered]@{
    format = 'icon-pack.v1'
    pixel_format = 'argb8888'
    width = $side
    height = $side
    icons = $icons
}
[System.IO.File]::WriteAllText(
    (Join-Path $packDirectory 'icons.json'),
    (($payload | ConvertTo-Json -Depth 6 -Compress) + "`n"))

$manifest = [ordered]@{
    id = 'org.idreeswatch.icons.og'
    name = 'OG Icons'
    version = '1.0.0'
    author = 'IdreesWatch'
    summary = 'The original full-colour IdreesWatch launcher artwork.'
    kind = 'icon-pack'
    runtime = 'content'
    entry = 'icon-pack.v1'
    artifact = 'icons.json'
    icon = 'settings'
    license = 'LicenseRef-IdreesWatch-Mixed'
    min_os_api = 1
}
[System.IO.File]::WriteAllText(
    (Join-Path $packDirectory 'manifest.json'),
    (($manifest | ConvertTo-Json -Depth 4) + "`n"))

$columns = 4
$rows = [Math]::Ceiling($rendered.Count / $columns)
$preview = [System.Drawing.Bitmap]::new($columns * 72, $rows * 72)
$previewGraphics = [System.Drawing.Graphics]::FromImage($preview)
try {
    $previewGraphics.Clear([System.Drawing.Color]::FromArgb(10, 13, 20))
    $index = 0
    foreach ($id in $rendered.Keys) {
        $x = ($index % $columns) * 72 + 14
        $y = [Math]::Floor($index / $columns) * 72 + 14
        $previewGraphics.DrawImage($rendered[$id].bitmap, $x, $y, 44, 44)
        ++$index
    }
    $preview.Save((Join-Path $packDirectory 'preview.png'),
                  [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $previewGraphics.Dispose()
    $preview.Dispose()
}

$doomBytes = Get-BgraBytes $rendered['doom'].bitmap
$windowsBytes = Get-BgraBytes $rendered['tiny386'].bitmap
Write-LvglAsset `
    (Join-Path $repoRoot 'components/assets/src/icon_doom_112_112.c') `
    'icon_doom_112_112' $doomBytes `
    'classic DOOM logo downloaded from pngimg.com (id Software trademark)'
Write-LvglAsset `
    (Join-Path $repoRoot 'components/assets/src/icon_tiny386_112_112.c') `
    'icon_tiny386_112_112' $windowsBytes `
    'Microsoft Windows 1992-2001 flag from Wikimedia Commons'

foreach ($entry in $rendered.Values) {
    $entry.bitmap.Dispose()
}

Write-Output "Built OG Icons pack with $($icons.Count) icons"
