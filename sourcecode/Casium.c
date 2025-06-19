#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define MAX_ZEILE    1024
#define MAX_DATEN    1000
#define CASIUM_VERSION "v1.0.0"
#define CASIUM_AUTOR   "Entwickelt von Christopher Lane Charles Dentmon"

// Zeigt Fehlermeldung an und schreibt ins Log
void logFehler(const wchar_t *nachricht) {
    fwprintf(stderr, L"FEHLER: %s\n", nachricht);
    MessageBoxW(NULL, nachricht, L"Fehler", MB_OK | MB_ICONERROR);
    FILE *log = _wfopen(L"fehler.log", L"a, ccs=UTF-8");
    if (log) {
        fwprintf(log, L"FEHLER: %s\n", nachricht);
        fclose(log);
    }
}

// Wandelt Windows-1252-String in Unicode um
void konvertiere1252ZuWide(const char *input, wchar_t *output, int size) {
    MultiByteToWideChar(CP_ACP, 0, input, -1, output, size);
}

// Verdoppelt ' für SQL
void maskiereApostrophe(const char *eingabe, char *ausgabe, size_t maxSize) {
    size_t pos = 0;
    for (size_t i = 0; eingabe[i] != '\0' && pos + 1 < maxSize; ++i) {
        if (eingabe[i] == '\'') {
            if (pos + 2 < maxSize) {
                ausgabe[pos++] = '\'';
                ausgabe[pos++] = '\'';
            }
        } else {
            ausgabe[pos++] = eingabe[i];
        }
    }
    ausgabe[pos] = '\0';
}

// Struktur für ein Datenpaar
typedef struct {
    char quelle[256];
    char ziel[256];
} Zuordnung;

// Liest CSV und füllt Daten
int leseCSV(const wchar_t *dateiname,
            Zuordnung *daten,
            int *anzahlZeilen,
            int *leereUebersprungen,
            int *doppelteUebersprungen,
            char *aliasSpalte,
            char *origSpalte) {
    // Debug & Trim Quotes
    wchar_t pfad[512]; wcscpy(pfad, dateiname);
    size_t len = wcslen(pfad);
    if (len > 1 && pfad[0] == L'"' && pfad[len-1] == L'"') {
        memmove(pfad, pfad+1, (len-2)*sizeof(wchar_t));
        pfad[len-2] = L'\0';
    }
    wprintf(L"DEBUG: Öffne CSV-Datei: %s\n", pfad);
    // Existenzprüfung
    if (_waccess(pfad, 0) != 0) {
        wchar_t err[512];
        swprintf(err, L"Datei nicht gefunden oder kein Zugriff: %s", pfad);
        logFehler(err);
        return 1;
    }
    FILE *datei = _wfopen(pfad, L"r");
    if (!datei) { logFehler(L"Datei konnte nicht geöffnet werden."); return 1; }
    // Größe prüfen
    fseek(datei, 0, SEEK_END);
    long groesse = ftell(datei);
    rewind(datei);
    if (groesse == 0) { logFehler(L"Datei ist leer."); fclose(datei); return 1; }
    
    // Kopfzeile einlesen
    char zeile[MAX_ZEILE];
    if (!fgets(zeile, MAX_ZEILE, datei)) {
        logFehler(L"Kopfzeile konnte nicht gelesen werden."); fclose(datei); return 1; }
    // BOM entfernen
    if ((unsigned char)zeile[0]==0xEF && (unsigned char)zeile[1]==0xBB && (unsigned char)zeile[2]==0xBF)
        memmove(zeile, zeile+3, strlen(zeile+3)+1);

    // Spalten auslesen
    const char sep[] = ",;";
    char *spalten[100]; int ncol = 0;
    char *tok = strtok(zeile, sep);
    wprintf(L"Spalten gefunden:\n");
    while (tok && ncol < 100) {
        spalten[ncol] = _strdup(tok);
        spalten[ncol][strcspn(spalten[ncol], "\r\n")] = '\0';
        wchar_t wbuf[256]; konvertiere1252ZuWide(spalten[ncol], wbuf, 256);
        wprintf(L"  [%d] %s\n", ncol, wbuf);
        tok = strtok(NULL, sep);
        ncol++;
    }
    if (ncol < 2) { logFehler(L"Zu wenige Spalten gefunden."); fclose(datei); return 1; }
    
    // Spaltenwahl
    int idxQ=-1, idxZ=-1;
    do { wprintf(L"Nummer der Quellspalte (0-%d): ", ncol-1); } while (wscanf(L"%d", &idxQ)!=1 || idxQ<0 || idxQ>=ncol);
    do { wprintf(L"Nummer der Zielspalte (0-%d): ", ncol-1); } while (wscanf(L"%d", &idxZ)!=1 || idxZ<0 || idxZ>=ncol);
    // Originalspaltenname
    strncpy(origSpalte, spalten[idxQ], 255);
    // Alias (SQL-Spalte)
    wprintf(L"Name für SQL-Spalte: "); char tmp[256]; scanf("%255s", tmp); strncpy(aliasSpalte, tmp, 255);

    *anzahlZeilen = *leereUebersprungen = *doppelteUebersprungen = 0;
    // Datenzeilen einlesen
    while (fgets(zeile, MAX_ZEILE, datei)) {
        char *fld[100] = {0}; int cnt = 0;
        tok = strtok(zeile, sep);
        while (tok && cnt < 100) { fld[cnt++] = tok; tok = strtok(NULL, sep); }
        if (cnt <= idxQ || cnt <= idxZ) continue;
        fld[idxQ][strcspn(fld[idxQ], "\r\n")] = '\0';
        fld[idxZ][strcspn(fld[idxZ], "\r\n")] = '\0';
        if (!*fld[idxQ] || !*fld[idxZ]) { (*leereUebersprungen)++; continue; }
        int dup = 0;
        for (int i = 0; i < *anzahlZeilen; i++) {
            if (strcmp(daten[i].quelle, fld[idxQ]) == 0) { dup = 1; break; }
        }
        if (dup) { (*doppelteUebersprungen)++; continue; }
        strncpy(daten[*anzahlZeilen].quelle, fld[idxQ], 255);
        strncpy(daten[*anzahlZeilen].ziel,   fld[idxZ],   255);
        (*anzahlZeilen)++;
    }
    fclose(datei);
    for (int i = 0; i < ncol; i++) free(spalten[i]);
    return 0;
}

