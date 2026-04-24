#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <locale.h>
#include <fcntl.h>
#include <io.h>
#include <mbctype.h>
#include <mbstring.h>
#include <windows.h>

#ifndef _O_WTEXT
#define _O_WTEXT 0x4000
#endif
#ifndef _MB_CP_1252
#define _MB_CP_1252 1252
#endif

#define MAX_ZEILE     1024
#define MAX_DATEN     1000
#define MAX_SPALTEN   100
#define MAX_PFAD      512
#define MAX_VORSCHAU  20
#define CASIUM_VERSION "v1.3.0"
#define CASIUM_AUTOR   "Entwickelt von Christopher Lane Charles Dentmon"

typedef struct { char quelle[256]; char ziel[256]; } Zuordnung;
typedef enum { FORMAT_CASE = 1, FORMAT_DECODE, FORMAT_VALUES, FORMAT_JSON } AusgabeFormat;

// ============================================================
//  Hilfsfunktionen
// ============================================================

void logFehler(const wchar_t *nachricht) {
    fwprintf(stderr, L"FEHLER: %s\n", nachricht);
    MessageBoxW(NULL, nachricht, L"Fehler", MB_OK | MB_ICONERROR);
    FILE *log = _wfopen(L"fehler.log", L"a, ccs=UTF-8");
    if (log) {
        time_t t = time(NULL);
        char zeitbuf[32];
        strftime(zeitbuf, sizeof(zeitbuf), "%Y-%m-%d %H:%M:%S", localtime(&t));
        fwprintf(log, L"[%hs] FEHLER: %s\n", zeitbuf, nachricht);
        fclose(log);
    }
}

void konvertiere1252ZuWide(const char *input, wchar_t *output, int size) {
    MultiByteToWideChar(CP_ACP, 0, input, -1, output, size);
}

static void maskiereApostrophe(const char *ein, char *aus, size_t max) {
    size_t pos = 0;
    for (size_t i = 0; ein[i] && pos + 1 < max; ++i) {
        if (ein[i] == '\'') {
            if (pos + 2 < max) { aus[pos++] = '\''; aus[pos++] = '\''; }
        } else {
            aus[pos++] = ein[i];
        }
    }
    aus[pos] = '\0';
}

static void maskiereJSON(const char *ein, char *aus, size_t max) {
    size_t pos = 0;
    for (size_t i = 0; ein[i] && pos + 1 < max; ++i) {
        char c = ein[i];
        if (c == '"' || c == '\\') {
            if (pos + 2 < max) { aus[pos++] = '\\'; aus[pos++] = c; }
        } else {
            aus[pos++] = c;
        }
    }
    aus[pos] = '\0';
}

static void leereStdin(void) {
    wint_t c;
    while ((c = fgetwc(stdin)) != L'\n' && c != WEOF);
}

static char erkenneTrenner(const char *kopfzeile) {
    int kommas = 0, semis = 0;
    for (const char *p = kopfzeile; *p; p++) {
        if (*p == ',')      kommas++;
        else if (*p == ';') semis++;
    }
    return (semis >= kommas) ? ';' : ',';
}

static void trimChar(char *s) {
    char *start = s;
    while (isspace((unsigned char)*start)) start++;
    memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) s[--len] = '\0';
}

// ============================================================
//  RFC-4180-konformer CSV-Parser
// ============================================================
static int parseCSVZeile(const char *zeile, char sep, char felder[][MAX_ZEILE], int maxFelder) {
    int n = 0;
    const char *p = zeile;
    while (*p && *p != '\r' && *p != '\n' && n < maxFelder) {
        char *dst = felder[n];
        int pos = 0;
        if (*p == '"') {
            p++;
            while (*p) {
                if (*p == '"') {
                    if (*(p+1) == '"') { if (pos < MAX_ZEILE-1) dst[pos++] = '"'; p += 2; }
                    else { p++; break; }
                } else {
                    if (pos < MAX_ZEILE-1) dst[pos++] = *p;
                    p++;
                }
            }
        } else {
            while (*p && *p != sep && *p != '\r' && *p != '\n') {
                if (pos < MAX_ZEILE-1) dst[pos++] = *p;
                p++;
            }
        }
        dst[pos] = '\0';
        n++;
        if (*p == sep) p++;
        else break;
    }
    return n;
}

