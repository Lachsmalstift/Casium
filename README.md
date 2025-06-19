# Casium ein SQL CASE Generator

Casium ist ein einfaches Konsolenprogramm, das aus CSV-Daten automatisch SQL CASE-Ausdrücke generiert.  
Es dient zur schnellen Umwandlung von Mapping-Tabellen (z. B. IDs, Personalnummern, Codes) in fertige SQL-Statements.

---

## Features

- CSV einlesen (mit BOM-Erkennung, Trennung nach Komma oder Semikolon)
- Interaktive Auswahl von Quell- und Zielspalten
- Prüfung auf doppelte und leere Einträge
- Ausgabe eines fertigen SQL CASE Ausdrucks
- Konsolenbasiert, portabel, ohne zusätzliche Abhängigkeiten
- Fehler-Logging in Logdatei
- Mehrfachdurchläufe möglich
- UTF-8 und Unicode-Ausgabe für Windows-Konsole

---

## Nutzung

- Starte Casium über die Konsole.
- Gib den Pfad zur CSV-Datei an (mit oder ohne Anführungszeichen).
- Wähle die gewünschten Spalten aus.
- Das Programm erstellt automatisch den fertigen SQL CASE Ausdruck.
- Die Ausgabe wird in der Datei `FertigerCase.txt` gespeichert (oder in einer selbstgewählten Datei).

---

## Lizenz

Dieses Projekt steht unter der  
Creative Commons Attribution 4.0 International License (CC BY 4.0).

Siehe die ausführliche Lizenz in der Datei [`LICENSE.md`](LICENSE.md).

Urheber:  
Made with tears and hate in Mesum
by Christopher Lane Charles Dentmon  
(c) 2025
---

## Changelog

Siehe [`CHANGELOG.md`](CHANGELOG.md).

---

## Projektstatus

Version v1.0.0 — erste stabile Veröffentlichung.