// Erzeugt SQL CASE Ausdruck
void erstelleCase(const Zuordnung *daten, int count, const char *origSpalte, const char *aliasSpalte, FILE *out) {
    wchar_t wOrig[256], wAlias[256];
    konvertiere1252ZuWide(origSpalte, wOrig, 256);
    konvertiere1252ZuWide(aliasSpalte, wAlias, 256);
    wprintf(L"\nCASE-Ausdruck:\nCASE\n"); fwprintf(out, L"CASE\n");
    for (int i = 0; i < count; i++) {
        char q[512], z[512];
        maskiereApostrophe(daten[i].quelle, q, sizeof(q));
        maskiereApostrophe(daten[i].ziel,   z, sizeof(z));
        wchar_t wq[256], wz[256];
        konvertiere1252ZuWide(q, wq, 256);
        konvertiere1252ZuWide(z, wz, 256);
        wprintf(L"  WHEN %s = '%s' THEN '%s'\n", wOrig, wq, wz);
        fwprintf(out, L"  WHEN %s = '%s' THEN '%s'\n", wOrig, wq, wz);
    }
    wprintf(L"  ELSE 'Unbekannt'\nEND AS %s;\n", wAlias);
    fwprintf(out, L"  ELSE 'Unbekannt'\nEND AS %s;\n", wAlias);
}

// Speichert Ergebnis
void speichereAusgabe(const wchar_t *file, const char *origSpalte, const char *aliasSpalte, const Zuordnung *daten, int count) {
    FILE *f = _wfopen(file, L"w, ccs=UTF-8");
    if (!f) { logFehler(L"Kann Ausgabedatei nicht erstellen."); return; }
    erstelleCase(daten, count, origSpalte, aliasSpalte, f);
    fclose(f);
}

// Einstiegspunkt als normale main, nicht wmain
int main(void) {
    _setmbcp(_MB_CP_1252);
    setlocale(LC_ALL, "");
    _setmode(_fileno(stdout), _O_WTEXT);

    wprintf(L"Casium SQL CASE Generator %hs\n%hs\n\n", CASIUM_VERSION, CASIUM_AUTOR);

    wchar_t csvPfad[512], outPfad[512];
    Zuordnung daten[MAX_DATEN];
    int count, skipEmpty, skipDup;
    char aliasSpalte[256], origSpalte[256];

    while (1) {
        wprintf(L"Pfad zur CSV (q zum Beenden): ");
        fgetws(csvPfad, 512, stdin);
        csvPfad[wcscspn(csvPfad, L"\r\n")] = L'\0';
        if (wcslen(csvPfad)==1 && (csvPfad[0]==L'q'||csvPfad[0]==L'Q')) break;

        if (leseCSV(csvPfad, daten, &count, &skipEmpty, &skipDup, aliasSpalte, origSpalte)) continue;

        wprintf(L"\nÜbersprungen: %d leer, %d doppelt\n", skipEmpty, skipDup);
        wprintf(L"Ausgabedatei (Default FertigerCase.txt): ");
        fgetws(outPfad, 512, stdin);
        outPfad[wcscspn(outPfad, L"\r\n")] = L'\0';
        if (wcslen(outPfad)==0) wcscpy(outPfad, L"FertigerCase.txt");

        speichereAusgabe(outPfad, origSpalte, aliasSpalte, daten, count);

        wprintf(L"\nNoch einen erstellen? (j/n): ");
        wchar_t ant = fgetwc(stdin);
        while (fgetwc(stdin)!=L'\n');
        if (!(ant==L'j'||ant==L'J')) break;
    }
    return 0;
}