// ============================================================
//  Clipboard
// ============================================================

// Liest die gespeicherte UTF-8-Datei zurück und übergibt sie an die Zwischenablage
static void kopiereInZwischenablage(const wchar_t *pfad) {
    FILE *f = _wfopen(pfad, L"rb");
    if (!f) { logFehler(L"Zwischenablage: Ausgabedatei konnte nicht gelesen werden."); return; }

    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    rewind(f);

    char *utf8 = malloc(bytes + 1);
    if (!utf8) { fclose(f); return; }
    fread(utf8, 1, bytes, f);
    utf8[bytes] = '\0';
    fclose(f);

    // UTF-8 BOM überspringen falls vorhanden
    char *src = utf8;
    if (bytes >= 3 && (unsigned char)src[0]==0xEF && (unsigned char)src[1]==0xBB && (unsigned char)src[2]==0xBF)
        src += 3;

    // UTF-8 → UTF-16 für Windows Clipboard
    int wlen = MultiByteToWideChar(CP_UTF8, 0, src, -1, NULL, 0);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
    if (!hMem) { free(utf8); return; }

    wchar_t *wbuf = (wchar_t *)GlobalLock(hMem);
    MultiByteToWideChar(CP_UTF8, 0, src, -1, wbuf, wlen);
    GlobalUnlock(hMem);
    free(utf8);

    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        SetClipboardData(CF_UNICODETEXT, hMem); // Clipboard übernimmt Ownership von hMem
        CloseClipboard();
        wprintf(L"In Zwischenablage kopiert.\n");
    } else {
        GlobalFree(hMem);
        logFehler(L"Zwischenablage konnte nicht geöffnet werden.");
    }
}

// ============================================================
//  CSV-Lesen
// ============================================================

static int leseKopfzeile(FILE *datei, char *sep_out, char spalten[][256], int *nSpalten) {
    char zeile[MAX_ZEILE];
    if (!fgets(zeile, MAX_ZEILE, datei)) {
        logFehler(L"Kopfzeile konnte nicht gelesen werden.");
        return 1;
    }
    if ((unsigned char)zeile[0]==0xEF && (unsigned char)zeile[1]==0xBB && (unsigned char)zeile[2]==0xBF)
        memmove(zeile, zeile+3, strlen(zeile+3)+1);

    *sep_out = erkenneTrenner(zeile);

    char felder[MAX_SPALTEN][MAX_ZEILE];
    int n = parseCSVZeile(zeile, *sep_out, felder, MAX_SPALTEN);
    if (n < 2) { logFehler(L"Zu wenige Spalten gefunden."); return 1; }

    *nSpalten = n;
    for (int i = 0; i < n; i++) {
        strncpy(spalten[i], felder[i], 255);
        spalten[i][255] = '\0';
    }
    return 0;
}

static void leseDaten(FILE *datei, char sep, int idxQ, int idxZ,
                      Zuordnung *daten, int *anzahl, int *leere, int *doppelte) {
    *anzahl = *leere = *doppelte = 0;
    char zeile[MAX_ZEILE];
    char felder[MAX_SPALTEN][MAX_ZEILE];
    while (fgets(zeile, MAX_ZEILE, datei) && *anzahl < MAX_DATEN) {
        int n = parseCSVZeile(zeile, sep, felder, MAX_SPALTEN);
        if (n <= idxQ || n <= idxZ) continue;
        if (!*felder[idxQ] || !*felder[idxZ]) { (*leere)++; continue; }
        int dup = 0;
        for (int i = 0; i < *anzahl; i++) {
            if (strcmp(daten[i].quelle, felder[idxQ]) == 0) { dup = 1; break; }
        }
        if (dup) { (*doppelte)++; continue; }
        strncpy(daten[*anzahl].quelle, felder[idxQ], 255); daten[*anzahl].quelle[255] = '\0';
        strncpy(daten[*anzahl].ziel,   felder[idxZ], 255); daten[*anzahl].ziel[255]   = '\0';
        (*anzahl)++;
    }
}

