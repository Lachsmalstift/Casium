# Changelog

## v1.2.0 — April 2026

- Feature: RFC-4180-konformer CSV-Parser — Felder mit Komma/Semikolon in Anführungszeichen werden korrekt geparst
- Feature: ELSE-Wert konfigurierbar (Enter = 'Unbekannt', 'NULL' für SQL NULL, beliebiger Text)
- Feature: Mehrere Spaltenpaare pro Durchlauf — nach jedem CASE-Ausdruck kann ein weiteres Spaltenpaar aus derselben CSV gewählt werden
- Feature: Ausgabeformat wählbar: SQL CASE, DECODE (Oracle), VALUES-Tabelle, JSON
- Feature: Konsolenvorschau auf 20 Einträge begrenzt — bei großen CSVs kein Konsolen-Flooding mehr
- Feature: Zeitstempel in fehler.log (Format: YYYY-MM-DD HH:MM:SS)
- Refactoring: CSV-Einlesen in leseKopfzeile + leseDaten aufgeteilt — Datei bleibt für mehrere Durchläufe offen

## v1.1.0 — April 2026

- Bugfix: Buffer Overflow bei CSV-Dateien mit mehr als 1000 Zeilen behoben
- Bugfix: Endlosschleife bei nicht-numerischer Spalteneingabe behoben (stdin-Flush)
- Bugfix: `scanf` / `wscanf` Mischung entfernt — einheitlich `fgetws` / `wscanf`
- Bugfix: Debug-Ausgabe `"DEBUG: Öffne CSV-Datei"` aus Produktionscode entfernt
- Verbesserung: CSV-Trennzeichen wird automatisch erkannt (Komma oder Semikolon)
- Verbesserung: Memory Leaks bei frühem Rücksprung in `leseCSV` behoben
- Verbesserung: `daten`-Array von Stack auf Heap verschoben (~500KB Stack entlastet)
- Verbesserung: `erstelleCase` schreibt nur noch in eine Ausgabe (keine gemischten Seiteneffekte)
- Verbesserung: Wide-Char-Puffer in `erstelleCase` auf 512 erweitert (passend zur Eingabegröße)
- Verbesserung: `strncpy` mit explizitem Null-Terminator abgesichert
- Refactoring: Magic Numbers durch Konstanten ersetzt (`MAX_SPALTEN`, `MAX_PFAD`)

## v1.0.0 — Juni 2024

- Erste öffentliche stabile Version
- CSV-Parser mit BOM-Unterstützung
- Interaktive Konsolensteuerung
- Prüfung auf doppelte und leere Einträge
- Fehler-Logging in Logdatei
- Mehrfachdurchläufe möglich
- UTF-8 & Unicode-Ausgabe für Windows-Konsole
- Lizenz: CC-BY 4.0
