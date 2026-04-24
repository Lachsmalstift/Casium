using System.Text;

namespace CasiumGUI;

public enum OutputFormat { Case = 1, Decode, Values, Json }

public sealed record MappingEntry(string Source, string Target);

public static class SqlGenerator
{
    public static string Generate(
        IList<MappingEntry> entries,
        string sourceColumn,
        string targetAlias,
        string elseValue,
        OutputFormat format) => format switch
    {
        OutputFormat.Case   => GenerateCase(entries, sourceColumn, targetAlias, elseValue),
        OutputFormat.Decode => GenerateDecode(entries, sourceColumn, targetAlias, elseValue),
        OutputFormat.Values => GenerateValues(entries, sourceColumn, targetAlias),
        OutputFormat.Json   => GenerateJson(entries, sourceColumn, targetAlias),
        _                   => throw new ArgumentOutOfRangeException(nameof(format))
    };

    private static string EscapeSql(string s)  => s.Replace("'", "''");
    private static string EscapeJson(string s) => s.Replace("\\", "\\\\").Replace("\"", "\\\"");

    private static string ElseSql(string elseValue)
        => string.IsNullOrEmpty(elseValue) ? "NULL" : $"'{EscapeSql(elseValue)}'";

    private static string GenerateCase(IList<MappingEntry> e, string src, string alias, string els)
    {
        var sb = new StringBuilder();
        sb.AppendLine("CASE");
        foreach (var m in e)
            sb.AppendLine($"  WHEN {src} = '{EscapeSql(m.Source)}' THEN '{EscapeSql(m.Target)}'");
        sb.AppendLine($"  ELSE {ElseSql(els)}");
        sb.Append($"END AS {alias};");
        return sb.ToString();
    }

    private static string GenerateDecode(IList<MappingEntry> e, string src, string alias, string els)
    {
        var sb = new StringBuilder();
        sb.AppendLine($"DECODE({src}");
        foreach (var m in e)
            sb.AppendLine($"  , '{EscapeSql(m.Source)}', '{EscapeSql(m.Target)}'");
        sb.AppendLine($"  , {ElseSql(els)}");
        sb.Append($") AS {alias}");
        return sb.ToString();
    }

    private static string GenerateValues(IList<MappingEntry> e, string src, string alias)
    {
        var sb = new StringBuilder();
        sb.AppendLine("SELECT *");
        sb.AppendLine("FROM (VALUES");
        for (int i = 0; i < e.Count; i++)
        {
            string comma = i < e.Count - 1 ? "," : "";
            sb.AppendLine($"  ('{EscapeSql(e[i].Source)}', '{EscapeSql(e[i].Target)}'){comma}");
        }
        sb.Append($") AS mapping({src}, {alias});");
        return sb.ToString();
    }

    private static string GenerateJson(IList<MappingEntry> e, string src, string alias)
    {
        var sb = new StringBuilder();
        sb.AppendLine("{");
        sb.AppendLine($"  \"quelle\": \"{EscapeJson(src)}\",");
        sb.AppendLine($"  \"ziel\": \"{EscapeJson(alias)}\",");
        sb.AppendLine("  \"mapping\": {");
        for (int i = 0; i < e.Count; i++)
        {
            string comma = i < e.Count - 1 ? "," : "";
            sb.AppendLine($"    \"{EscapeJson(e[i].Source)}\": \"{EscapeJson(e[i].Target)}\"{comma}");
        }
        sb.AppendLine("  }");
        sb.Append("}");
        return sb.ToString();
    }
}