// ============================================================
//  Ausgabe-Generierung
// ============================================================

static void schreibeCase(const Zuordnung *d, int n, const char *orig, const char *alias,
                         const char *elseWert, FILE *out, int lim) {
    wchar_t wOrig[256], wAlias[256];
    konvertiere1252ZuWide(orig, wOrig, 256); konvertiere1252ZuWide(alias, wAlias, 256);
    int zeige = (lim > 0 && n > lim) ? lim : n;
    fwprintf(out, L"CASE\n");
    for (int i = 0; i < zeige; i++) {
        char q[512], z[512]; wchar_t wq[512], wz[512];
        maskiereApostrophe(d[i].quelle, q, sizeof(q)); maskiereApostrophe(d[i].ziel, z, sizeof(z));
        konvertiere1252ZuWide(q, wq, 512); konvertiere1252ZuWide(z, wz, 512);
        fwprintf(out, L"  WHEN %s = '%s' THEN '%s'\n", wOrig, wq, wz);
    }
    if (lim > 0 && n > lim) fwprintf(out, L"  ... (%d weitere WHEN-Bedingungen)\n", n - lim);
    if (*elseWert == '\0') {
        fwprintf(out, L"  ELSE NULL\nEND AS %s;\n", wAlias);
    } else {
        char esc[512]; wchar_t wElse[512];
        maskiereApostrophe(elseWert, esc, sizeof(esc)); konvertiere1252ZuWide(esc, wElse, 512);
        fwprintf(out, L"  ELSE '%s'\nEND AS %s;\n", wElse, wAlias);
    }
}

static void schreibeDecode(const Zuordnung *d, int n, const char *orig, const char *alias,
                            const char *elseWert, FILE *out, int lim) {
    wchar_t wOrig[256], wAlias[256];
    konvertiere1252ZuWide(orig, wOrig, 256); konvertiere1252ZuWide(alias, wAlias, 256);
    int zeige = (lim > 0 && n > lim) ? lim : n;
    fwprintf(out, L"DECODE(%s\n", wOrig);
    for (int i = 0; i < zeige; i++) {
        char q[512], z[512]; wchar_t wq[512], wz[512];
        maskiereApostrophe(d[i].quelle, q, sizeof(q)); maskiereApostrophe(d[i].ziel, z, sizeof(z));
        konvertiere1252ZuWide(q, wq, 512); konvertiere1252ZuWide(z, wz, 512);
        fwprintf(out, L"  , '%s', '%s'\n", wq, wz);
    }
    if (lim > 0 && n > lim) fwprintf(out, L"  ... (%d weitere)\n", n - lim);
    if (*elseWert == '\0') {
        fwprintf(out, L"  , NULL\n) AS %s\n", wAlias);
    } else {
        char esc[512]; wchar_t wElse[512];
        maskiereApostrophe(elseWert, esc, sizeof(esc)); konvertiere1252ZuWide(esc, wElse, 512);
        fwprintf(out, L"  , '%s'\n) AS %s\n", wElse, wAlias);
    }
}

