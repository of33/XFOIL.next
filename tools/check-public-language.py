#!/usr/bin/env python3
# XFOIL.next
#
# Copyright (C) 2026 Olaf Frommann
# https://github.com/of33/XFOIL.next
#
# XFOIL.next is based on XFOIL 6.99 by Mark Drela and
# Harold Youngren.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Permanent public-language gate for XFOIL.next.

Checks that publicly visible developer text (C++ sources and headers,
tests, tools, Makefile, project-side Fortran harness comments) contains
no German text and no internal migration markers.  Canonical Fortran
product text, golden/reference data and internal migration material are
excluded by rule, not by ad-hoc suppression.

The tool never writes to the repository.  Exit code 0 means clean.
"""

import argparse
import json
import pathlib
import re
import sys

ROOT_DEFAULT = pathlib.Path(__file__).resolve().parent.parent

INCLUDE_DIR_GLOBS = ("src", "include", "tests")
SOURCE_SUFFIXES = {".cpp", ".h"}

EXCLUDED_DIRS = (
    "fortran-src",
    "fortran-bin",
    "sample",
    "docs",
    ".agents",
    "build",
)
EXCLUDED_PATHS = (
    "tests/golden",
    "tests/data",
    "tests/fortran-reference/cases",
    "tests/fortran-reference/manifest.tsv",
    "LICENSE",
    "NOTICE",
    "SHA256SUMS",
    "AGENTS.md",
    "handover.md",
    "scope.md",
    "zusatzbericht.md",
    "xfoil-opencode-handoff.md",
    "plotting.md",
    "osmapDP-result.md",
)
EXCLUDED_FILES = (
    "tools/diagnostics",
    "tools/build-core-migration-inventories.py",
    "tools/inventory-fortran.py",
    "tools/inventory-fortran-cpp.py",
    "tools/gen-golden-inventory.sh",
)

UMMLAUT = re.compile(r"[äöüÄÖÜß]")
TOKEN = re.compile(r"[A-Za-zÄÖÜäöüß][A-Za-zÄÖÜäöüß-]*")
PATH_LIKE = re.compile(r"(?:[A-Za-z0-9_.-]+/)+[A-Za-z0-9_.-]*")
MARKERS = [
    (re.compile(r"(?<![A-Za-z0-9])C11b(?:-P\d+)*\b"), "migration marker C11b"),
    (re.compile(r"(?<![A-Za-z0-9])C12(?:-P\d+)?\b"), "migration marker C12"),
    (re.compile(r"(?<![A-Za-z0-9])C11\b"), "migration marker C11"),
    (re.compile(r"(?<![A-Za-z0-9])OSM-[A-Z]\d*[a-z]?\b"), "migration marker OSM-"),
    (re.compile(r"(?<![A-Za-z0-9])P7-[A-Z0-9]+\b"), "migration marker P7-"),
    (re.compile(r"\bCutover\b"), "migration marker Cutover"),
    (re.compile(r"\bpending port\b"), "migration phrase 'pending port'"),
    (re.compile(r"\bnot yet ported\b"), "migration phrase 'not yet ported'"),
    (re.compile(r"\bmigration complete\b"), "migration phrase 'migration complete'"),
    (re.compile(r"(?i)\bunrouted\b"), "migration phrase 'unrouted'"),
    (re.compile(r"(?i)\bproduction[- ]rout(?:ing|ed|es|s)?\b"), "migration phrase 'production routing'"),
    (re.compile(r"(?i)\b(?:not yet|still not) production\b"), "migration phrase 'not yet production'"),
    (re.compile(r"\batomically production\b"), "migration phrase 'atomically production'"),
    (re.compile(r"\batomic(?:al)? switch(?:[- ]over)?\b"), "migration phrase 'atomic switch'"),
    (re.compile(r"\bswitch[- ]over\b"), "migration phrase 'switch-over'"),
    (re.compile(r"\b(?:pending|block|not-started) list\b"), "migration phrase 'pending/block list'"),
    (re.compile(r"\bunported\b"), "migration phrase 'unported'"),
    (re.compile(r"(?i)\bprepared (?:handlers?\b|but\b|and still\b|, not\b)"), "migration phrase 'prepared handler'"),
    (re.compile(r"(?<![A-Za-z0-9])G-B[0-9]+\b"), "internal work-package label G-B"),
    (re.compile(r"(?i)\bof the assignment\b"), "internal assignment reference"),
    (re.compile(r"(?i)\bassignment sections?\b"), "internal assignment reference"),
    (re.compile(r"(?i)\bsections? [0-9]+ to [0-9]+\b"), "internal assignment section range"),
    (re.compile(r"\bG1-G4\b"), "internal golden label G1-G4"),
    (re.compile(r"(?<![A-Za-z0-9])R12\b"), "internal migration step label R12"),
    (re.compile(r"(?<![A-Za-z0-9])R0\b"), "internal migration step label R0"),
    (re.compile(r"(?<![A-Za-z0-9])[AG]-R[0-9]+\b"),
     "internal acceptance finding label A-R/G-R"),
    (re.compile(r"(?i)\bfits not to\b"), "broken English phrase 'fits not to'"),
    (re.compile(r"(?i)\berneuter\b"), "German word 'erneuter'"),
    (re.compile(r"(?i)\bbecause dies the frozen reference values\b"),
     "broken English phrase (because-dies)"),
    (re.compile(r"(?i)\blets the value unchanged\b"),
     "broken English phrase (lets-unchanged)"),
    (re.compile(r"(?i)\bafter\s+NSEQEX\s*=\s*4\s+points\s+holds\b"),
     "broken English phrase (points-holds)"),
    (re.compile(r"(?i)\bSA/approximately\b"),
     "broken English phrase (SA-approximately)"),
    (re.compile(r"(?i)\bA edit descriptor\b"),
     "broken English phrase (A-edit-descriptor)"),
    (re.compile(r"(?i)\bThey changes no\b"),
     "broken English phrase (They-changes-no)"),
    (re.compile(r"(?i)\bIt change no\b"),
     "broken English phrase (It-change-no)"),
    (re.compile(r"(?i)\bunchanged on\."),
     "broken English phrase (unchanged-on)"),
    (re.compile(r"(?i)\bAll Tables values finite\b"),
     "broken English phrase (All-Tables-values-finite)"),
    (re.compile(r"(?i)\bare a on-demand\b"),
     "broken English phrase (are-a-on-demand)"),
    (re.compile(r"(?i)\bis a still never-written\b"),
     "broken English phrase (is-a-still-never-written)"),
    (re.compile(r"(?i)\bthen follows mandatorily\b"),
     "broken English phrase (then-follows-mandatorily)"),
    (re.compile(r"(?i)\ba run changed not the state a other\b"),
     "broken English phrase (run-changed-not-the-state)"),
    (re.compile(r"(?i)\bMach calls itself No FCPMIN\b"),
     "broken English phrase (Mach-calls-itself-No-FCPMIN)"),
    (re.compile(r"(?i)\bhere a controlled deviation\b"),
     "broken English phrase (here-a-controlled-deviation)"),
    (re.compile(r"(?i)\bthere is transition\b"),
     "broken English phrase (there-is-transition)"),
    (re.compile(r"(?i)\bxfoil session(?: tests)?\.(?:h|cpp)\b"),
     "misspelled project file 'Xfoil session.*'"),
    (re.compile(r"(?i)\bsession types\.h\b"),
     "misspelled project file (Session-types)"),
    (re.compile(r"(?i)\bpolar tests\.cpp\b"),
     "misspelled project file (polar-tests)"),
    (re.compile(r"(?!include/xfoil/Limits\.h\b)(?i:include/xfoil/limits\.h\b)"),
     "misspelled project file (limits-case)"),
    (re.compile(r"(?i)\bfortran-src/airfoil\.f\b"),
     "nonexistent Fortran source 'airfoil.f'"),
    (re.compile(r"(?i)\bfortran-src/xgdes\.f/xtcam\.f\b"),
     "broken Fortran source path 'xgdes.f/xtcam.f'"),
]

# Removed target names, routing labels and broken hybrid phrases.  A line
# that names one of them only as a forbidden historical fixture (negative
# gate list or self-test) stays allowed; active prose is a finding.
INTERNAL_NAME_MARKERS = [
    (re.compile(r"(?i)\bcore-unit\b"), "removed internal target core-unit"),
    (re.compile(r"(?i)\btaken over shared via\b"), "broken hybrid phrase"),
    (re.compile(r"(?i)\bLBLINI-State\b"), "broken hybrid phrase"),
    (re.compile(r"(?i)\bRepository-Root\b"), "broken hybrid phrase"),
    (re.compile(r"(?i)\bCore-Source\b(?!-)"), "broken hybrid phrase"),
    (re.compile(r"(?i)\bwould be controlled blocked\b"), "broken hybrid phrase"),
]

# Internal migration paths are findings in active prose and help text.  A
# line that names such a path only as a forbidden historical path (negative
# gate list or fixture) stays allowed.
INTERNAL_PATH_MARKERS = [
    (re.compile(r"docs/refactoring\b"), "internal migration path"),
    (re.compile(r"\bOpenSourceMigration\b"), "internal migration path"),
]
NEGATIVE_MARKER_CONTEXT = re.compile(
    r"(?i)\b(?:forbidden|negative|historical|obsolete|rejected)\b")

# Comment-only markers: internal phase labels and dead diagnostic paths.
COMMENT_MARKERS = [
    (re.compile(r"\((?:P[1-6])(?:\s+[a-z][A-Za-z0-9 -]*)?\)"),
     "internal phase label P1-P6"),
    (re.compile(r"\bP[1-6] (?:phase|blocker|viscous harnesses)\b"),
     "internal phase label P1-P6"),
    (re.compile(r"tools/diagnostics/"),
     "dead tools/diagnostics reference"),
]

LEXICON = set(['ab', 'abarbeiten', 'abbildung', 'abbildungen', 'abbruchzustaende', 'aber', 'abfolge', 'abgearbeitet', 'abgeben', 'abgebildet', 'abgegeben', 'abgelehnt', 'abgeleitet', 'abgeleitete', 'abgeschaltet', 'abgeschlossen', 'abgeschlossene', 'abgeschnitten', 'abgeschnittene', 'abgesichert', 'abgrenzung', 'abhaengig', 'abhaengigkeit', 'abhaengigkeiten', 'abhaengigkeitsinventar', 'abhaengt', 'ablehnen', 'ablehnung', 'ableiten', 'ableitung', 'abloesung', 'abnahme', 'absatz', 'abschliessen', 'abschliessende', 'abschliessenden', 'abschluss', 'abschnitt', 'abschnitte', 'absicherung', 'absichtlich', 'absolut', 'absolutbetrag', 'abstoessung', 'abstrom', 'abwaerts', 'abweichen', 'abweicht', 'abweichung', 'abweichungen', 'abweisung', 'achse', 'achsen', 'achtet', 'additiv', 'adoptions', 'aehnlichkeitsstation', 'aendern', 'aendert', 'aenderung', 'aenderungen', 'aequivalent', 'aequivalente', 'aeussere', 'aeusseren', 'aeusserlich', 'aktiv', 'aktive', 'aktiven', 'aktiver', 'aktives', 'aktivieren', 'aktiviert', 'aktivierte', 'aktivierung', 'aktualisieren', 'aktualisiert', 'aktualisierte', 'aktualisierung', 'aktuell', 'aktuelle', 'aktuellee', 'aktuellem', 'aktuellen', 'aktueller', 'aktuelles', 'akzeptieren', 'akzeptiert', 'aligniert', 'alignierung', 'alle', 'allein', 'allen', 'aller', 'alles', 'als', 'alt', 'alte', 'alten', 'alter', 'altlast', 'am', 'an', 'analog', 'analoge', 'analyse', 'analyseergebnis', 'analyseinvalidierung', 'analysen', 'analytisch', 'andere', 'anderen', 'anderer', 'anderes', 'anderm', 'anfachung', 'anfachungsgleichung', 'anfaenglich', 'anfang', 'anfangsbedingung', 'anfangsbedingungen', 'anfangswert', 'anfangswerte', 'anforderung', 'anforderungen', 'anfuehrungszeichen', 'angefuegt', 'angegebene', 'angelaufen', 'angelegt', 'angestoessen', 'angezeigt', 'anhaengen', 'anhaengend', 'anhaengende', 'anhaengenden', 'anhaengendes', 'anhangigkeitspunkt', 'anlegbar', 'anlegen', 'anleitung', 'anmerkung', 'anmerkungen', 'annahme', 'annahmeg', 'annahmen', 'ans', 'ansaetze', 'ansatz', 'ansaugung', 'anschliessend', 'ansprechen', 'anspricht', 'anstatt', 'anstellwinkel', 'anstossen', 'anstroemung', 'anstrom', 'anteil', 'anteile', 'antwort', 'antworten', 'anweisung', 'anwender', 'anwendung', 'anwendungsfaelle', 'anwendungsfall', 'anwendungsschicht', 'anzahl', 'anzahlwert', 'anziehung', 'aperiodisch', 'approximieren', 'arbeit', 'arbeitspuffer', 'arbeitsverzeichnis', 'architektur', 'argumente', 'argumentvalidierung', 'artefakt', 'artefakte', 'artefakts', 'assemblierung', 'asymmetrisch', 'atomaren', 'auch', 'auf', 'auffuellung', 'aufgefuellt', 'aufgeloest', 'aufgeloeste', 'aufgeloesten', 'aufgenommen', 'aufgerufen', 'aufgerufene', 'aufgeteilt', 'aufgrund', 'auflistung', 'aufloesbar', 'aufloesen', 'aufnehmen', 'aufruf', 'aufrufe', 'aufrufen', 'aufrufer', 'aufruft', 'aufteilen', 'auftrieb', 'aufwaendig', 'aufwaerts', 'aus', 'ausdruecklich', 'ausdrueckliche', 'ausfuehrbar', 'ausfuehrbarkeit', 'ausfuehrung', 'ausfuehrungsvertrag', 'ausfuehrungszeitpunkt', 'ausfuehrungvertrag', 'ausgabe', 'ausgabedatei', 'ausgabeformat', 'ausgaben', 'ausgabepfad', 'ausgabezeile', 'ausgabezeilen', 'ausgeben', 'ausgefuehrt', 'ausgefuehrten', 'ausgegeben', 'ausgelassen', 'ausgeloest', 'ausgetauscht', 'ausgewaehlt', 'ausgewertet', 'ausgewirkt', 'auslassen', 'ausloesen', 'ausnahme', 'ausnahmen', 'ausnahmsweise', 'ausreichend', 'ausreichende', 'ausrichtung', 'ausschliesslich', 'ausschliessliche', 'aussen', 'ausser', 'ausserdem', 'ausserhalb', 'austauschen', 'auswaehlen', 'auswerten', 'auswertung', 'auswertungen', 'auswirkung', 'auswirkungen', 'authoritaet', 'authoritativen', 'authoritatives', 'autoritaet', 'autoritativ', 'autoritative', 'autoritativen', 'autoritativer', 'autoritatives', 'baeume', 'basiert', 'basierte', 'basierter', 'basisloesung', 'batchmodus', 'bauen', 'baum', 'baustein', 'bausteine', 'bausteinen', 'beachten', 'beachtung', 'bedeutend', 'bedeutet', 'bedeutsam', 'bedeutung', 'bedeutungen', 'bedingung', 'bedingungen', 'beeinflusst', 'beeinflusste', 'beenden', 'beendet', 'beendigung', 'befehl', 'befehle', 'befehlen', 'befehls', 'befuellung', 'beginn', 'beginnt', 'begrenzen', 'begrenzt', 'begrenzung', 'begriff', 'begriffe', 'begruendung', 'begruendungen', 'behaelt', 'behandlung', 'bei', 'beibehaelt', 'beibehalten', 'beide', 'beiden', 'beim', 'beinahe', 'beinhaltend', 'beinhaltet', 'beispiel', 'beispiele', 'beispielsweise', 'beispielweise', 'bekannt', 'bekannte', 'bekannten', 'belegen', 'belegt', 'bemerkung', 'bemerkungen', 'benoetigen', 'benoetigt', 'benoetigte', 'benoetigten', 'benutzbar', 'benutzbarkeit', 'benutzen', 'benutzer', 'benutzerdefiniert', 'benutzerdefinierte', 'benutzern', 'benutzt', 'beobachtbar', 'berechnen', 'berechnet', 'berechnung', 'bereich', 'bereiche', 'bereichs', 'bereichsgrenze', 'bereits', 'bericht', 'berichte', 'berichtsgate', 'beruecksichtigen', 'beruecksichtigt', 'beruehrt', 'beruht', 'beschaedigt', 'beschleunigen', 'beschleunigt', 'beschraenkt', 'beschreiben', 'beschreibt', 'beschreibung', 'beschreibungen', 'bestaetigen', 'bestaetigt', 'bestaetigte', 'bestaetigten', 'bestaetigter', 'bestaetigung', 'bestaetigungen', 'bestaetigungslinie', 'bestaetigungsprompt', 'bestaetigungszeile', 'bestanden', 'bestandsaufnahme', 'besteht', 'bestimmen', 'bestimmt', 'betrag', 'betretbar', 'betreuer', 'betriebsgroesse', 'betriebsgroessen', 'bevor', 'bewirken', 'bewirkt', 'bewusst', 'bewusster', 'bezeichnung', 'bezeichnungen', 'bezug', 'bibliothek', 'bibliotheken', 'binaer', 'binaerdatei', 'binaerdateien', 'binaere', 'binaeren', 'bindestrich', 'bindung', 'bis', 'bisher', 'bisherige', 'bisherigen', 'bitidentisch', 'bitte', 'bitvergleichbar', 'blankzeile', 'blankzeilen', 'blatt', 'blattknoten', 'bleiben', 'bleibt', 'blieb', 'blocke', 'blockieren', 'blockiert', 'bloecke', 'bloecken', 'bogenlaenge', 'bogensehne', 'boundaryvertraege', 'brauchen', 'braucht', 'breit', 'breite', 'breiten', 'buchstabe', 'buchstaben', 'bufferuebernahme', 'buildverzeichnis', 'buildvorgang', 'bytegenau', 'bytegenaue', 'bytegetreu', 'bytegetreue', 'bytegleich', 'byteidentisch', 'bzw', 'bzw.', 'ca', 'caller', 'cursorzustaende', 'd.h', 'da', 'dabei', 'dadurch', 'daempfen', 'daempfung', 'dafuer', 'dagegen', 'daher', 'dahinter', 'damit', 'danach', 'daneben', 'dann', 'daran', 'darauf', 'darf', 'darin', 'darstellbar', 'darstellung', 'darueber', 'darum', 'darunter', 'das', 'dass', 'dasselbe', 'datei', 'dateianteil', 'dateibehandlung', 'dateien', 'dateiende', 'dateiname', 'dateinamen', 'dateipfad', 'dateispitze', 'dateityp', 'dateitypen', 'dateityperkennung', 'datensaetze', 'datensatz', 'dauerhaft', 'dauerhafte', 'dauerhaften', 'dauerhafter', 'dauerhaftes', 'davon', 'davor', 'dazu', 'deaktivieren', 'deaktiviert', 'deaktivierung', 'defekt', 'definieren', 'definiert', 'definierte', 'dehnung', 'deklarator', 'deklaratorliste', 'deklorator', 'dekodieren', 'dem', 'den', 'denn', 'der', 'deren', 'dergleichen', 'derselbe', 'derselben', 'derzeit', 'derzeitige', 'des', 'deserialisiert', 'desselben', 'dessen', 'determinismus', 'deterministisch', 'deutlich', 'deutliche', 'deutsch', 'deutsche', 'deutschen', 'deutscher', 'deutsches', 'dezimal', 'dezimalexponent', 'dezimalpunkt', 'dezimalstellen', 'diagnose', 'diagnoseausgabe', 'diagnosekopien', 'diagnosen', 'diagnosestrom', 'dialoge', 'dichte', 'dick', 'dicke', 'dickenverteilung', 'die', 'diese', 'dieselbe', 'dieselben', 'diesem', 'diesen', 'dieser', 'dieses', 'dimensionsaehngige', 'direkt', 'displayrundung', 'dissipationsfunktion', 'divergenz', 'divergieren', 'divergiert', 'doku', 'dokument', 'dokumentation', 'dokumente', 'dokumentieren', 'dokumentiert', 'domaene', 'domaenen', 'domaenensumme', 'doppelpunkt', 'doppelt', 'doppelte', 'doppelten', 'dort', 'drehen', 'drehmoment', 'drei', 'dritte', 'dritten', 'druck', 'druckbeiwert', 'druckpunkt', 'druckschwerpunkt', 'druckt', 'druckverteilung', 'druckverteilungen', 'drueben', 'duenn', 'duerfen', 'dunkel', 'dupliziert', 'durch', 'durchfuehren', 'durchgefuehrt', 'durchgereicht', 'durchgeschaltet', 'durchlauf', 'durchreichung', 'durchreichwert', 'durchreichwerte', 'durchsetzung', 'ebene', 'ebenen', 'ebenfalls', 'ebenso', 'ebensolcher', 'eckenmittelung', 'eckknoten', 'eckwinkel', 'effekt', 'effekte', 'effektiv', 'ehemalig', 'ehemalige', 'ehemaligen', 'eigenen', 'eigenes', 'ein', 'eindeutig', 'eindeutige', 'eindeutigen', 'eindeutiges', 'eindeutigkeit', 'eine', 'einem', 'einen', 'einer', 'einfach', 'einfache', 'einfachen', 'einflussmatrix', 'einflusssystem', 'einfuegung', 'eingabe', 'eingabeaufforderung', 'eingabedatei', 'eingaben', 'eingaengen', 'eingehalten', 'eingelaufen', 'eingelesen', 'eingerueckt', 'eingeschaltet', 'eingeschwingt', 'eingeschwungen', 'eingesetzt', 'eingestellt', 'eingetragen', 'einhalten', 'einhaltung', 'einheit', 'einheiten', 'einheitlich', 'einheitliche', 'einheitlichen', 'einige', 'einlesen', 'einmal', 'einmalig', 'einmalige', 'einruecken', 'einrueckung', 'einrueckungen', 'einschaltmeldung', 'einschliesslich', 'einstellen', 'einstellung', 'einstellungen', 'eintraege', 'eintrag', 'eintrage', 'eintragen', 'einzelfallaufuehrung', 'einzelgenauigkeit', 'einzelne', 'einzelnen', 'einzig', 'einzige', 'einzigen', 'einziges', 'elternverzeichnis', 'emittieren', 'emittiert', 'emittierte', 'empfaenger', 'empfindlich', 'empirisch', 'ende', 'endet', 'endgueltig', 'endgueltige', 'endlich', 'endlichkeitspruefung', 'endwert', 'endwerte', 'energie', 'entfaellt', 'entfallen', 'entfernen', 'entfernt', 'entfernte', 'entfernten', 'entfernter', 'entferntes', 'entfernung', 'entfernungen', 'entgegen', 'enthaelt', 'enthalten', 'entkoppelt', 'entlang', 'entscheiden', 'entscheidet', 'entscheidung', 'entscheidungen', 'entsprechen', 'entsprechende', 'entsprechender', 'entspricht', 'entwickler', 'entwurf', 'er', 'ereignisse', 'ereignisstrom', 'erfasst', 'erfolg', 'erfolgreich', 'erfolgsmeldung', 'erfolgszustand', 'erforderlich', 'erforderliche', 'erforderlichen', 'erfordert', 'erfuellen', 'erfuellt', 'ergab', 'ergaenzen', 'ergaenzt', 'ergaenzte', 'ergaenzung', 'ergaenzungen', 'ergeben', 'ergebnis', 'ergebnisse', 'ergebnisses', 'ergibt', 'erhaelt', 'erhalten', 'erhielt', 'erhoehen', 'erhoeht', 'erkannt', 'erkennbar', 'erkennen', 'erklaerbar', 'erklaeren', 'erklaert', 'erklaerung', 'erklaerungen', 'erlaubt', 'erledigt', 'ermoeglichen', 'ermoeglicht', 'erneut', 'erreichbar', 'erreichbarkeit', 'erreichen', 'erreicht', 'erreichte', 'ersetzbar', 'ersetzen', 'ersetzt', 'ersetzte', 'ersetzten', 'ersetzung', 'ersetzungen', 'erst', 'erste', 'ersten', 'erstmalig', 'erwarten', 'erwartet', 'erwartete', 'erwarteten', 'erwartung', 'erwartungen', 'erwartungshorizont', 'erwartungswert', 'erweiterte', 'erweiterung', 'erweiterungen', 'erzeugen', 'erzeugt', 'erzeugte', 'erzwingen', 'erzwungen', 'etwa', 'exakt', 'exakte', 'exakten', 'existenzpruefung', 'existieren', 'existiert', 'explizit', 'exponentiell', 'extern', 'extrapolieren', 'extrapoliert', 'fachlich', 'faelle', 'faellen', 'faellt', 'faelschlich', 'faengt', 'faktor', 'faktoren', 'faktorisierung', 'fall', 'fallaufloesung', 'falldir', 'fallname', 'fallnamen', 'fallpfad', 'falls', 'falsch', 'falsche', 'falschen', 'fassade', 'fassaden', 'fast', 'fehlen', 'fehlend', 'fehlende', 'fehlendem', 'fehlenden', 'fehlender', 'fehlendes', 'fehler', 'fehler:', 'fehlerbehaftet', 'fehlerfaelle', 'fehlerfall', 'fehlerfrei', 'fehlerhaft', 'fehlerhafte', 'fehlermeldung', 'fehlermeldungen', 'fehlern', 'fehlerpfad', 'fehlert', 'fehlerzustand', 'fehlgeschlagen', 'fehlgeschlagene', 'fehlgeschlagenen', 'fehlgeschlagener', 'fehlschlaege', 'fehlschlag', 'fehlt', 'fehlverhalten', 'feld', 'feldbreite', 'felddicke', 'felder', 'feldes', 'feldgroesse', 'feldgroessen', 'feldnutzung', 'feldzugriffe', 'feldzugriffen', 'fertig', 'fest', 'feste', 'festen', 'festes', 'festformat', 'festgelegt', 'festgestellt', 'festlegen', 'festlegung', 'feststellen', 'feststellt', 'finalisieren', 'finden', 'findet', 'flaeche', 'flaechen', 'fliessen', 'fokussiert', 'fokussierte', 'fokussierten', 'folge', 'folgegruppe', 'folgegruppen', 'folgende', 'folgender', 'folgendes', 'formate', 'formatierung', 'formatzeile', 'formelgetreu', 'formelgetreue', 'fortfahren', 'fortfuehren', 'fortfuehrt', 'fortgefuehrt', 'fortgesetzt', 'fortlauftext', 'fortranquelle', 'fortranquellen', 'fortschreibung', 'fortsetzen', 'fortsetzung', 'frage', 'fragen', 'freheitsgrad', 'frei', 'freie', 'freiem', 'freien', 'freier', 'freies', 'freigegeben', 'freiheitsgrade', 'freue', 'frueher', 'fruehere', 'frueheren', 'frueherer', 'fruehestens', 'fuehren', 'fuehrende', 'fuehrendem', 'fuehrenden', 'fuehrender', 'fuehrendes', 'fuehrt', 'fuehrte', 'fuehrten', 'fuellt', 'fuellung', 'fuenf', 'fuer', 'funf', 'funktion', 'funktionalitaet', 'funktionen', 'gab', 'gaengig', 'gaenzlich', 'ganze', 'ganzen', 'ganzes', 'garantie', 'garantien', 'geaendert', 'geaenderte', 'geben', 'gedaempft', 'gedreht', 'gedruckt', 'geeignet', 'geeignete', 'gefuehrt', 'gefuellt', 'gefuert', 'gefunden', 'gefundenen', 'gegeben', 'gegen', 'geglaettet', 'geglaettete', 'geglaetteter', 'gehoert', 'geklaert', 'gekoppelt', 'gekuerzt', 'geladen', 'gelegentlich', 'geleitet', 'gelesen', 'gelesene', 'geliefert', 'gelingt', 'gelinkt', 'geloescht', 'geloeschte', 'geloeschten', 'geloeschtes', 'geloest', 'gelungen', 'gemacht', 'gemaess', 'gemeinsam', 'gemeinsame', 'gemeinsamen', 'gemeinsamer', 'gemessen', 'gemischt', 'genaehert', 'genau', 'genauigkeit', 'genauigkeitsanforderung', 'genauigkeitsverlust', 'genauso', 'genehmigt', 'genommen', 'geometrie', 'geometriegroesse', 'geometrien', 'geometrienachbereitung', 'geometrierekord', 'geometrisch', 'geparst', 'geprueft', 'gepruefte', 'geprueften', 'geroutet', 'gerundet', 'gesamt', 'gesamtbaum', 'gesamten', 'gesamter', 'gesamtzahl', 'geschrieben', 'geschwindigkeit', 'geschwindigkeiten', 'geschwister', 'gesetzt', 'gespeichert', 'gespeicherte', 'gesperrt', 'gespiegelt', 'gestartet', 'gestoppt', 'getauscht', 'geteilt', 'geteilte', 'geteilten', 'getrennt', 'getrennte', 'getrennten', 'getriggert', 'gewaehrleisten', 'gezaehlt', 'gezaehlter', 'gezeigt', 'ggf', 'gibt', 'giermoment', 'glaetten', 'glatt', 'gleich', 'gleiche', 'gleichem', 'gleichen', 'gleicher', 'gleiches', 'gleichheit', 'gleichrangig', 'gleichsetzung', 'gleichung', 'gleichungen', 'gleichungsloeser', 'gleichwertig', 'globale', 'globalen', 'goldene', 'goldener', 'goldenes', 'grad', 'grenze', 'grenzen', 'grenzfaelle', 'grenzfall', 'grenzschicht', 'grenzschichtdaten', 'grenzwert', 'grenzwerte', 'groesse', 'groessen', 'groessenordnung', 'groesser', 'groesserer', 'groesstenteils', 'gross', 'grossbuchstaben', 'gruen', 'gruen;', 'gruenden', 'gruene', 'gruenen', 'gruengate', 'gueltig', 'gueltige', 'gueltigem', 'gueltigen', 'gueltiger', 'gueltiges', 'gueltigkeit', 'gueltigkeitsvertrag', 'haben', 'haelt', 'haengt', 'haette', 'haeufig', 'handbuch', 'handgeschrieben', 'handgeschriebene', 'handgeschriebenen', 'handlerlaeufe', 'handlerlauf', 'hangt', 'hart', 'hartes', 'hat', 'hatte', 'hatten', 'haupt', 'hauptsachlich', 'hauptsaechlich', 'hauptteil', 'headerdatei', 'headerdateien', 'heil', 'hell', 'hervorgerufen', 'hier', 'hierbei', 'hierfuer', 'hilfe', 'hilfreich', 'hilfs', 'hilfsfunktion', 'hilfsfunktionen', 'hinreichend', 'hinter', 'hinterkante', 'hinterkantmeldung', 'hinweis', 'hinweise', 'hinweisen', 'hinweist', 'hinzu', 'historisch', 'historische', 'hoch', 'hochkomma', 'hoechstens', 'hoeher', 'identisch', 'identische', 'identischen', 'identischer', 'identitaet', 'ihr', 'ihre', 'ihrem', 'ihren', 'im', 'immer', 'implementiert', 'implementierung', 'impuls', 'inaktiv', 'inaktive', 'inaktiven', 'inaktives', 'indexaufloesung', 'indexierung', 'indizes', 'initialisieren', 'initialisiert', 'initialisierung', 'inkrement', 'innerhalb', 'innerlich', 'innerliche', 'insgesamt', 'instanzbezogen', 'instationaer', 'intakt', 'integriert', 'integrierte', 'interaktiv', 'interaktiver', 'intern', 'interne', 'internen', 'interpolieren', 'interpoliert', 'invalidieren', 'invalidiert', 'invalidierung', 'invalidierungen', 'inventar', 'inventare', 'inventarisierung', 'inversen', 'inverser', 'inverses', 'invertieren', 'invertiert', 'inviscidloesung', 'inviskide', 'irgendwo', 'isoliert', 'ist', 'iterationen', 'jede', 'jeder', 'jedes', 'jeweilige', 'jeweiligen', 'jeweils', 'kann', 'kanonisch', 'kanonische', 'kanonischen', 'kanonischer', 'kanonisches', 'kanten', 'kapazitaet', 'kapazitaets', 'kapazitaetsgrenze', 'kapazitaetsmeldung', 'kapazitaetsmeldungen', 'kaputt', 'katalog', 'kataloge', 'kategorie', 'kein', 'keine', 'keinem', 'keinen', 'keiner', 'kennung', 'kennungen', 'kennzeichnung', 'kern', 'kerne', 'kernen', 'kernpunkt', 'kind', 'kinder', 'kippen', 'klammer', 'klammeraffe', 'klammerinhalt', 'klammern', 'klammerung', 'klammerungstiefe', 'klappe', 'klappen', 'klar', 'klare', 'klasse', 'klassen', 'klassifikation', 'klein', 'kleinbuchstaben', 'kleiner', 'klemmung', 'knapp', 'knoten', 'kodierung', 'koeffizient', 'koeffizienten', 'koennen', 'koennte', 'koennten', 'komentare', 'komma', 'kommando', 'kommandoname', 'kommandonamen', 'kommandos', 'kommastelle', 'kommastellen', 'kommentar', 'kommentare', 'kommentaren', 'kommentarzeile', 'kommentarzeilen', 'kompatibilitaets', 'kompatibilitaetsarrays', 'kompatibilitaetsfunktionen', 'kompilierbar', 'kompiliert', 'komplett', 'komplex', 'komplexe', 'komplexen', 'kompressibilitaet', 'konfiguration', 'konfigurieren', 'konfiguriert', 'konflikt', 'konflikte', 'konform', 'konformitaet', 'konsistent', 'konsistente', 'konsistenten', 'konsistenz', 'konsistenzpruefung', 'konstant', 'konstante', 'konstanten', 'konstruktionen', 'kontrolle', 'kontrollen', 'kontrolliert', 'kontur', 'konturknotenzahl', 'konturpunkt', 'konturpunkte', 'konturpunkten', 'konvergenz', 'konvergenzmeldung', 'konvergenzmeldungen', 'konvergieren', 'konvergierende', 'konvergiert', 'konvertierung', 'konzept', 'kopf', 'kopfdaten', 'kopfpruefung', 'kopfzeile', 'kopierbar', 'kopieren', 'kopiert', 'koppler', 'kopplung', 'kopplungen', 'korrekt', 'korrekte', 'korrekten', 'kraefte', 'kraft', 'kreisabbildung', 'kreisgitter', 'kriterien', 'kriterium', 'kruemmung', 'kubisch', 'kuenftig', 'kuenftige', 'kuenstlich', 'kuenstliche', 'kuerzen', 'kuerzer', 'kuerzt', 'kuerzung', 'kurz', 'ladbar', 'laden', 'laedt', 'laenge', 'laengen', 'laengendifferenz', 'laenger', 'laengstens', 'laesst', 'laeufe', 'laeuft', 'lage', 'lang', 'langsam', 'lassen', 'lauf', 'laufen', 'laufzeit', 'laufzeitartefakt', 'laut', 'lediglich', 'leer', 'leere', 'leeren', 'leerer', 'leeres', 'leerzeichen', 'leerzeile', 'leerzeilen', 'leicht', 'leistung', 'leiten', 'leitet', 'lesbar', 'lesbarkeit', 'lesefehler', 'lesen', 'letzte', 'letzten', 'letzter', 'lief', 'liefert', 'lieferte', 'liegt', 'liest', 'linkbar', 'linken', 'linkt', 'liste', 'listen', 'lizenzkopf', 'locker', 'loeschbar', 'loeschbarkeit', 'loeschen', 'loescht', 'loeschte', 'loesen', 'loeser', 'loest', 'loesung', 'loesungen', 'logarithmisch', 'logdatei', 'lokal', 'lokale', 'lokalen', 'lokalisiert', 'machen', 'macht', 'machzahl', 'man', 'manche', 'manchmal', 'manifeste', 'manuell', 'markierung', 'massgeblich', 'massgebliche', 'massgeblichen', 'massgebliches', 'mathematisch', 'matrixgroesse', 'matrizen', 'maximale', 'mechanismen', 'mechanismus', 'mehrere', 'mehrfach', 'mehrheit', 'mehrmals', 'meist', 'meisten', 'meistens', 'meldet', 'meldung', 'meldungen', 'menge', 'mengen', 'menue', 'menues', 'menueschleife', 'menuezustand', 'merken', 'merkt', 'messen', 'messgroesse', 'messgroessen', 'messstelle', 'messstellen', 'methode', 'methoden', 'methodenaufrufe', 'methodennutzung', 'migrationsinventare', 'milliarde', 'millionen', 'minderheit', 'mindestens', 'mischen', 'misst', 'mit', 'mitgezaehlt', 'mithilfe', 'mitteilung', 'mitteilungen', 'mittel', 'mittelbar', 'mittelbare', 'mittelbereich', 'mittels', 'mittelung', 'modul', 'moduls', 'modus', 'moeglich', 'moegliche', 'moeglichen', 'monoton', 'muessen', 'muesste', 'multiplikativ', 'muss', 'muster', 'mutationen', 'mutiert', 'mutierte', 'mutierten', 'nach', 'nachbereitung', 'nachbildung', 'nachdem', 'nachfolgende', 'nachfolger', 'nachfolgern', 'nachgefuehrt', 'nachgelagert', 'nachgewiesen', 'nachher', 'nachkommastelle', 'nachkommastellen', 'nachtraeglich', 'nachvollziehbar', 'nachvollziehbarkeit', 'nachvollziehen', 'naechste', 'naechsten', 'naechster', 'naechstes', 'naemlich', 'nahe', 'naherungsweise', 'nahezu', 'namen', 'namens', 'namenskonflikt', 'namensraum', 'namlich', 'nase', 'nativen', 'nebenbedingung', 'nebenbedingungen', 'negativcheck', 'negativfaelle', 'negativprobe', 'nehmen', 'neigung', 'neu', 'neue', 'neuem', 'neuen', 'neuer', 'neutrale', 'neutralen', 'neutraler', 'neutrales', 'nicht', 'nichtlinear', 'nichtverfuegbarkeit', 'nickmoment', 'nie', 'niedrig', 'niemals', 'nimmt', 'nirgends', 'noch', 'nocheinmal', 'nochmals', 'noetig', 'normalisierung', 'normierung', 'notwendig', 'nuetzlich', 'null', 'nullauffuellung', 'numerisch', 'numerische', 'numerischem', 'numerischen', 'numerischer', 'numerisches', 'nummer', 'nur', 'nutzbar', 'nutzbarkeit', 'nutzen', 'nutzt', 'nutzte', 'nutzung', 'ob', 'oben', 'oberflaeche', 'oberflaechen', 'obgleich', 'obige', 'objekt', 'objektbaum', 'objektdatei', 'objekte', 'obwohl', 'oder', 'oeffentlich', 'oeffentliche', 'oeffentlichen', 'oeffentlicher', 'oeffentliches', 'oefter', 'offen', 'offene', 'offenen', 'oft', 'ohne', 'operationale', 'operationen', 'optionen', 'orakel', 'orchestrieren', 'orchestriert', 'ordner', 'ordnung', 'orientierung', 'originalartefakt', 'originalartefakte', 'originalartefakts', 'originalquelle', 'originalquellen', 'ort', 'orts', 'outputbefehle', 'panelanzahl', 'panelgeometrie', 'panelierung', 'panelisierung', 'panelkontur', 'panelneuerzeugung', 'paneltangentialgeschwindigkeit', 'parametern', 'parameterverzweigung', 'parsen', 'passen', 'passend', 'passlimit', 'passt', 'periodisch', 'pfad', 'pfade', 'pfaden', 'pflicht', 'phasen', 'physikalisch', 'plaene', 'platz', 'platzhalter', 'plausibel', 'plausibilitaet', 'ploten', 'plotliste', 'polarachse', 'polarachsen', 'polarakkumulation', 'polarbefehl', 'polarbefehle', 'polarblock', 'polarblocke', 'polardatei', 'polardateien', 'polare', 'polaren', 'polarformat', 'polarindex', 'polarindizes', 'polarkurve', 'polarlaeufe', 'polarlauf', 'polarliste', 'polarlisten', 'polarmenue', 'polarmenues', 'polarname', 'polaroperation', 'polaroperationen', 'polaroption', 'polaroptionen', 'polarparameter', 'polarpunkt', 'polarpunkte', 'polarsumme', 'polarsummen', 'polartext', 'polarwert', 'polarwerte', 'polarzustand', 'polarzustande', 'portiert', 'postprozessierung', 'praefix', 'praefixtransformation', 'praefixvertrag', 'praezision', 'praktisch', 'primaer', 'primaere', 'primaeren', 'primaerwerte', 'privat', 'privaten', 'pro', 'probeweise', 'probleme', 'produkt', 'produktbanner', 'produktcode', 'produkte', 'produktiv', 'produktive', 'produktiven', 'produktiver', 'produktives', 'produktivpfad', 'produktname', 'produkttext', 'profil', 'profilen', 'profilname', 'profilnase', 'profils', 'projektkopf', 'protokoll', 'prozedur', 'prozeduren', 'prozent', 'prozentual', 'pruefe', 'pruefen', 'pruefsumme', 'pruefsummen', 'prueft', 'pruefte', 'pruefung', 'pruefungen', 'puffer', 'puffern', 'puffers', 'pufferzeiger', 'punkt', 'punkte', 'punkten', 'punktoperation', 'punktzahl', 'quadatisch', 'qualitaetsgate', 'queldatei', 'queldateien', 'quell', 'quellcode', 'quelldatei', 'quelldateien', 'quelle', 'quellen', 'quellkontur', 'quellmenue', 'quellscan', 'quelltext', 'quellverteilung', 'quer', 'raender', 'raeumt', 'rand', 'randbedingung', 'randbedingungen', 'randfaelle', 'randfall', 'rang', 'rau', 'raute', 'realistisch', 'rechenkern', 'rechnerisch', 'rechnung', 'rechnungen', 'rechtfertigung', 'recordgroesse', 'recordlaenge', 'redaktion', 'redozustaende', 'reduziert', 'referenz', 'referenzbasis', 'referenzdatei', 'referenzdateien', 'referenzdaten', 'referenzen', 'referenzfaelle', 'referenzfaellen', 'referenzfall', 'referenzlauf', 'referenzpfad', 'referenzpolar', 'referenzquelle', 'referenzquellen', 'referenzwert', 'referenzwerte', 'regel', 'regelcode', 'regelfaelle', 'regelfall', 'regelkonform', 'regeln', 'registrieren', 'registriert', 'regressionssupport', 'regressionstest', 'regressionstests', 'regulaer', 'regulaerem', 'reibung', 'reihe', 'reihen', 'reihenfolge', 'reihenfolgenabhaengigkeit', 'rein', 'relativ', 'reoriginierung', 'repositorywurzel', 'repraesentation', 'repraesentationen', 'reproduzierbar', 'reproduzierbare', 'reproduzierbaren', 'reproduzierbarer', 'reproduzierbarkeit', 'reservieren', 'reserviert', 'revisionsbindung', 'reynoldszahl', 'richtig', 'richtige', 'richtigen', 'richtigkeit', 'richtung', 'richtungen', 'rief', 'riefen', 'roheintraege', 'rollmoment', 'routinen', 'routinenliste', 'rueckbau', 'rueckgabe', 'rueckgabewert', 'rueckgabewerte', 'rueckkehr', 'rueckmeldung', 'rueckmeldungen', 'rueckparsung', 'rueckschreiben', 'rueckschreibung', 'ruecksetzen', 'ruecksetzung', 'ruecksprung', 'ruecksubstitution', 'rueckwaerts', 'ruft', 'rund', 'runden', 'rundet', 'rundung', 'runtimepfad', 'saemtliche', 'sammlung', 'schaegstrich', 'schaetzung', 'schaetzwert', 'schallgeschwindigkeit', 'schalter', 'scharf', 'scheitern', 'scheitert', 'schicht', 'schichten', 'schlaegt', 'schleife', 'schleifen', 'schleifengrenze', 'schliesslich', 'schluessel', 'schluesselwoerter', 'schluesselwort', 'schmal', 'schnell', 'schnellstart', 'schnittstelle', 'schon', 'schraegstrich', 'schreibbar', 'schreibbarkeit', 'schreiben', 'schreiborientierung', 'schreibt', 'schritt', 'schrittdatei', 'schritte', 'schrittweite', 'schub', 'schuetzen', 'schuetzt', 'schutz', 'schutzgate', 'schwach', 'schwelle', 'schwellenwert', 'schwellenwerte', 'schwer', 'schwerpunkt', 'sehne', 'sehr', 'sei', 'seien', 'sein', 'seine', 'seit', 'seite', 'seiten', 'seitenaarrays', 'seitenanzahl', 'seiteneffekt', 'seiteneffekte', 'seitenlaenge', 'seitenrekord', 'seitenweise', 'sekundaeren', 'selbe', 'selben', 'selbst', 'selbststaendigkeits', 'selbsttest', 'selten', 'senke', 'senkrecht', 'sensitivitaet', 'sensitivitaeten', 'sentinelplaetze', 'separat', 'sequenz', 'sequenzen', 'serialisiert', 'sessionbeendigung', 'sessiongebundene', 'sessiongebundenen', 'sessiongebundener', 'sessiongebundenes', 'sessionkomposition', 'sessionmutation', 'sessionseitig', 'sessionunabhaengigkeit', 'setzen', 'setzt', 'sich', 'sicherheit', 'sicherheiten', 'sicherstellen', 'sicherung', 'sichtbar', 'sichtbare', 'sichtbaren', 'sie', 'siehe', 'signale', 'signaturaenderung', 'signifikante', 'sind', 'sinkt', 'sinnvoll', 'sinnvolle', 'sinnvollen', 'sitzung', 'sitzungen', 'sitzungszustand', 'skalar', 'skalieren', 'skaliert', 'skalierung', 'skript', 'skripte', 'sobald', 'sofort', 'sofortig', 'sog', 'sogenannte', 'sogenannten', 'solange', 'solche', 'solcher', 'soll', 'sollte', 'sollten', 'sollwert', 'solverarrays', 'solverzustaende', 'somit', 'sonderbehandlung', 'sondercode', 'sonderfaelle', 'sonderfall', 'sonderloesung', 'sondermeldung', 'sondern', 'sonderpfad', 'sonderpfade', 'sonderzeichen', 'sonst', 'sortieren', 'sortiert', 'sortierte', 'sortierung', 'soweit', 'spa', 'spaeter', 'spaetere', 'spaeteren', 'spaeterer', 'spaetestens', 'spaetzung', 'spalte', 'spalten', 'spaltenbreite', 'spannung', 'spar', 'speicher', 'speichern', 'speichert', 'spezifischer', 'spiegeln', 'spiegelt', 'spiegelung', 'springen', 'springt', 'sprung', 'stabil', 'staendig', 'staerker', 'stammt', 'standard', 'standardcode', 'standardfaelle', 'standardfall', 'standardmaessig', 'standardmaessige', 'standardmaessigen', 'standardwert', 'standardwerte', 'standig', 'stark', 'startbar', 'starten', 'stationaer', 'stationen', 'stationierung', 'stationsanzahl', 'statischen', 'statt', 'stattdessen', 'statusse', 'stauchung', 'staupunkt', 'staupunkte', 'stehen', 'steigt', 'steigung', 'steigungen', 'stelle', 'stellen', 'stellenwert', 'stellt', 'sternchen', 'steuert', 'stimmen', 'stimmt', 'stoppen', 'straff', 'strategie', 'strategien', 'strich', 'stringliteral', 'strom', 'stromab', 'stromabwaerts', 'stromauf', 'stromaufwaerts', 'stromlinie', 'stromlinien', 'struktur', 'strukturen', 'strukturfortschreibung', 'strukturierte', 'stumm', 'stumpf', 'summe', 'symbolunterdrueckung', 'symmetrie', 'symmetrisch', 'tabelle', 'tabellen', 'tabellenwert', 'tangentialgeschwindigkeit', 'tatsachewert', 'tatsaechlich', 'tatsaechliche', 'tatsaechlichen', 'tauscht', 'tausend', 'teil', 'teilbaeume', 'teilbaum', 'teile', 'teilen', 'teilruecksetzung', 'teilschritt', 'teilt', 'teilweise', 'teilweiser', 'teilzustand', 'teilzustande', 'teilzustanden', 'temperatur', 'temporaer', 'temporaere', 'temporaeren', 'temporaeres', 'testarbeitsverzeichnis', 'testarbeitverzeichnis', 'testdatei', 'testdaten', 'testergebnis', 'testergebnisse', 'testfaelle', 'testfaellen', 'testfall', 'testfallbezeichnung', 'testgestuetzt', 'testlaeufe', 'testlauf', 'testmatrix', 'testobjekt', 'testobjekte', 'testsuite', 'testverzeichnis', 'testweise', 'textformatierung', 'textzeile', 'textzeilen', 'theoretisch', 'tief', 'tiefe', 'tiefer', 'tiefkomma', 'toggelt', 'toggelte', 'toleranz', 'toleranzen', 'toleranzerfuellung', 'toleranzerhoehung', 'traegt', 'tragen', 'tranent', 'transformieren', 'transformiert', 'transformierte', 'transkript', 'transkripte', 'transkripttext', 'transliterieren', 'transliterierte', 'transliterierter', 'transliteriertes', 'trennen', 'trenner', 'trennpunkt', 'trennpunkte', 'trennung', 'trennzeichen', 'trimmung', 'trotz', 'truibe', 'truueb', 'typ', 'typdeklarationen', 'typs', 'ua', 'ueber', 'ueberall', 'ueberblick', 'ueberein', 'uebereinstimmen', 'uebereinstimmend', 'uebereinstimmende', 'uebereinstimmt', 'uebereinstimmung', 'ueberfluessig', 'uebergab', 'uebergabe', 'uebergang', 'uebergange', 'uebergangen', 'uebergangs', 'uebergangspunkt', 'uebergeben', 'uebergebene', 'uebergebenen', 'uebergeordnet', 'uebergibt', 'ueberhaupt', 'ueberlagern', 'ueberlagert', 'ueberlagerung', 'ueberlauf', 'uebernahme', 'uebernahmen', 'uebernehmen', 'uebernimmt', 'uebernommen', 'uebernommene', 'ueberpruefen', 'ueberprueft', 'ueberpruefung', 'ueberschreibabfrage', 'ueberschreibablehnung', 'ueberschreiben', 'ueberschreibprompt', 'ueberschreibt', 'ueberschreibung', 'ueberschreiten', 'ueberschreitet', 'ueberschreitung', 'ueberschrieben', 'ueberschritt', 'uebersetzbar', 'uebersetzten', 'uebersetzung', 'uebersetzungen', 'ueberspringen', 'ueberspringt', 'uebersprungen', 'uebersteigen', 'uebersteigt', 'uebertragen', 'uebertragene', 'uebertragung', 'uebertrugen', 'ueberwachung', 'ueberwiegend', 'ueblich', 'uebrig', 'uebrigbleibend', 'uebrigbleibende', 'uebrigbleibenden', 'uebrigbleibt', 'uebrige', 'uebrigen', 'um', 'umbenannt', 'umbenennung', 'umdbruch', 'umfang', 'umgebungs', 'umgekehrt', 'umgekehrte', 'umgekehrten', 'umgerechnet', 'umgeschaltet', 'umgestellt', 'umgewandelt', 'umlaufrichtung', 'umrechnen', 'umrechnung', 'umschalten', 'umschaltpunkt', 'umschaltung', 'umschlag', 'umschlagkurve', 'umwandeln', 'unabhaengig', 'unabhaengige', 'unabhaengigkeit', 'unbedeutend', 'unbekannt', 'unbekannte', 'unbekannten', 'unberuehrt', 'unbeschaedigt', 'unbeteiligte', 'unbeteiligter', 'unbrauchbar', 'und', 'uneinheitlich', 'ungeaenderte', 'ungeeignet', 'ungefaehr', 'ungefaehre', 'ungefaehren', 'ungenuegend', 'ungeroutet', 'ungewoehnlich', 'ungewoehnliche', 'ungleich', 'ungueltig', 'ungueltige', 'ungueltigem', 'ungueltigen', 'ungueltiger', 'ungueltigkeit', 'unklar', 'unmittelbar', 'unmittelbare', 'unmittelbaren', 'unmittelbares', 'unplausibel', 'unqualifizierte', 'unrealistisch', 'unrein', 'unser', 'unsinnig', 'unten', 'unter', 'unterdrueckt', 'untergeordnet', 'unterscheidbar', 'unterscheiden', 'unterschied', 'unterschiede', 'unterschreiten', 'unterschreitet', 'unterschreitung', 'unterstrich', 'unterstuetzen', 'unterstuetzt', 'unterstuetzten', 'unterstuetztes', 'unterstuetzung', 'unveraendert', 'unveraenderte', 'unveraenderten', 'unveraenderter', 'unveraendertes', 'unverfuegbar', 'unverzueglich', 'unvollstaendig', 'unwichtig', 'unzulaessig', 'ursache', 'ursachen', 'urspruenglich', 'urspruengliche', 'urspruenglichen', 'urspruengliches', 'ursprung', 'usf', 'usw', 'validieren', 'validiert', 'validierung', 'variablen', 'vektor', 'vektoren', 'veraenderbaren', 'veraenderbarer', 'veraenderbarkeit', 'veraenderlich', 'veraendern', 'veraendert', 'veraenderte', 'veraendertem', 'veraenderung', 'veraenderungen', 'veraltet', 'veraltete', 'veralteten', 'verbieten', 'verbindlich', 'verbindliche', 'verbindlichen', 'verbindlicher', 'verbleiben', 'verbleibend', 'verbleibende', 'verbleibenden', 'verbleibendes', 'verbleibt', 'verbliebene', 'verblochen', 'verblockt', 'verborgen', 'verbot', 'verboten', 'verbotene', 'verbotenen', 'verbotener', 'verbotenes', 'verbunden', 'verdraengung', 'verdraengungsdicke', 'vereinfachen', 'vereinfacht', 'verfeinerung', 'verfuegbar', 'verfuegbare', 'verfuegbarkeit', 'vergleich', 'vergleichbar', 'vergleichbare', 'vergleichbarkeit', 'vergleichen', 'vergleichsdatei', 'vergleicht', 'verglichen', 'verhaeltnis', 'verhaeltnisse', 'verhalten', 'verhindern', 'verhindert', 'verlaesst', 'verlangsamen', 'verlangsamt', 'verlassen', 'verlauf', 'verletzen', 'verletzt', 'verletzte', 'verletzung', 'verletzungen', 'vermerkt', 'vermittels', 'vermutung', 'vermutungen', 'vernachlaessigbar', 'verpflichtend', 'verpflichtende', 'verschachtelt', 'verschieben', 'verschiebt', 'verschiebung', 'verschieden', 'verschiedene', 'verschiedenen', 'verschmelzen', 'verschmolzen', 'verschoben', 'verstaerkung', 'versteckt', 'versteckte', 'versteckter', 'verstecktes', 'verstoess', 'verstoesse', 'verstoessen', 'verstoesst', 'verstoss', 'versuchsweise', 'verteilung', 'verteilungen', 'vertraege', 'vertraegen', 'vertrag', 'vertragstest', 'verursachen', 'verursacht', 'verwandeln', 'verweigern', 'verweigert', 'verweigerung', 'verweis', 'verwenden', 'verwendet', 'verwendete', 'verwendeten', 'verwendung', 'verwerfen', 'verwirft', 'verworfen', 'verzeichnis', 'verzeichnisse', 'verzeichnissen', 'verzweigung', 'verzweigungen', 'viskos', 'viskose', 'viskositaet', 'viskositat', 'voll', 'volle', 'vollen', 'vollstaendig', 'vollstaendige', 'vollstaendigem', 'vollstaendigen', 'vollstaendiger', 'vollstaendigkeit', 'vollziehbar', 'von', 'vor', 'voraussetzung', 'voraussetzungen', 'vorbedingung', 'vorbereiten', 'vorbereitet', 'vorbereitung', 'vorderkante', 'voreingestellt', 'voreingestellte', 'vorgabe', 'vorgaben', 'vorgaenger', 'vorgaengern', 'vorgehen', 'vorgehensweise', 'vorgelagert', 'vorgeschrieben', 'vorgesehen', 'vorgesehene', 'vorhanden', 'vorhandene', 'vorhandenen', 'vorher', 'vorherige', 'vorherigen', 'vorhin', 'vorige', 'vorkommen', 'vorlaeufig', 'vorlaeufige', 'vormals', 'vorschreiben', 'vorschreitet', 'voruebergehend', 'voruebergehende', 'vorverlegt', 'vorwaerts', 'vorwaertsdeklaration', 'vorzeichen', 'waehlt', 'waehrend', 'waende', 'waerme', 'wagenruecklaufzeichen', 'waichtig', 'wand', 'wandelt', 'war', 'waren', 'warnung', 'wartungshinweis', 'wegen', 'weggelassen', 'weglassen', 'weich', 'weichen', 'weil', 'weite', 'weiter', 'weitere', 'weiterer', 'weiteres', 'weitergegeben', 'weitergereicht', 'weiterhin', 'weiterleiten', 'weiterleitung', 'weiterleitungsheader', 'welche', 'welchem', 'welchen', 'welcher', 'welches', 'wenn', 'werden', 'werkzeug', 'werkzeuge', 'wert', 'werte', 'wertebereich', 'werten', 'wertetyp', 'wertpins', 'werttypen', 'wesentlich', 'wesentliche', 'wich', 'wichtig', 'wichtige', 'wichtigen', 'widerspruch', 'widersprueche', 'widerspruechlich', 'widerstand', 'wie', 'wieder', 'wiederauftaucht', 'wiederbenutzen', 'wiedergewinnen', 'wiederhergestellt', 'wiederherstellen', 'wiederherstellung', 'wiederholen', 'wiederholt', 'wiederholte', 'wiederholter', 'wiederholtes', 'wiederholung', 'wiederholungen', 'wiederholungs', 'wiederladen', 'wiederverwenden', 'wiederverwendet', 'wiederverwendung', 'winkel', 'winkeln', 'wir', 'wirbel', 'wirbelstaerke', 'wird', 'wirksam', 'wirkung', 'wirkungslos', 'woeblung', 'woelbung', 'woelbungslinie', 'woelbungsverteilung', 'woerter', 'wort', 'wortern', 'wortlaut', 'wortlautlich', 'wortlich', 'wortteil', 'wortteile', 'wuerde', 'wurde', 'wurden', 'wurzel', 'z.b', 'zaehlen', 'zaehler', 'zaehlregel', 'zaehlt', 'zaehlung', 'zahl', 'zahlen', 'zahlenformat', 'zahlreiche', 'zeichen', 'zeichenanzahl', 'zeichenfolge', 'zeichenkette', 'zeichenketten', 'zeichensatz', 'zeichenzahl', 'zeigen', 'zeiger', 'zeile', 'zeilen', 'zeilenanfang', 'zeilenende', 'zeilenevidenz', 'zeilenlaenge', 'zeilennummer', 'zeilenueberhang', 'zeilenumbruch', 'zeilenweise', 'zeit', 'zentral', 'zentrale', 'zerlegung', 'zerstoert', 'ziel', 'ziele', 'zielverzeichnis', 'zielwert', 'ziffer', 'ziffern', 'ziffernzugriff', 'zu', 'zuerst', 'zugriff', 'zugriffe', 'zugriffen', 'zugunsten', 'zukuenftig', 'zukuenftigen', 'zulaessig', 'zulaessige', 'zulasten', 'zuletzt', 'zum', 'zumal', 'zunaechst', 'zunahme', 'zuordnung', 'zuordnungen', 'zur', 'zurueck', 'zurueckfuehren', 'zurueckfuehrung', 'zurueckgeben', 'zurueckgefuehrt', 'zurueckgefuehrte', 'zurueckgefuert', 'zurueckgefuhrte', 'zurueckgefuhrtem', 'zurueckgefuhrten', 'zurueckgefuhrter', 'zurueckgefuhrtes', 'zurueckgegeben', 'zurueckgeschaltet', 'zurueckgesetzt', 'zurueckgewinnen', 'zurueckgewonnen', 'zuruecksetzen', 'zurueckziehen', 'zurueckzufuehren', 'zusaetzlich', 'zusaetzliche', 'zusaetzlichen', 'zusammen', 'zusammenfassung', 'zusammenfuehren', 'zusammenfuehrt', 'zusammengefuehrt', 'zusatzbedingung', 'zustaende', 'zustaenden', 'zustaendig', 'zustaendige', 'zustaendigkeit', 'zustand', 'zustanden', 'zustandes', 'zustands', 'zustandsabbildung', 'zustandsaenderung', 'zustandsautomat', 'zustandslos', 'zuvor', 'zuweisung', 'zweck', 'zwecke', 'zweckmaessig', 'zwecks', 'zweig', 'zweige', 'zweite', 'zwischen', 'zwischenergebnis', 'zwischenergebnisse', 'zwischenschritt', 'zwischenwert', 'zwischenwerte', 'zyklen', 'zyklus'])
# Residual additions from the final re-acceptance list and the
# single-word findings closed in this pass.  Every entry is verified absent from
# the public tree by the gate run itself.
LEXICON |= {
    "eines", "kernfehler", "dateiinhalt", "steuersignal", "menuekommandos",
    "chordlinie", "chordlinien", "panelnormalen", "rechten", "verlangt",
    "nachfolgenden", "ueberdauern", "programmlaufs", "startzustand",
    "konstantenausdrucks", "endliche", "endlichen", "endlicher", "endliches",
    "folgenden", "vergiftet", "direkter", "extrahiertes", "schrumfendes",
    "tailwerte", "directedes", "reellen", "punkts", "punktaufnahme",
    "laminarer", "startet", "startalpha", "abgeschlossenen", "abgeschlossener",
    "callbackseitig", "linkesten", "monotoner", "anfachungsgrad", "baut",
    "produktprogramm", "festgelegten", "batchprozess", "inlinewert",
    "inlinewerte", "inlinewerten", "zentralen", "iterationsfolge", "viseiden",
    "gespeicherten", "umlaufene", "winkelsumme", "systemaufbau",
    "fortranablauf", "vorlaufereignisse", "laufs", "promptablauf",
    "defaultvorgabe", "laufenden", "laufende", "durchlaufener",
    "fortlaufende", "schleifendurchlauf", "uberlauf", "stationswerte",
    "stationswert", "schreibweise", "gesplinten", "gesplinte", "separater",
    "kleines", "bekannter", "verschiebe", "existierende", "existierenden",
    "dateiexporte", "promptketten", "dateipolitiken", "dateipolitik",
    "transaktionales", "nuller", "langer", "wachst", "altpfad", "altpfads",
    "diskreter", "diskretem", "iterationszeilen",
    "definiertes", "kanonischem", "bestehenden", "bestehender", "bestehendes",
    "gefundenem", "gefundene", "eingaenge", "eingefuehrt", "wuerdige",
    "ereignisklassen", "befehlsroute", "jobbefehlen", "defaultpolitik",
    "korrelationen", "globaler", "geschriebene", "geschriebenen",
    "erhaltene", "erhaltenen", "shiftet", "standardparameter",
    "abbruchpunkten", "abbruchtext", "abbruchzustand", "abbruchzweig",
    "abfrageknoten", "abfragt", "abgelehnte", "abgeleiteten",
    "abschliessend", "absolutdiff", "absoluten", "absolutmaximum",
    "abstandsquadrat", "absteigend", "absurde", "abweichenden", "addiert",
    "additiven", "additiver", "ahnlichkeitsstation", "aliase", "aliasgleich",
    "allerersten", "allgemeine", "allgemeinen", "amplifikation",
    "amplifikationspfad", "angabe", "angesprochen",
    "endlicher", "persistenten", "geklemmter",
    # final residual pass additions: German fragments found in harness and
    # header comments; each is verified absent from the public tree.
    "sollvektor", "einheitszahlen", "rohdaten", "betriebspunkt",
    "textblock", "flagblock", "vorab", "erzeugtes", "erzeugten",
    "fokussiert", "fokussierter", "fokussiertes", "griffnicht",
    "unqualifiziert", "unqualifiziertes", "verfeinerungsbereiche",
    "nichtstandard", "momentenreferenz", "totenluft", "systemzeiger",
    "stationsanfangszustand", "mappingkontrollwerte", "grenzfallwerte",
    "programmabbruch", "kopplungsroutinen", "stationszustand",
    "symmetrischem", "symmetrischer", "viskoser", "viskoses", "viskos",
    "vorgegebenes", "vorgegebenen", "cachevertrag", "schrumpfendes",
    "tailwerte", "kanonische", "kanonischen", "referenzartefakt",
    "referenzartefakte", "punktaufnahmen", "seitenanzahl",
    "seitendatenanzahl", "zeilenindex", "singularer", "singulaere",
    "ausgabe",
    # gaps proven by the final independent scan: inflected compounds the
    # exact-form lexicon and the suffix list did not cover.
    "ueberschritten", "ueberlaeuft", "verhaltensreferenz", "moderne",
    "analysekonfiguration", "testbasisverzeichnis",
    "cacheverzeichnis", "uebersetzt", "regulaeren", "eigenstaendiges",
    # compound and inflection gaps proven by the final re-acceptance scan;
    # every entry is verified absent from the public tree by the gate run.
    "weicht", "wurzeln", "eigene", "bestand", "pflichtdatei",
    "shellwerkzeug", "setupfehler", "pruefwurzel", "infrastrukturfehler",
    "infrastrukturtests", "quellmanifest", "negativen", "positiven",
    "abschlusslastzeile", "produktprozess", "schutzfall", "strukturellen",
    "projektquellen", "vergleichsgrenze", "exitstatus", "timeoutstatus",
    "targetvertrag", "toolchainvertrag", "ausfuehren",
    # gaps proven by the final re-acceptance pass (R6)
    "provenienz", "absoluter",
}
LEX_HAZARD = {
    "re", "mach", "mrcl", "mses", "ok", "tsvs", "on", "the", "was", "one",
    "has", "will", "child", "case", "location", "weg", "arm", "los", "full",
    "post", "port", "gang", "her", "hin", "haft", "reich", "net", "bet", "gauss",
    "an", "caller", "standard", "am", "hat", "war", "man", "falls",
    "fall", "kind", "null", "alt", "hell",
}

# German-only inflection and word-formation endings.  A token carrying one of
# these endings with a non-trivial stem counts as German material even when the
# exact inflected form is not listed in LEXICON.
SUFFIXES = (
    "ungen", "ung", "heiten", "heit", "keiten", "keit",
    "ieren", "ierte", "ierten", "iert",
    "schaften", "schaft",
    "lichen", "licher", "liches", "liche",
    "ischen", "ischer", "isches", "ische", "isch",
    "haften", "hafter", "haftes", "hafte",
    "losen", "loser", "loses", "lose",
    "vollen", "voller", "volles", "volle",
    "baren", "barer", "bares", "bare",
    "armen", "armer", "armes", "arme",
    "chen", "lein",
)

# English words that share a German-looking ending; they never flag on their own.
MORPH_HAZARD = {
    "lichen", "kitchen", "young", "among", "height", "weight", "freight",
    "request", "coefficient", "sufficient", "insufficient", "unique",
    "antique", "grotesque", "arabesque", "reliable", "unreliable",
    "available", "unavailable", "compatible", "incompatible", "reasonable",
    "unreasonable", "single", "angle", "triangle", "rectangle", "circle",
    "bundle", "handle", "sample", "example", "simple", "close", "closed",
    "closely", "nearly", "early", "yearly", "fully", "fulfil", "fulfill",
    "bare", "barely", "rarely", "square", "armour", "armory", "harmonic", "harmless",
    "warmly", "chosen", "broken", "farmer", "charmer", "closer", "loser",
    "darling", "duckling", "yearling", "handling", "packaging", "folding",
    "building", "children", "wildlife", "mild", "gates", "later", "lateral",
    "water", "matter", "linear", "slice", "sliced", "notice", "notify",
    "service", "surface", "replace", "grace", "space", "place", "castle",
    "declare", "declared", "prepare", "prepared", "compare", "compared",
    "share", "shared", "square", "ensure", "assure", "caveat", "obstacle",
}

PHRASES = [
    (re.compile(r"(?<![A-Za-z])d\.\s?h\.?(?![A-Za-z])"), "German phrase 'd.h.'"),
    (re.compile(r"(?<![A-Za-z])z\.\s?[Bb]\.?(?![A-Za-z])"), "German phrase 'z.B.'"),
    (re.compile(r"(?<![A-Za-z])u\.\s?a\.?(?![A-Za-z])"), "German phrase 'u.a.'"),
    (re.compile(r"(?<![A-Za-z])u\.\s?s\.\s?w\.?(?![A-Za-z])"), "German phrase 'u.s.w.'"),
    (re.compile(r"(?<![A-Za-z])s\.\s?o\.?(?![A-Za-z])"), "German phrase 's.o.'"),
    (re.compile(r"(?<![A-Za-z])w\.\s?m\.\s?w\.?(?![A-Za-z])"), "German phrase 'w.m.w.'"),
]


def norm(token):
    return (token.replace("ä", "ae").replace("ö", "oe").replace("ü", "ue")
            .replace("Ä", "Ae").replace("Ö", "Oe").replace("Ü", "Ue")
            .replace("ß", "ss").lower())


UNIVERSAL_PATTERNS = {".", ".*", ".+", "^.*$", "^.*", ".*$", "^", "$",
                      "(.*)", "(.+)", "(.)", "^.*.*$"}


def validate_allowlist(entries):
    rules = []
    errors = []
    seen = set()
    for e in entries:
        f = e.get("file", "")
        pat = e.get("pattern", "")
        reason = e.get("reason", "")
        if not f or not pat or not reason:
            errors.append("entry missing file/pattern/reason: %r" % (e,))
            continue
        if pat in UNIVERSAL_PATTERNS:
            errors.append("universal pattern not allowed: %s :: %s" % (f, pat))
            continue
        try:
            rx = re.compile(pat)
        except re.error as exc:
            errors.append("pattern does not compile: %s :: %s (%s)" % (f, pat, exc))
            continue
        if rx.search("") is not None:
            errors.append("pattern matches the empty line: %s :: %s" % (f, pat))
            continue
        key = (f, pat)
        if key in seen:
            errors.append("duplicate entry: %s :: %s" % (f, pat))
            continue
        seen.add(key)
        rules.append((f, rx, reason))
    return rules, errors


def is_broad(rx, lines):
    """True if a rule matches more than half of the non-blank lines."""
    nonempty = [ln for ln in lines if ln.strip()]
    if len(nonempty) < 10:
        return False
    hits = sum(1 for ln in nonempty if rx.search(ln))
    return hits > 0.5 * len(nonempty)


# Files that implement the language gates themselves.  Allowlist rules in
# these files match the gate's own lexicon and self-test fixtures; the
# breadth check exempts them, and every such rule carries a reason naming
# the lexicon or self-test purpose.
GATE_OWN_FILES = {"tools/check-public-language.py", "tools/check-public-docs.py"}


def breadth_problems(rules, root):
    problems = []
    cache = {}
    for f, rx, reason in rules:
        # Gate-owned lexicon/self-test files carry rules that match the
        # gate's own detection lists by design; the allowlist reasons for
        # every rule in these files name the lexicon or self-test purpose.
        if f in GATE_OWN_FILES:
            continue
        path = root / f
        if not path.is_file():
            continue
        if f not in cache:
            try:
                cache[f] = path.read_text(encoding="utf-8").splitlines()
            except UnicodeDecodeError:
                cache[f] = []
        if is_broad(rx, cache[f]):
            problems.append("allowlist: overly broad rule %s :: %s (%s)"
                            % (f, rx.pattern, reason))
    return problems


def load_allowlist(root):
    path = root / "tools" / "language-allowlist.json"
    if not path.is_file():
        return []
    entries = json.loads(path.read_text(encoding="utf-8"))
    rules, errors = validate_allowlist(entries)
    for err in errors:
        print("allowlist: %s" % err)
    if errors:
        raise ValueError("language allowlist validation failed")
    return rules


def is_excluded(rel):
    pure = pathlib.PurePosixPath(rel)
    if pure.parts and pure.parts[0] in EXCLUDED_DIRS:
        return True
    for ex in EXCLUDED_PATHS:
        if rel == ex or rel.startswith(ex + "/"):
            return True
    for ex in EXCLUDED_FILES:
        if rel == ex or rel.startswith(ex + "/"):
            return True
    return False


def iter_files(root):
    for d in INCLUDE_DIR_GLOBS:
        base = root / d
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file():
                continue
            rel = path.relative_to(root).as_posix()
            if is_excluded(rel):
                continue
            if path.suffix in SOURCE_SUFFIXES:
                yield rel, path
    harness = root / "tests" / "fortran-reference" / "harnesses"
    if harness.is_dir():
        for path in sorted(harness.rglob("*.f")):
            rel = path.relative_to(root).as_posix()
            if not is_excluded(rel):
                yield rel, path
        for path in sorted(harness.rglob("*.txt")):
            rel = path.relative_to(root).as_posix()
            if "/data/" in rel:
                continue  # consumed input fixtures, category D
            if not is_excluded(rel):
                yield rel, path
    tools = root / "tools"
    if tools.is_dir():
        for path in sorted(tools.iterdir()):
            if path.suffix in (".py", ".sh"):
                rel = path.relative_to(root).as_posix()
                if not is_excluded(rel):
                    yield rel, path
    makefile = root / "Makefile"
    if makefile.is_file():
        yield "Makefile", makefile


def comment_of(rel, line):
    if rel.endswith((".cpp", ".h")):
        i = mask_literals(line).find("//")
        return line[i:] if i >= 0 else ""
    if rel.endswith((".py", ".sh")) or rel == "Makefile":
        i = mask_literals(line).find("#")
        return line[i:] if i >= 0 else ""
    if rel.endswith(".f"):
        if line[:1] in ("C", "c", "*"):
            return line
        i = mask_literals(line).find("!")
        return line[i:] if i >= 0 else ""
    if rel.endswith(".txt"):
        return line
    return ""


EN_SHORT = {
    "an", "am", "at", "as", "or", "if", "of", "to", "by", "up", "us", "we",
    "be", "do", "go", "it", "is", "on", "in", "no", "so", "la", "li", "de",
    "ad", "ae", "ox", "ex", "ha", "he", "hi", "id", "mi", "na", "ne", "ni",
    "nu", "od", "oh", "pa", "pi", "re", "sh", "ta", "ti", "un", "ut", "wo",
    "ye", "ar", "ed", "my",
}


def flag_token(tok):
    """German finding for one comment token, or None."""
    if len(tok) > 1 and tok.isupper():
        return None
    if "-" not in tok and any(c.isupper() for c in tok[1:]):
        return None  # CamelCase identifier, not German prose
    low = norm(tok)
    if low in LEX_HAZARD or low in MORPH_HAZARD:
        return None
    if low in LEXICON:
        return "German word '%s' in comment" % tok
    parts = [p for p in low.split("-") if p]
    if len(parts) > 1:
        for p in parts:
            if p in EN_SHORT or p in LEX_HAZARD or p in MORPH_HAZARD:
                continue
            if p in LEXICON:
                return "German word '%s' in comment" % tok
    for p in parts:
        if len(p) < 5 or p in MORPH_HAZARD or p in LEX_HAZARD:
            continue
        for s in SUFFIXES:
            min_stem = 3 if s.startswith("lich") else 4
            if p.endswith(s) and len(p) - len(s) >= min_stem:
                return "German inflection '%s' in comment" % tok
    return None


def literals_of(rel, line):
    """Contents of string literals on a non-comment line, or ''."""
    out = []
    i = 0
    n = len(line)
    quote = '"' if rel.endswith((".cpp", ".h", ".py")) else "'"
    while i < n:
        c = line[i]
        if c == quote or (rel.endswith(".f") and c == "'"):
            j = i + 1
            while j < n:
                if line[j] == "\\":
                    j += 2
                    continue
                if line[j] == c:
                    break
                j += 1
            out.append(line[i + 1:j])
            i = j + 1
        else:
            i += 1
    text = " ".join(out)
    # printf/fortran edit descriptors are format syntax, not prose
    text = re.sub(r"%[-+ #0]*\d*(?:\.\d+)?[a-zA-Z]", " ", text)
    # URLs, paths and file names are addressing, not prose
    text = re.sub(r"https?://\S+", " ", text)
    text = re.sub(r"(?:[A-Za-z0-9_.-]+/)+[A-Za-z0-9_.-]*", " ", text)
    text = re.sub(r"[A-Za-z0-9_.-]+\.[A-Za-z]{1,4}\b", " ", text)
    # hex digests and digit-bearing tokens are not prose
    text = re.sub(r"[0-9A-Za-z]*\d[0-9A-Za-z]*", " ", text)
    return text


def mask_literals(line):
    """Blank out string/char literal contents; return the masked line."""
    out = list(line)
    i = 0
    n = len(line)
    while i < n:
        c = line[i]
        if c in "\"'":
            j = i + 1
            while j < n:
                if line[j] == "\\":
                    j += 2
                    continue
                if line[j] == c:
                    break
                j += 1
            for k in range(i + 1, min(j, n)):
                out[k] = " "
            i = j + 1
        else:
            i += 1
    return "".join(out)


def comment_regions(rel, text):
    """Map line number -> comment/docstring text for one file.

    Covers C++ line and block comments, Python comments and docstrings,
    shell/Makefile comments and Fortran comment forms.  Literal contents are
    masked before comment delimiters are located.
    """
    regions = {}

    def add(n, chunk):
        if chunk:
            prev = regions.get(n, "")
            regions[n] = (prev + " " + chunk).strip()

    if rel.endswith((".cpp", ".h")):
        inblock = False
        for n, line in enumerate(text.splitlines(), 1):
            masked = mask_literals(line)
            i = 0
            if inblock:
                j = masked.find("*/")
                if j < 0:
                    add(n, line)
                    continue
                add(n, line[:j])
                inblock = False
                i = j + 2
            while True:
                a = masked.find("//", i)
                b = masked.find("/*", i)
                if a < 0 and b < 0:
                    break
                if a >= 0 and (b < 0 or a < b):
                    add(n, line[a + 2:])
                    break
                end = masked.find("*/", b + 2)
                if end < 0:
                    add(n, line[b + 2:])
                    inblock = True
                    break
                add(n, line[b + 2:end])
                i = end + 2
    elif rel.endswith(".py"):
        lines = text.splitlines()
        n = 0
        while n < len(lines):
            line = lines[n]
            i = 0
            while i < len(line):
                c = line[i]
                if c == "#":
                    add(n + 1, line[i + 1:])
                    break
                if c in "\"'" and not line.startswith(c * 3, i):
                    j = i + 1
                    while j < len(line):
                        if line[j] == "\\":
                            j += 2
                            continue
                        if line[j] == c:
                            break
                        j += 1
                    i = j + 1
                    continue
                if c in "\"'" and line.startswith(c * 3, i):
                    q = c * 3
                    docstring = not line[:i].strip()
                    j = line.find(q, i + 3)
                    if j >= 0:
                        if docstring:
                            add(n + 1, line[i + 3:j])
                        i = j + 3
                        continue
                    if docstring:
                        add(n + 1, line[i + 3:])
                    m = n + 1
                    k = -1
                    while m < len(lines):
                        k = lines[m].find(q)
                        if docstring:
                            add(m + 1, lines[m][:k] if k >= 0 else lines[m])
                        if k >= 0:
                            break
                        m += 1
                    if m >= len(lines):
                        break
                    n = m
                    line = lines[m]
                    i = k + 3
                    continue
                i += 1
            n += 1
    elif rel.endswith(".f"):
        for n, line in enumerate(text.splitlines(), 1):
            if line[:1] in ("C", "c", "*"):
                add(n, line[1:])
                continue
            masked = mask_literals(line)
            i = masked.find("!")
            if i >= 0:
                add(n, line[i + 1:])
    elif rel.endswith(".txt"):
        for n, line in enumerate(text.splitlines(), 1):
            add(n, line)
    else:
        for n, line in enumerate(text.splitlines(), 1):
            masked = mask_literals(line)
            i = masked.find("#")
            if i >= 0:
                add(n, line[i + 1:])
    return regions


def check_line(rel, line, comment=None):
    findings = []
    if comment is None:
        comment = comment_of(rel, line)
    if comment:
        if UMMLAUT.search(comment):
            findings.append(("umlaut", "non-ASCII German character in comment"))
        for rx, label in MARKERS:
            if rx.search(comment):
                findings.append(("marker", label))
        if not NEGATIVE_MARKER_CONTEXT.search(comment):
            for rx, label in INTERNAL_NAME_MARKERS + INTERNAL_PATH_MARKERS:
                if rx.search(comment):
                    findings.append(("marker", label))
        for rx, label in COMMENT_MARKERS:
            if rx.search(comment):
                findings.append(("marker", label))
        for rx, label in PHRASES:
            if rx.search(comment):
                findings.append(("german", label))
                break
        for m in TOKEN.finditer(PATH_LIKE.sub(" ", comment)):
            msg = flag_token(m.group(0))
            if msg:
                findings.append(("german", msg))
                break
    else:
        if UMMLAUT.search(line):
            findings.append(("umlaut", "non-ASCII German character outside comment"))
        for rx, label in MARKERS:
            if rx.search(line):
                findings.append(("marker", label))
        if not NEGATIVE_MARKER_CONTEXT.search(line):
            for rx, label in INTERNAL_NAME_MARKERS + INTERNAL_PATH_MARKERS:
                if rx.search(line):
                    findings.append(("marker", label))
        for m in TOKEN.finditer(literals_of(rel, line)):
            msg = flag_token(m.group(0))
            if msg:
                findings.append(("german", msg.replace(" in comment",
                                                      " in string literal")))
                break
    return findings


def check_tree(root):
    allow = load_allowlist(root)
    problems = breadth_problems(allow, root)
    used = set()
    for rel, path in iter_files(root):
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        regions = comment_regions(rel, text)
        for n, line in enumerate(text.split("\n"), 1):
            skip = False
            for idx, (a, rx, _) in enumerate(allow):
                if rel == a and rx.search(line):
                    used.add(idx)
                    skip = True
                    break
            if skip:
                continue
            for kind, msg in check_line(rel, line, regions.get(n, "")):
                problems.append("%s:%d: %s: %s" % (rel, n, kind, msg))
    for idx, (a, rx, reason) in enumerate(allow):
        if idx not in used:
            problems.append("allowlist: unused rule %s :: %s (%s)"
                            % (a, rx.pattern, reason))
    return problems


def self_test():
    ok = True

    def expect(rel, line, should_flag):
        nonlocal ok
        got = bool(check_line(rel, line))
        if got != should_flag:
            ok = False
            print("self-test FAILED: %r -> %r" % (line, got))

    expect("src/x.cpp", "// this comment is fully English", False)
    expect("src/x.cpp", "// a deutsche Description with Word", True)
    expect("src/x.cpp", "// additionally transliteriertes German", True)
    expect("src/x.cpp", "// C11b-P7-P2 phase note", True)
    expect("src/x.cpp", "// OSM-T1 helper", True)
    expect("src/x.cpp", "// SPEC12-0 reference case", False)
    expect("src/x.cpp", "// XICALC12 reference line", False)
    expect("src/x.cpp", "// Mach number Re=1e6 MSES Gauss OK", False)
    expect("src/x.cpp", 'out << "ä";', True)
    expect("tests/fortran-reference/harnesses/h.f", "C Datei: Pfad", True)
    expect("tests/fortran-reference/harnesses/h.f", "C file: path", False)
    # hazard words (German/English homographs) must not flag alone
    expect("src/x.cpp", "// an invalid argument falls back to the prompt", False)
    expect("src/x.cpp", "// the caller must not assume standard defaults", False)
    expect("src/x.cpp", "// I am not war: hat man am Weg?", False)
    # lexicon additions from the final language inventory residual list
    expect("src/x.cpp", "// daher ist die Achse sehr laut", True)
    expect("src/x.cpp", "// seine Bezugsnahme ist integriert", True)
    expect("src/x.cpp", "// Polarakkumulation und Durchreichung", True)
    expect("src/x.cpp", "// Kopfdaten der Dialoge solange erweitert", True)
    expect("src/x.cpp", "// beeinflusst wird stumm gemeldet", True)
    # dotted German phrases survive the token regex
    expect("src/x.cpp", "// value d.h. zero", True)
    expect("src/x.cpp", "// Optionen z.B. OFF", True)
    expect("src/x.cpp", "// list u.a. items", True)
    expect("src/x.cpp", "// the d. head node", False)
    # inflection and composites beyond the exact lexicon forms
    expect("src/x.cpp", "// anpassbaren Randwerten entlang", True)
    expect("src/x.cpp", "// numerische Toleranzen bleiben", True)
    expect("src/x.cpp", "// Panelnormalen-Verteilung", True)
    expect("src/x.cpp", "// the handling is available for close angles", False)
    expect("src/x.cpp", "// declared single sample, kitchen height", False)
    # all-uppercase tokens are identifiers, not German prose
    expect("src/x.cpp", "// DA and LGSAME stay unchanged", False)
    expect("src/x.cpp", "// DesignSessionState.h header note", False)
    expect("src/x.cpp", "// kind of null pivot: fall back", False)
    # CamelCase identifiers are not German prose
    expect("src/x.cpp", "// PolarText semantics for the polar file", False)
    # Residual lexicon additions from the final language inventory
    expect("src/x.cpp", "// eines der Kernfehler im Dateiinhalt", True)
    expect("src/x.cpp", "// endliche Thickness separater", True)
    expect("src/x.cpp", "// Punkts laminarer startet", True)
    expect("src/x.cpp", "// die bestehenden Stationswerte globaler Art", True)
    # German words inside string literals (non-canonical) are findings
    expect("src/x.cpp", 'log("nicht lesbar: Datei offen");', True)
    expect("src/x.cpp", 'log("No airfoil available");', False)
    expect("src/x.cpp", '#include "xfoil/io/PolarText.h"', False)
    # internal phase labels and dead diagnostic paths (comment-only)
    expect("src/x.cpp", "// (P2) phase note", True)
    expect("src/x.cpp", "// (P5 blocker) open", True)
    expect("src/x.cpp", "// P6 viscous harnesses note", True)
    expect("src/x.cpp", "// ST_A1 = P1 + P2", False)
    expect("src/x.cpp", "// see tools/diagnostics/cdcalc-direct/", True)
    expect("src/x.cpp", 'const char* p = "tools/diagnostics";', False)
    # routing/status markers added in the final language pass
    expect("src/x.cpp", "// unrouted handler stays", True)
    expect("src/x.cpp", "// production-routed handler", True)
    expect("src/x.cpp", "// production routed handler", True)
    expect("src/x.cpp", "// unported kernel", True)
    expect("src/x.cpp", "// C11 architecture pass", True)
    expect("src/x.cpp", "// c11a harness reference", False)
    expect("src/x.cpp", "// OSM-E2g residual note", True)
    expect("src/x.cpp", "// not yet production ready", True)
    expect("src/x.cpp", "// still not production routed", True)
    expect("src/x.cpp", "// switch-over point", True)
    expect("src/x.cpp", "// the TOP pending list", True)
    expect("src/x.cpp", "// an OPER block list remains", True)
    expect("src/x.cpp", "// prepared handlers are listed", True)
    expect("src/x.cpp", "// a prepared target segment", False)
    # routing/status markers, case-insensitive coverage
    expect("src/x.cpp", "// Prepared handlers.", True)
    expect("src/x.cpp", "// production routing.", True)
    expect("src/x.cpp", "// No Production Routing here", True)
    expect("src/x.cpp", "// without production routing", True)
    expect("src/x.cpp", "// the production routine stays", False)
    # internal work-package labels and assignment references
    expect("src/x.cpp", "// -G-B2: raw-bit parity", True)
    expect("src/x.cpp", "// G-B1 inviscid PACC proof", True)
    expect("src/x.cpp", "// C11b-G-B3 case", True)
    expect("src/x.cpp", "// precision contract (section 4 of the assignment)", True)
    expect("src/x.cpp", "// Compared values (assignment sections 8/16):", True)
    expect("src/x.cpp", "// see sections 12 to 19", True)
    expect("src/x.cpp", "// Step 0.5 increments the sequence", False)
    expect("src/x.cpp", "// the C04 buffer stays", False)
    # Denglish capitalization and broken hybrids are lexicon findings
    expect("src/x.cpp", "// Einheitszahlen bleiben", True)
    expect("src/x.cpp", "// value not byteidentisch", True)
    # residual tool-diagnostic compounds closed in the final pass
    expect("src/x.cpp", "// value weicht from expectation", True)
    expect("src/x.cpp", "// the value differs from expectation", False)
    expect("src/x.cpp", "// Pflichtdatei missing", True)
    expect("src/x.cpp", "// required file missing", False)
    expect("src/x.cpp", "// Shellwerkzeug not executable", True)
    expect("src/x.cpp", "// shell tool not executable", False)
    expect("src/x.cpp", "// io-Wurzeln und session-Wurzeln", True)
    expect("src/x.cpp", "// I/O roots and session roots", False)
    expect("src/x.cpp", "// Core-Quellmanifest fehlt", True)
    expect("src/x.cpp", "// core source manifest", False)
    expect("src/x.cpp", "// Infrastrukturfehler melden", True)
    expect("src/x.cpp", "// infrastructure error", False)
    expect("src/x.cpp", "// Exitstatus pruefen", True)
    expect("src/x.cpp", "// exit status", False)
    expect("src/x.cpp", "// Timeoutstatus pruefen", True)
    expect("src/x.cpp", "// timeout status", False)
    expect("src/x.cpp", "// public Targetvertrag", True)
    expect("src/x.cpp", "// public target contract", False)
    expect("src/x.cpp", "// Toolchainvertrag beachten", True)
    expect("src/x.cpp", "// toolchain contract", False)
    expect("src/x.cpp", "// eigene Wurzeln und Bestand", True)
    expect("src/x.cpp", "// Abschlusslastzeile im Produktprozess", True)
    expect("src/x.cpp", "// final load line in the product process", False)
    expect("src/x.cpp", "// Vergleichsgrenze und Schutzfall", True)
    expect("src/x.cpp", "// comparison boundary and guard case", False)
    expect("src/x.cpp", "// Projektquellen mit Quellmanifest", True)
    expect("src/x.cpp", "// project sources with source manifest", False)
    expect("src/x.cpp", "// strukturellen negativen positiven", True)
    expect("src/x.cpp", "// Pruefwurzel und Setupfehler", True)
    expect("src/x.cpp", "// check root and setup error", False)
    expect("src/x.cpp", "// Infrastrukturtests und Infrastrukturfehler", True)
    expect("src/x.cpp", 'throw std::invalid_argument("(iblpan first ausfuehren)");', True)
    expect("src/x.cpp", 'throw std::invalid_argument("(run iblpan first)");', False)
    # removed internal dependency scope spellings are no longer
    # part of the gate; current scope spellings stay clean
    expect("Makefile", "\tpython3 tools/check-core-dependencies.py --scope=session", False)
    expect("Makefile", "\tpython3 tools/check-core-dependencies.py --scope=cli", False)
    expect("src/x.cpp", "// session dependency check: OK", False)
    expect("src/x.cpp", "// CLI dependency check: OK", False)
    # internal golden/step labels; protected identifiers stay allowed
    expect("src/x.cpp", "// native batch goldens G1-G4", True)
    expect("src/x.cpp", "// R12 replaced the batch path", True)
    expect("src/x.cpp", "// COMMON CR12 XCMREF default 0.25", False)
    expect("src/x.cpp", "// A1 + A34 are math labels", False)
    # R6 hardening: removed target names, case variants, broken hybrids
    expect("src/x.cpp", "// core-unit target", True)
    expect("src/x.cpp", "// UNROUTED handler", True)
    expect("src/x.cpp", "// taken over shared via the old path", True)
    expect("src/x.cpp", "// LBLINI-State stays", True)
    expect("src/x.cpp", "// Repository-Root path", True)
    expect("src/x.cpp", "// Core-Source entry", True)
    expect("src/x.cpp", "// would be controlled blocked", True)
    expect("src/x.cpp", "// A-R6-1 finding", True)
    expect("src/x.cpp", "// G-R6-5 gap", True)
    expect("src/x.cpp", "// phase R0 note", True)
    expect("src/x.cpp", "// Provenienz of the data", True)
    expect("src/x.cpp", "// absoluter Case path", True)
    # R6 hardening: active internal paths are findings, negative fixtures stay clean
    expect("src/x.cpp", "// see docs/refactoring/OpenSourceMigration/", True)
    expect("src/x.cpp", "// forbidden historical path docs/refactoring/", False)
    expect("src/x.cpp", "// OpenSourceMigration is internal", True)
    expect("src/x.cpp", "// forbidden internal path OpenSourceMigration", False)
    # R6 false-positive controls
    expect("src/x.cpp", "// A1, P1 and P2 are math labels", False)
    expect("src/x.cpp", "// Step 0.5 increments the sequence", False)
    expect("src/x.cpp", "// the repository root and the core source stay", False)
    expect("src/x.cpp", "// the current contour stays", False)
    # residual R7 phrases closed in this pass
    expect("src/x.cpp", "// record length fits not to the layout", True)
    expect("src/x.cpp", 'throw std::invalid_argument("record fits not to N");', True)
    expect("src/x.cpp", "// record length does not match the layout", False)
    expect("tests/fortran-reference/harnesses/h.f", "C ==== INIT #3: erneuter Toggle on false ====", True)
    expect("tests/fortran-reference/harnesses/h.f", "C ==== INIT #3: another toggle to false ====", False)
    # residual R8 prose, canonical-name and activity-verb classes
    expect("src/x.cpp", "// Not by std::numbers::pi replace, because dies the frozen Reference values.", True)
    expect("src/x.cpp", "// Not by std::numbers::pi replace, because this could change the reference values.", False)
    expect("src/x.cpp", "// An empty answer lets the value unchanged.", True)
    expect("src/x.cpp", "// An empty answer leaves the value unchanged.", False)
    expect("tests/fortran-reference/harnesses/h.f", "C     after NSEQEX=4 points holds.", True)
    expect("tests/fortran-reference/harnesses/h.f", "C     Check the points retained after NSEQEX = 4.", False)
    expect("src/x.cpp", "// Xfoil session.h", True)
    expect("src/x.cpp", "// Xfoil session.cpp", True)
    expect("src/x.cpp", "// Session types.h", True)
    expect("src/x.cpp", "// polar tests.cpp", True)
    expect("src/x.cpp", "// include/xfoil/limits.h", True)
    expect("src/x.cpp", "// fortran-src/airfoil.f", True)
    expect("src/x.cpp", "// fortran-src/xgdes.f/xtcam.f", True)
    expect("src/x.cpp", "// XFoilSession.h", False)
    expect("src/x.cpp", "// XFoilSession.cpp", False)
    expect("src/x.cpp", "// SessionTypes.h", False)
    expect("src/x.cpp", "// PolarTests.cpp", False)
    expect("src/x.cpp", "// include/xfoil/Limits.h", False)
    expect("src/x.cpp", "// fortran-src/profil.f", False)
    expect("src/x.cpp", "// fortran-src/xgdes.f and fortran-src/xtcam.f", False)
    # residual R9 prose classes closed in this pass
    expect("src/x.cpp", "// SA/approximately are updated here.", True)
    expect("src/x.cpp", "// SA and CA are updated here.", False)
    expect("src/x.cpp", "// A edit descriptor is emitted.", True)
    expect("src/x.cpp", "// An edit descriptor is emitted.", False)
    expect("src/x.cpp", "// They changes no Fortran source.", True)
    expect("src/x.cpp", "// They do not change any Fortran source.", False)
    expect("src/x.cpp", "// It change no source file.", True)
    expect("src/x.cpp", "// It does not change any source file.", False)
    expect("src/x.cpp", "// The geometry remains unchanged on.", True)
    expect("src/x.cpp", "// The geometry remains unchanged.", False)
    expect("src/x.cpp", "// All Tables values finite.", True)
    expect("src/x.cpp", "// All table values are finite.", False)
    expect("src/x.cpp", "// These values are a on-demand derivation.", True)
    expect("src/x.cpp", "// These values are derived on demand.", False)
    expect("src/x.cpp", "// This is a still never-written output.", True)
    expect("src/x.cpp", "// This output has not yet been written.", False)
    expect("src/x.cpp", "// Then follows mandatorily the EXEC path.", True)
    expect("src/x.cpp", "// The EXEC path must then follow.", False)
    expect("src/x.cpp", "// a run changed not the state a other.", True)
    expect("src/x.cpp", "// One run does not change the state of another.", False)
    expect("tests/fortran-reference/harnesses/h.f", "C Mach calls itself No FCPMIN.", True)
    expect("tests/fortran-reference/harnesses/h.f", "C Mach itself does not call FCPMIN.", False)
    expect("src/x.cpp", "// here a controlled deviation", True)
    expect("src/x.cpp", "// This is a controlled deviation from the reference behavior.", False)
    expect("src/x.cpp", "// there is transition in X1..X2", True)
    expect("src/x.cpp", "// There is a transition at this station.", False)
    # paths, URLs, file names and batch markers in literals are not prose
    expect("src/x.cpp", 'open("/tmp/datei.dat");', False)
    expect("src/x.cpp", 'out << "XFOIL-BATCH 1";', False)
    expect("src/x.cpp", 'msg("batch process not runnable");', False)
    expect("src/x.cpp", 'u = "https://example.org/datei.html";', False)
    expect("src/x.cpp", 'p = "tests/data/core-c11a-state-reference/x.txt";', False)

    def expect_regions(rel, text, flagged_lines):
        nonlocal ok
        findings = {n for n, chunk in comment_regions(rel, text).items()
                    if check_line(rel, chunk, chunk)}
        if findings != set(flagged_lines):
            ok = False
            print("self-test FAILED regions %s: %r != %r"
                  % (rel, sorted(findings), sorted(flagged_lines)))

    expect_regions("src/x.cpp", "/* deutsche Beschreibung */\nint x;  // ok\n", [1])
    expect_regions("src/x.cpp", "/*\n * Kontur bleibt\n */\n", [2])
    expect_regions("src/x.cpp", 'out << "Kante";  // ok\n', [])
    expect_regions("tools/y.py", '"""Zustaende der Sitzung."""\n# Datei pfad\n', [1, 2])
    expect_regions("tools/y.py", 'x = "Kante"  # ok\n', [])
    expect_regions("tools/y.sh", '#! /bin/sh\n# Datei pfad\n', [2])
    expect_regions("Makefile", "CXXFLAGS := -Wall  # keine Warnung\n", [1])
    expect_regions("tests/fortran-reference/harnesses/h.f", "C Datei: Pfad\n      x = 1 ! Wert\n", [1, 2])
    # orphan harness .txt sources are scanned as comment material
    expect_regions("tests/fortran-reference/harnesses/v/viscous_harness.txt", "COMMON-Initialisierung der Werte\n", [1])
    # allowlist behaviour
    allow = [("tools/gen-fortran-cases.sh", re.compile(r'^\s+"[^"]*"$'), "meta")]
    line = '  "Not-default PPAR-Configuration"'
    allowed = any(rx.search(line) for _, rx, _ in allow)
    if not allowed:
        ok = False
        print("self-test FAILED: allowlist pattern did not match")
    # protected category-D reference ids stay allowed through a narrow rule
    ref_id = "G1-" + "G4"
    allow = [("tests/golden/metadata.md", re.compile(ref_id), "reference id")]
    line = "native batch goldens " + ref_id
    if not any(rx.search(line) for _, rx, _ in allow):
        ok = False
        print("self-test FAILED: category-D reference-id rule did not match")
    # allowlist validation
    def expect_errors(entries, count, label):
        nonlocal ok
        _, errs = validate_allowlist(entries)
        if len(errs) != count:
            ok = False
            print("self-test FAILED: %s -> %d error(s): %r" % (label, len(errs), errs))

    good = {"file": "a.cpp", "pattern": "Polar", "reason": "canonical"}
    expect_errors([good], 0, "valid entry")
    expect_errors([good, good], 1, "duplicate entry")
    expect_errors([{"file": "a.cpp", "pattern": ".", "reason": "all"}], 1, "universal dot")
    expect_errors([{"file": "a.cpp", "pattern": ".*", "reason": "all"}], 1, "universal star")
    expect_errors([{"file": "a.cpp", "pattern": "^.*$", "reason": "all"}], 1, "universal anchored")
    expect_errors([{"file": "a.cpp", "pattern": "Polar", "reason": ""}], 1, "empty reason")
    expect_errors([{"file": "", "pattern": "Polar", "reason": "x"}], 1, "empty file")
    expect_errors([{"file": "a.cpp", "pattern": "(", "reason": "x"}], 1, "broken regex")
    expect_errors([{"file": "a.cpp", "pattern": "x*", "reason": "x"}], 1, "empty-matching regex")
    # overly broad allowlist rules are rejected by match ratio
    many = ["line %d of english prose here" % i for i in range(100)]
    for bad in (".*[a-z]+.*", ".+[A-Za-z].+", "^[^$]+$", "[a-z]+"):
        if not is_broad(re.compile(bad), many):
            ok = False
            print("self-test FAILED: broad rule not detected: %s" % bad)
    if is_broad(re.compile("Polar"), ["Polar file"] + many):
        ok = False
        print("self-test FAILED: narrow rule flagged as broad")
    if is_broad(re.compile(r'^\s+"[^"]*"$'),
               ['  "Polar sample text"' if i % 5 == 0 else "  new_case x"
                for i in range(100)]):
        ok = False
        print("self-test FAILED: category-D style rule flagged as broad")
    print("self-test:", "OK" if ok else "FAILED")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=str(ROOT_DEFAULT))
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    root = pathlib.Path(args.root).resolve()
    try:
        problems = check_tree(root)
    except ValueError as exc:
        print(exc)
        return 1
    for p in problems:
        print(p)
    if problems:
        print("language gate: %d violation(s)" % len(problems))
        return 1
    print("language gate: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
