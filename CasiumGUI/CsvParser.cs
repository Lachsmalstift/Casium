using System.Text;

namespace CasiumGUI;

public sealed class CsvRecord
{
    public string[] Fields { get; }
    public CsvRecord(string[] fields) => Fields = fields;
}

public sealed class CsvParser
{
    public char       Separator { get; private set; }
    public string[]   Headers   { get; private set; } = [];
    public List<CsvRecord> Records { get; private set; } = [];

    public void Parse(string filePath)
    {
        var encoding = DetectEncoding(filePath);
        var lines = File.ReadAllLines(filePath, encoding);
        if (lines.Length == 0) throw new InvalidDataException("Datei ist leer.");

        string header = lines[0].TrimStart('\uFEFF'); // BOM entfernen
        Separator = DetectSeparator(header);
        Headers = ParseLine(header, Separator);
        if (Headers.Length < 2)
            throw new InvalidDataException("Zu wenige Spalten in der Kopfzeile.");

        Records = new List<CsvRecord>(lines.Length);
        for (int i = 1; i < lines.Length; i++)
        {
            if (string.IsNullOrWhiteSpace(lines[i])) continue;
            Records.Add(new CsvRecord(ParseLine(lines[i], Separator)));
        }
    }

    private static Encoding DetectEncoding(string path)
    {
        using var fs = new FileStream(path, FileMode.Open, FileAccess.Read);
        Span<byte> bom = stackalloc byte[3];
        fs.Read(bom);
        return (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF)
            ? new UTF8Encoding(encoderShouldEmitUTF8Identifier: true)
            : Encoding.GetEncoding(1252);
    }

    private static char DetectSeparator(string line)
    {
        int commas = 0, semis = 0;
        foreach (char c in line)
        {
            if (c == ',') commas++;
            else if (c == ';') semis++;
        }
        return semis >= commas ? ';' : ',';
    }

    // RFC-4180-konformer Parser: behandelt "Felder, mit Komma" und escaped ""Quotes""
    public static string[] ParseLine(string line, char sep)
    {
        var fields = new List<string>();
        int i = 0;
        while (i <= line.Length)
        {
            if (i == line.Length) { fields.Add(""); break; }
            var sb = new StringBuilder();
            if (line[i] == '"')
            {
                i++;
                while (i < line.Length)
                {
                    if (line[i] == '"')
                    {
                        if (i + 1 < line.Length && line[i + 1] == '"') { sb.Append('"'); i += 2; }
                        else { i++; break; }
                    }
                    else sb.Append(line[i++]);
                }
            }
            else
            {
                while (i < line.Length && line[i] != sep) sb.Append(line[i++]);
            }
            fields.Add(sb.ToString());
            if (i < line.Length && line[i] == sep) i++;
            else break;
        }
        return [.. fields];
    }
}