static void schreibeValues(const Zuordnung *d, int n, const char *orig, const char *alias,
                            FILE *out, int lim) {
    wchar_t wOrig[256], wAlias[256];
    konvertiere1252ZuWide(orig, wOrig, 256); konvertiere1252ZuWide(alias, wAlias, 256);
    int zeige = (lim > 0 && n > lim) ? lim : n;
    int hatMehr = (lim > 0 && n > lim);
    fwprintf(out, L"SELECT *\nFROM (VALUES\n");
    for (int i = 0; i < zeige; i++) {
        char q[512], z[512]; wchar_t wq[512], wz[512];
        maskiereApostrophe(d[i].quelle, q, sizeof(q)); maskiereApostrophe(d[i].ziel, z, sizeof(z));
        konvertiere1252ZuWide(q, wq, 512); konvertiere1252ZuWide(z, wz, 512);
        const wchar_t *komma = (i < zeige-1 || hatMehr) ? L"," : L"";
        fwprintf(out, L"  ('%s', '%s')%s\n", wq, wz, komma);
    }
    if (hatMehr) fwprintf(out, L"  -- ... (%d weitere)\n", n - lim);
    fwprintf(out, L") AS mapping(%s, %s);\n", wOrig, wAlias);
}

static void schreibeJSON(const Zuordnung *d, int n, const char *orig, const char *alias,
                          FILE *out, int lim) {
    wchar_t wOrig[256], wAlias[256];
    konvertiere1252ZuWide(orig, wOrig, 256); konvertiere1252ZuWide(alias, wAlias, 256);
    int zeige = (lim > 0 && n > lim) ? lim : n;
    int hatMehr = (lim > 0 && n > lim);
    fwprintf(out, L"{\n  \"quelle\": \"%s\",\n  \"ziel\": \"%s\",\n  \"mapping\": {\n", wOrig, wAlias);
    for (int i = 0; i < zeige; i++) {
        char q[512], z[512]; wchar_t wq[512], wz[512];
        maskiereJSON(d[i].quelle, q, sizeof(q)); maskiereJSON(d[i].ziel, z, sizeof(z));
        konvertiere1252ZuWide(q, wq, 512); konvertiere1252ZuWide(z, wz, 512);
        const wchar_t *komma = (i < zeige-1 || hatMehr) ? L"," : L"";
        fwprintf(out, L"    \"%s\": \"%s\"%s\n", wq, wz, komma);
    }
    if (hatMehr) fwprintf(out, L"    /* ... %d weitere */\n", n - lim);
    fwprintf(out, L"  }\n}\n");
}

static void erstelleAusgabe(const Zuordnung *d, int n, const char *orig, const char *alias,
                             const char *elseWert, AusgabeFormat fmt, FILE *out, int lim) {
    switch (fmt) {
        case FORMAT_CASE:   schreibeCase(d, n, orig, alias, elseWert, out, lim); break;
        case FORMAT_DECODE: schreibeDecode(d, n, orig, alias, elseWert, out, lim); break;
        case FORMAT_VALUES: schreibeValues(d, n, orig, alias, out, lim); break;
        case FORMAT_JSON:   schreibeJSON(d, n, orig, alias, out, lim); break;
    }
}

static void speichereAusgabe(const wchar_t *pfad, const char *orig, const char *alias,
                              const char *elseWert, AusgabeFormat fmt,
                              const Zuordnung *d, int n) {
    FILE *f = _wfopen(pfad, L"w, ccs=UTF-8");
    if (!f) { logFehler(L"Kann Ausgabedatei nicht erstellen."); return; }
    erstelleAusgabe(d, n, orig, alias, elseWert, fmt, f, 0);
    fclose(f);
    wprintf(L"\nVorschau (max. %d Einträge):\n", MAX_VORSCHAU);
    erstelleAusgabe(d, n, orig, alias, elseWert, fmt, stdout, MAX_VORSCHAU);
}

// ============================================================
//  Interaktive Eingabe-Helfer
// ============================================================

static AusgabeFormat waehleFormat(void) {
    wprintf(L"\nAusgabeformat:\n  [1] SQL CASE\n  [2] DECODE (Oracle)\n  [3] VALUES-Tabelle\n  [4] JSON\n");
    int wahl = -1;
    do {
        wprintf(L"Format (1-4, Enter = 1): ");
        if (wscanf(L"%d", &wahl) != 1) wahl = 1;
        leereStdin();
    } while (wahl < 1 || wahl > 4);
    return (AusgabeFormat)wahl;
}

