param(
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\app\assets\douyu_monitor.ico'),
    [string]$SourcePath = (Join-Path $PSScriptRoot '..\app\qml\assets\brand\icon-app.png')
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies 'C:\Windows\Microsoft.NET\Framework64\v4.0.30319\System.Drawing.dll' -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

public static class DouyuMonitorIconGenerator
{
    private static Bitmap CreateBitmap(Image source, int size)
    {
        var bitmap = new Bitmap(size, size, PixelFormat.Format32bppArgb);
        using (var graphics = Graphics.FromImage(bitmap))
        {
            graphics.CompositingMode = CompositingMode.SourceCopy;
            graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
            graphics.PixelOffsetMode = PixelOffsetMode.HighQuality;
            graphics.Clear(Color.Transparent);
            // Preserve the approved artwork and alpha without adding a matte.
            using (var attributes = new ImageAttributes())
            {
                attributes.SetWrapMode(WrapMode.TileFlipXY);
                graphics.DrawImage(source, new Rectangle(0, 0, size, size),
                    0, 0, source.Width, source.Height, GraphicsUnit.Pixel, attributes);
            }
        }
        return bitmap;
    }

    public static void Write(string sourcePath, string outputPath)
    {
        int[] sizes = { 16, 24, 32, 48, 64, 128, 256 };
        var pngLayers = new byte[sizes.Length][];
        using (var source = Image.FromFile(sourcePath))
        {
            if (source.Width != source.Height)
                throw new ArgumentException("The source icon must have a square canvas.");
            if (!Image.IsAlphaPixelFormat(source.PixelFormat))
                throw new ArgumentException("Use the approved transparent brand PNG as the source.");
            for (int index = 0; index < sizes.Length; index++)
            {
                using (var bitmap = CreateBitmap(source, sizes[index]))
                using (var stream = new MemoryStream())
                {
                    bitmap.Save(stream, ImageFormat.Png);
                    pngLayers[index] = stream.ToArray();
                }
            }
        }

        Directory.CreateDirectory(Path.GetDirectoryName(outputPath));
        using (var stream = File.Create(outputPath))
        using (var writer = new BinaryWriter(stream))
        {
            writer.Write((ushort)0);
            writer.Write((ushort)1);
            writer.Write((ushort)sizes.Length);
            int offset = 6 + (16 * sizes.Length);
            for (int index = 0; index < sizes.Length; index++)
            {
                int size = sizes[index];
                writer.Write((byte)(size == 256 ? 0 : size));
                writer.Write((byte)(size == 256 ? 0 : size));
                writer.Write((byte)0);
                writer.Write((byte)0);
                writer.Write((ushort)1);
                writer.Write((ushort)32);
                writer.Write(pngLayers[index].Length);
                writer.Write(offset);
                offset += pngLayers[index].Length;
            }
            for (int index = 0; index < pngLayers.Length; index++) writer.Write(pngLayers[index]);
        }
    }
}
'@

$sourceFile = (Resolve-Path -LiteralPath $SourcePath).Path
$outputFile = [System.IO.Path]::GetFullPath($OutputPath)
if ([string]::Equals($sourceFile, $outputFile, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'SourcePath and OutputPath must be different files.'
}
[DouyuMonitorIconGenerator]::Write($sourceFile, $outputFile)
Write-Output "Generated CSTG application icon: $outputFile"
