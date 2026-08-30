param(
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\app\assets\douyu_monitor.ico')
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
    private static GraphicsPath RoundedRectangle(RectangleF bounds, float radius)
    {
        var path = new GraphicsPath();
        var diameter = radius * 2f;
        path.AddArc(bounds.X, bounds.Y, diameter, diameter, 180, 90);
        path.AddArc(bounds.Right - diameter, bounds.Y, diameter, diameter, 270, 90);
        path.AddArc(bounds.Right - diameter, bounds.Bottom - diameter, diameter, diameter, 0, 90);
        path.AddArc(bounds.X, bounds.Bottom - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();
        return path;
    }

    private static Bitmap CreateBitmap(int size)
    {
        var bitmap = new Bitmap(size, size, PixelFormat.Format32bppArgb);
        using (var graphics = Graphics.FromImage(bitmap))
        {
            graphics.SmoothingMode = SmoothingMode.AntiAlias;
            graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
            graphics.Clear(Color.Transparent);

            float scale = size / 256f;
            Func<float, float> value = unit => unit * scale;
            using (var background = RoundedRectangle(new RectangleF(value(12), value(12), value(232), value(232)), value(58)))
            using (var brush = new SolidBrush(Color.FromArgb(242, 117, 34)))
                graphics.FillPath(brush, background);

            using (var border = RoundedRectangle(new RectangleF(value(24), value(24), value(208), value(208)), value(46)))
            using (var pen = new Pen(Color.FromArgb(255, 196, 155), value(12)))
                graphics.DrawPath(pen, border);

            using (var screen = RoundedRectangle(new RectangleF(value(57), value(71), value(142), value(106)), value(24)))
            using (var pen = new Pen(Color.White, value(14)))
                graphics.DrawPath(pen, screen);

            using (var white = new SolidBrush(Color.White))
            {
                graphics.FillEllipse(white, value(84), value(102), value(20), value(20));
                graphics.FillEllipse(white, value(152), value(102), value(20), value(20));
                using (var signal = RoundedRectangle(new RectangleF(value(93), value(138), value(70), value(12)), value(6)))
                    graphics.FillPath(white, signal);
                using (var baseLine = RoundedRectangle(new RectangleF(value(105), value(190), value(46), value(12)), value(6)))
                    graphics.FillPath(white, baseLine);
            }
        }
        return bitmap;
    }

    public static void Write(string outputPath)
    {
        int[] sizes = { 16, 24, 32, 48, 64, 128, 256 };
        var pngLayers = new byte[sizes.Length][];
        for (int index = 0; index < sizes.Length; index++)
        {
            using (var bitmap = CreateBitmap(sizes[index]))
            using (var stream = new MemoryStream())
            {
                bitmap.Save(stream, ImageFormat.Png);
                pngLayers[index] = stream.ToArray();
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

[DouyuMonitorIconGenerator]::Write((Resolve-Path -LiteralPath (Split-Path -Parent $OutputPath)).Path + '\\' + (Split-Path -Leaf $OutputPath))