static void waehleElseWert(char *elseWert, size_t maxSize) {
    wprintf(L"ELSE-Wert (Enter = 'Unbekannt', 'NULL' für NULL): ");
    wchar_t wtmp[256] = {0};
    fgetws(wtmp, 256, stdin);
    wtmp[wcscspn(wtmp, L"\r\n")] = L'\0';
    if (wcslen(wtmp) == 0) {
        strncpy(elseWert, "Unbekannt", maxSize - 1);
    } else if (wcscmp(wtmp, L"NULL") == 0 || wcscmp(wtmp, L"null") == 0) {
        elseWert[0] = '\0';
    } else {
        WideCharToMultiByte(CP_ACP, 0, wtmp, -1, elseWert, (int)maxSize - 1, NULL, NULL);
    }
    elseWert[maxSize - 1] = '\0';
}

// ============================================================
//  Batch-Modus
// ============================================================

typedef struct {
    wchar_t csv[MAX_PFAD];
    int     quellspalte;
    int     zielspalte;
    char    alias[256];
    AusgabeFormat format;
    char    elseWert[256];
    wchar_t ausgabe[MAX_PFAD];
    int     clipboard;
    // Pflichtfelder gesetzt?
    int     csvSet, quelleSet, zielSet, aliasSet;
} BatchConfig;

static int leseBatchConfig(const wchar_t *pfad, BatchConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->elseWert, "Unbekannt", sizeof(cfg->elseWert) - 1);
    cfg->format = FORMAT_CASE;
    wcscpy(cfg->ausgabe, L"FertigerCase.txt");

    FILE *f = _wfopen(pfad, L"r");
    if (!f) { logFehler(L"Batch-Konfigurationsdatei nicht gefunden."); return 1; }

    char zeile[512];
    while (fgets(zeile, sizeof(zeile), f)) {
        zeile[strcspn(zeile, "\r\n")] = '\0';
        if (!zeile[0] || zeile[0] == '#') continue;

        char *eq = strchr(zeile, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = zeile, *val = eq + 1;
        trimChar(key); trimChar(val);

        if (_stricmp(key, "csv") == 0) {
            MultiByteToWideChar(CP_ACP, 0, val, -1, cfg->csv, MAX_PFAD);
            cfg->csvSet = 1;
        } else if (_stricmp(key, "quellspalte") == 0) {
            cfg->quellspalte = atoi(val); cfg->quelleSet = 1;
        } else if (_stricmp(key, "zielspalte") == 0) {
            cfg->zielspalte = atoi(val); cfg->zielSet = 1;
        } else if (_stricmp(key, "alias") == 0) {
            strncpy(cfg->alias, val, sizeof(cfg->alias) - 1);
            cfg->alias[sizeof(cfg->alias) - 1] = '\0';
            cfg->aliasSet = 1;
        } else if (_stricmp(key, "format") == 0) {
            int fmt = atoi(val);
            if (fmt >= 1 && fmt <= 4) cfg->format = (AusgabeFormat)fmt;
        } else if (_stricmp(key, "else") == 0) {
            strncpy(cfg->elseWert, val, sizeof(cfg->elseWert) - 1);
            cfg->elseWert[sizeof(cfg->elseWert) - 1] = '\0';
        } else if (_stricmp(key, "ausgabe") == 0) {
            MultiByteToWideChar(CP_ACP, 0, val, -1, cfg->ausgabe, MAX_PFAD);
        } else if (_stricmp(key, "clipboard") == 0) {
            cfg->clipboard = atoi(val);
        }
    }
    fclose(f);

    if (!cfg->csvSet || !cfg->quelleSet || !cfg->zielSet || !cfg->aliasSet) {
        logFehler(L"Batch-Konfiguration unvollständig. Erforderlich: csv, quellspalte, zielspalte, alias");
        return 1;
    }
    return 0;
}

