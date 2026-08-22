# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Casium is a single-file, Windows-only C console application that generates SQL `CASE` (and `DECODE`/`VALUES`/JSON) expressions from CSV mapping files. It's a small personal utility distributed as a prebuilt `.exe` with an Inno Setup installer, not a project with a conventional build/test pipeline.

## Repository layout

- `sourcecode/Casium.c` — the entire application (single file, ~460 lines)
- `Casium.exe` — prebuilt Windows binary committed as the release artifact users download
- `Installer/CasiumSetup.exe` — Inno Setup installer, also committed as a release artifact
- `CasiumLogo.ico` — app icon
- `README.md` (German) — user-facing usage docs
- `Changelog.md` (German) — version history; update when the source changes
- `Lizenz.md` — CC BY 4.0 license text

## Build

There is no Makefile, build script, or CI in this repo. `sourcecode/Casium.c` is compiled manually on Windows and the resulting binary is committed directly as `Casium.exe`. Given the Windows-only APIs it uses (`<windows.h>`, `_wfopen`, `_setmbcp`, `wscanf`/`fgetws`, `MessageBoxW`), it targets the MSVC/Windows CRT and cannot be built or run as-is on Linux/macOS.

To compile on Windows with MSVC:

```
cl sourcecode\Casium.c /Fe:Casium.exe
```

There are no automated tests. Verify changes by compiling and running the console app manually against a sample CSV. If working from a non-Windows environment, you can read/edit the C source but cannot compile or execute it — say so rather than claiming to have tested a change.

## Architecture

Everything lives in `sourcecode/Casium.c`, organized top to bottom:

1. **Helpers**: `logFehler` for error handling (writes a timestamped line to `fehler.log` *and* pops a Windows `MessageBox`), CP1252→wide-char conversion, apostrophe/JSON escaping for generated SQL/JSON strings, and delimiter auto-detection (`erkenneTrenner`).
2. **CSV parsing**: `parseCSVZeile` is a hand-rolled RFC-4180 parser (quoted fields, escaped `""` quotes, comma/semicolon delimiters). `leseKopfzeile` reads and parses the header row and leaves the file open; `leseDaten` then reads data rows for the chosen source/target column indices into a `Zuordnung` (mapping) array, skipping empty and duplicate entries.
3. **Output generation**: one function per format — `schreibeCase`, `schreibeDecode`, `schreibeValues`, `schreibeJSON` — sharing the same `(daten, n, orig, alias, ..., out, lim)` signature so the same code path writes both the full file output (`lim=0`) and the truncated console preview (`lim=MAX_VORSCHAU`). `erstelleAusgabe` dispatches by the `AusgabeFormat` enum; `speichereAusgabe` writes the output file, then prints the preview.
4. **`main`**: an interactive REPL-style loop — prompt for a CSV path → parse header → inner loop lets the user repeatedly pick a source/target column pair, alias, output format, and ELSE value against the same open file (multiple CASE expressions per CSV) → outer loop offers to process another CSV file.

Key conventions to follow when touching this code:

- Identifiers and user-facing strings are German throughout (`leseDaten`, `erstelleAusgabe`, `Weiteres Spaltenpaar?`) — match this style for new code and keep console output in German.
- All I/O goes through wide-char APIs (`wprintf`/`fwprintf`/`fgetws`) with explicit CP1252/UTF-8 conversions; input is read as Windows-1252 (`_setmbcp(_MB_CP_1252)`) and the console runs in wide-text mode (`_O_WTEXT`).
- Buffers are fixed-size (`MAX_ZEILE`, `MAX_DATEN`, `MAX_SPALTEN`, `MAX_PFAD`, `MAX_VORSCHAU`) with the mapping array (`daten`) heap-allocated. This codebase has a history of buffer-overflow and stack-size bugs (see `Changelog.md` v1.1.0) fixed by adding bounds checks and moving large arrays off the stack — preserve those checks when editing parsing/buffer code.
- New error paths should go through `logFehler` (MessageBox + `fehler.log` entry) rather than printing to stderr directly.
- Bump `CASIUM_VERSION`/`CASIUM_AUTOR` in `Casium.c` and add a corresponding `Changelog.md` entry when shipping a versioned change.