static void verarbeiteBatchDatei(const wchar_t *pfad) {
    BatchConfig cfg;
    if (leseBatchConfig(pfad, &cfg)) return;

    wprintf(L"Batch-Modus: %s\n", cfg.csv);

    if (_waccess(cfg.csv, 0) != 0) {
        wchar_t err[MAX_PFAD + 64];
        swprintf(err, MAX_PFAD + 64, L"CSV nicht gefunden: %s", cfg.csv);
        logFehler(err); return;
    }

    FILE *datei = _wfopen(cfg.csv, L"r");
    if (!datei) { logFehler(L"CSV konnte nicht geöffnet werden."); return; }

    fseek(datei, 0, SEEK_END);
    if (ftell(datei) == 0) { logFehler(L"CSV ist leer."); fclose(datei); return; }
    rewind(datei);

    char sep;
    char spaltenNamen[MAX_SPALTEN][256];
    int nSpalten;
    if (leseKopfzeile(datei, &sep, spaltenNamen, &nSpalten)) { fclose(datei); return; }

    if (cfg.quellspalte >= nSpalten || cfg.zielspalte >= nSpalten) {
        logFehler(L"Spaltenindex außerhalb des gültigen Bereichs."); fclose(datei); return;
    }

    Zuordnung *daten = malloc(MAX_DATEN * sizeof(Zuordnung));
    if (!daten) { logFehler(L"Speicherfehler."); fclose(datei); return; }

    int count, skipEmpty, skipDup;
    leseDaten(datei, sep, cfg.quellspalte, cfg.zielspalte, daten, &count, &skipEmpty, &skipDup);
    fclose(datei);

    wprintf(L"Batch: %d Einträge | %d leer | %d Duplikate\n", count, skipEmpty, skipDup);
    speichereAusgabe(cfg.ausgabe, spaltenNamen[cfg.quellspalte], cfg.alias, cfg.elseWert, cfg.format, daten, count);
    wprintf(L"Gespeichert: %s\n", cfg.ausgabe);

    if (cfg.clipboard) kopiereInZwischenablage(cfg.ausgabe);

    free(daten);
}

// ============================================================
//  main
// ============================================================

int main(int argc, char *argv[]) {
    _setmbcp(_MB_CP_1252);
    setlocale(LC_ALL, "");
    _setmode(_fileno(stdout), _O_WTEXT);

    wprintf(L"Casium SQL CASE Generator %hs\n%hs\n\n", CASIUM_VERSION, CASIUM_AUTOR);

    // Batch-Modus: casium.exe config.cfg
    if (argc > 1) {
        wchar_t batchPfad[MAX_PFAD];
        MultiByteToWideChar(CP_ACP, 0, argv[1], -1, batchPfad, MAX_PFAD);
        verarbeiteBatchDatei(batchPfad);
        return 0;
    }

    // Interaktiver Modus
    Zuordnung *daten = malloc(MAX_DATEN * sizeof(Zuordnung));
    if (!daten) { logFehler(L"Speicherfehler beim Start."); return 1; }

    wchar_t csvPfad[MAX_PFAD];
    char spaltenNamen[MAX_SPALTEN][256];
    int nSpalten;
    char sep;

    while (1) {
        wprintf(L"Pfad zur CSV (q zum Beenden): ");
        fgetws(csvPfad, MAX_PFAD, stdin);
        csvPfad[wcscspn(csvPfad, L"\r\n")] = L'\0';
        if (wcslen(csvPfad) == 1 && (csvPfad[0] == L'q' || csvPfad[0] == L'Q')) break;

        size_t len = wcslen(csvPfad);
        if (len > 1 && csvPfad[0] == L'"' && csvPfad[len-1] == L'"') {
            memmove(csvPfad, csvPfad+1, (len-2)*sizeof(wchar_t));
            csvPfad[len-2] = L'\0';
        }

        if (_waccess(csvPfad, 0) != 0) {
            wchar_t err[MAX_PFAD + 64];
            swprintf(err, MAX_PFAD + 64, L"Datei nicht gefunden: %s", csvPfad);
            logFehler(err); continue;
        }

        FILE *datei = _wfopen(csvPfad, L"r");
        if (!datei) { logFehler(L"Datei konnte nicht geöffnet werden."); continue; }

        fseek(datei, 0, SEEK_END);
        if (ftell(datei) == 0) { logFehler(L"Datei ist leer."); fclose(datei); continue; }
        rewind(datei);

        if (leseKopfzeile(datei, &sep, spaltenNamen, &nSpalten)) { fclose(datei); continue; }

        wprintf(L"\nSpalten (Trennzeichen: '%c'):\n", sep);
        for (int i = 0; i < nSpalten; i++) {
            wchar_t wbuf[256]; konvertiere1252ZuWide(spaltenNamen[i], wbuf, 256);
            wprintf(L"  [%d] %s\n", i, wbuf);
        }

        int weiteresSpaltenpaar = 1;
        while (weiteresSpaltenpaar) {
            int idxQ = -1, idxZ = -1;
            do {
                wprintf(L"\nNummer der Quellspalte (0-%d): ", nSpalten-1);
                if (wscanf(L"%d", &idxQ) != 1) idxQ = -1;
                leereStdin();
            } while (idxQ < 0 || idxQ >= nSpalten);
            do {
                wprintf(L"Nummer der Zielspalte (0-%d): ", nSpalten-1);
                if (wscanf(L"%d", &idxZ) != 1) idxZ = -1;
                leereStdin();
            } while (idxZ < 0 || idxZ >= nSpalten);

            wprintf(L"Name für SQL-Spalte: ");
            wchar_t wtmp[256] = {0};
            fgetws(wtmp, 256, stdin);
            wtmp[wcscspn(wtmp, L"\r\n")] = L'\0';
            char aliasSpalte[256];
            WideCharToMultiByte(CP_ACP, 0, wtmp, -1, aliasSpalte, 255, NULL, NULL);
            aliasSpalte[255] = '\0';

            AusgabeFormat format = waehleFormat();
            char elseWert[256] = "Unbekannt";
            if (format == FORMAT_CASE || format == FORMAT_DECODE)
                waehleElseWert(elseWert, sizeof(elseWert));

            rewind(datei);
            char skipbuf[MAX_ZEILE];
            fgets(skipbuf, MAX_ZEILE, datei);

            int count, skipEmpty, skipDup;
            leseDaten(datei, sep, idxQ, idxZ, daten, &count, &skipEmpty, &skipDup);
            wprintf(L"\n%d Einträge | %d leer | %d Duplikate\n", count, skipEmpty, skipDup);

            wchar_t outPfad[MAX_PFAD];
            wprintf(L"Ausgabedatei (Enter = FertigerCase.txt): ");
            fgetws(outPfad, MAX_PFAD, stdin);
            outPfad[wcscspn(outPfad, L"\r\n")] = L'\0';
            if (wcslen(outPfad) == 0) wcscpy(outPfad, L"FertigerCase.txt");

            speichereAusgabe(outPfad, spaltenNamen[idxQ], aliasSpalte, elseWert, format, daten, count);
            wprintf(L"\nGespeichert: %s\n", outPfad);

            wprintf(L"In Zwischenablage kopieren? (j/n): ");
            wchar_t clip = fgetwc(stdin);
            leereStdin();
            if (clip == L'j' || clip == L'J') kopiereInZwischenablage(outPfad);

            wprintf(L"\nWeiteres Spaltenpaar aus dieser Datei? (j/n): ");
            wchar_t ant = fgetwc(stdin);
            leereStdin();
            weiteresSpaltenpaar = (ant == L'j' || ant == L'J');
        }

        fclose(datei);

        wprintf(L"\nNoch eine CSV? (j/n): ");
        wchar_t ant = fgetwc(stdin);
        leereStdin();
        if (!(ant == L'j' || ant == L'J')) break;
    }

    free(daten);
    return 0;
}
