# XFOIL 6.996 — U0 inventory report

This report records the implementation and independent acceptance of the
U0 inventory, source freeze, and version-aware Fortran provenance gate.
Detailed machine-generated logs remain in the external audit area.

## X6996-U0-G1-R1 — Abschlussbericht (read-only)

**Entscheidung: `X6996-U0-G1-R1: PASS`**

Alle Prüfungen sind erfüllt. Es gab **keine Repositoryänderung, kein Staging, kein Commit, kein Push, kein Tag, kein Release, keinen Netzwerkzugriff**.

### Read-only-Nachweis
- Vollständiger Fingerabdruck des öffentlichen Baums (relative Pfade, Typ, Modus, Größe, SHA-256, Symlinkziel) vor/nach: **2866 Einträge, byteidentisch** (`diff` leer).
- Git-Snapshot vor/nach identisch. Ausgeschlossen: `.git_custom/`, `build/`, `docs/Logo/`.
- Auditdaten ausschließlich extern unter `…/u0-g1-r1/`; keine Auditdatei im öffentlichen Baum.

### Gitstatus und Fremdänderungen
- HEAD `58bca7d17f847e1aef6b1c8d3d4e167f40f69945`; `origin/main` gleich; Tag `v1.0.0` (annotiert, `dc7305b0…` → HEAD).
- Arbeitsbaum:
```text
 M README.md
 M XFOIL.next.png
 M tools/check-license-headers.py
?? docs/xfoil-6.996/
?? fortran-bin/xfoil-6.996
?? fortran-src/xfoil-6.996/
?? tools/fortran-provenance.json
```
- Staged: **0**. `git diff --check` grün.
- `README.md` (`a44f6945…`) und `XFOIL.next.png` (`4a89a54f…`): vorbestehende U0-fremde Änderungen, seit dem dokumentierten U0-Ausgangszustand unverändert, nicht Teil von U0/G1. Keine weitere unerwartete Datei.

### Manifeststruktur
`tools/fortran-provenance.json`, vollständig geparst und unabhängig validiert:
- `schema_version: 1`; IDs `xfoil-6.99`, `xfoil-6.996` eindeutig.
- Roots: `fortran-src` bzw. `fortran-src/xfoil-6.996`; beide relativ, ohne `..`, vorhanden, keine Symlinks.
- Alle Dateipfade relativ, keine Traversal-/Absolutpfade, keine Duplikate, alle SHA-256 formal gültig, `notice_expected` für jede Datei explizit, Metadaten explizit, keine Mehrfachzuordnung.

| Version | Root | Dateien | Notice | ohne Notice |
|---|---|---:|---:|---:|
| XFOIL 6.99 | `fortran-src` | 48 | 23 | 25 |
| XFOIL 6.996 | `fortran-src/xfoil-6.996` | 49 | 23 | 26 |

Die 46 ist nur Berichtswert, keine Entscheidungsgrundlage.

### Hash- und Noticeprüfung
- 6.99: 48/48 vorhanden, 48/48 Hash korrekt, 0 zusätzliche, 0 fehlende; `sha256sum --check fortran-src/SHA256SUMS` Exit 0.
- 6.996: 49/49 vorhanden, 49/49 Hash korrekt, 0 zusätzliche, 0 fehlende.
- `Makefile_gfortran`: `ece560720547fd0ce718fa33b6bacab96b43a0fa8ee80fe21f13344cfadbfc92`.
- Notices unabhängig: 6.99 23/23, 6.996 23/23; kein Ausgleich zwischen Versionen oder zwischen zwei Dateien derselben Version wird akzeptiert (Self-Test + Realbaumtest).

### Rootzuordnung
- `fortran-src/xfoil-6.996` gehört dem spezifischeren Root; der 6.99-Scan überspringt den gesamten 6.996-Unterbaum.
- Rootreihenfolge ist ergebnisunabhängig (extern mit umgekehrter Manifestreihenfolge geprüft: Exit 0).
- Überlappende/unklare Roots werden abgelehnt.
- Extern simuliert: aktuelles verschachteltes Layout **und** späteres Geschwisterlayout (`fortran-src/xfoil-6.99/` + `fortran-src/xfoil-6.996/`) → beide Exit 0. Beim U1-Layout ändert sich nur das 6.99-`root`-Feld; die relativen 6.99-Dateieinträge bleiben identisch.

### Gateimplementierung
- Bestehende Header-Verträge erhalten: SPDX-/Lizenzheader, Herkunftssätze, Shebangregeln, Kategorien A/B, Ausschlüsse, Fortran-Injektionsverbot; keine Abschwächung.
- Upstream-Prüfung manifestbasiert: Manifest/Schema, Root, Datei regulär, kein Symlink, Pfad im Root, SHA-256, Notice, Zusatzdateien, erlaubte Metadaten, Mehrfachzuordnung; Diagnosen enthalten Version und Pfad.
- Keine feste globale 23/46-Entscheidung mehr im Code (`grep` ohne Treffer).

### Self-Tests
`--self-test`: **Exit 0**, 38 OK-Zeilen. Alle geforderten Fälle werden tatsächlich ausgeführt (Quelltextprüfung), inkl. nested/sibling Roots, fehlende/zusätzliche Dateien beider Versionen, Hash, Notice fehlt/unerwartet, gleiche Gesamtsumme bei falscher Zuordnung, Duplikate, absolute Pfade, Traversal, Symlinks, fehlendes/ungültiges Manifest, unbekannte Schema-Version, Metadaten erlaubt/abgelehnt. Ausschließlich temporäre Verzeichnisse.

### Negative Realbaumtests (externe Kopien)
| Fall | Exit | Diagnose |
|---|---:|---|
| `xoper.f` entfernt | 1 | `xfoil-6.996: missing upstream file: xoper.f` |
| `extra.f` hinzugefügt | 1 | `xfoil-6.996: unexpected upstream file: extra.f` |
| `xutils.f` geändert | 1 | `xfoil-6.996: SHA-256 mismatch: xutils.f` |
| Notice verschoben, Manifesthashes angepasst, Summe 46 | 1 | `unexpected upstream notice: aread.f` + `expected upstream notice missing: xbl.f` |

Der echte Arbeitsbaum blieb unverändert.

### U0-Integrität
- 6.996: **49 Dateien**, **1.154.891 Bytes**, **49× 0644**, 0 Symlinks, 0 leer, **49/49 byteidentisch** zum lokalen Archiv (`0feb71b5…3dff3f23`).
- Nicht vorhanden: `dplot1.f`, `xqdes1.f`, `x.log`, `osmap.dat`, `osmapDP.dat`.
- Binary: `000b0468d9a7f4fce42d6fac24bd14147d7d326c8a9035909c8e9f98d180f50e`, 860.704 Bytes, Modus **0755**, Build-ID `1f07dff6…`, GCC 13.3.0; Status diagnostisch.

### `00-inventory.md`
`X6996-U0: PASS`, 49 Dateien / 1.154.891 Bytes, Gate als grün dokumentiert, versionsbewusste Manifestprüfung erklärt, 6.99 und 6.996 getrennt mit je 23 Notices, keine veraltete Rot-Aussage, keine lokalen absoluten Pfade, keine falsche Oracle-/Reproduzierbarkeitsbehauptung.

### Gates und Tests
| Prüfung | Ergebnis |
|---|---|
| `python3 tools/check-license-headers.py` | Exit 0 (`48/23`, `49/23`) |
| `make check-license-headers` | Exit 0 |
| `make check-fortran-sha` | Exit 0 |
| `make check-release-inventory` | Exit 0 |
| `make check-public-docs` | Exit 0 |
| `make check-dependencies` | Exit 0 |
| `make core-link-check` | Exit 0 |
| `make session-link-check` | Exit 0 |
| `make test-core` | **414/414** |
| `make test-unit` | **1668/1668** |

### Geschützte Verträge
- 6.99-Quellen und `fortran-src/SHA256SUMS` (`38c6503d…`) unverändert; `sha256sum --check` grün.
- 6.996-Quellen unverändert gegenüber der abgenommenen U0-f1-Liste.
- `fortran-bin/xfoil` (`f5779d69…`) und `fortran-bin/xfoil-6.996` (`000b0468…`) unverändert.
- Keine Golden-/Referenz-/Toleranz-/Windows-Golden-Änderung; Produktversion `1.0.0`.
- `README.md`/`XFOIL.next.png` durch G1 unverändert.

### Vorgesehene U0-Commitmenge
Pfadbegrenzt, **kein `git add -A`**:
```text
docs/xfoil-6.996/README.md
docs/xfoil-6.996/00-inventory.md
fortran-src/xfoil-6.996/**
fortran-bin/xfoil-6.996
tools/check-license-headers.py
tools/fortran-provenance.json
```
Nicht enthalten: `README.md`, `XFOIL.next.png`.

### Behandlung von `README.md` und `XFOIL.next.png`
Sie bleiben vor dem Commit bewusst **uncommittet** und werden in einem separaten Pass behandelt. Sie dürfen nicht in den U0-Commit gelangen.

### Nächster zulässiger Schritt
Ein normaler, **pfadbegrenzter** Commit der oben genannten vollständigen U0-Grundlage. Vorher ist die separate Behandlung von `README.md`/`XFOIL.next.png` festzulegen; beide bleiben in diesem Audit unangetastet.

**Ausdrücklich: keine Änderung, kein Staging, kein Commit, kein Push, kein Tag, kein Release, kein Netzwerkzugriff.**

## Report persistence closure — X6996-U0-D1

Status: PASS

- Created `docs/xfoil-6.996/00-inventory_report.md` and recorded the supplied
  X6996-U0-G1-R1 report without changing its meaning or measurements.
- Added the binding `## Step report convention` section to
  `docs/xfoil-6.996/README.md`.
- Formal checks on both documents: UTF-8 without BOM, LF line endings, final
  newline, no NUL characters, no trailing whitespace, mode `0644`, no absolute
  local paths, no credentials.
- Gates: `git diff --check`, `make check-public-docs`,
  `make check-release-inventory` and `make check-license-headers` all exit 0.
- No other files changed. `fortran-src/xfoil-6.996/**`,
  `fortran-bin/xfoil-6.996`, `tools/check-license-headers.py`,
  `tools/fortran-provenance.json` and `docs/xfoil-6.996/00-inventory.md` are
  unchanged.
- `README.md` and `XFOIL.next.png` are unchanged (pre-existing U0-foreign
  changes).
- Staged paths: 0. No commit, no push, no tag, no release.
- Decision: `X6996-U0-D1: PASS`.

Next allowed step:
`X6996-U0-D1-R1-Independent-Report-Persistence-Acceptance`.

## Owner README/logo review — X6996-U0-D2

Decision: FAIL

- The DOI line was already identical between HEAD and the worktree; the DOI
  update had been committed before this work.
- The actual README worktree diff consisted of nine removals of the explicit
  `6.99` qualifier in general product-description passages, changing
  `XFOIL 6.99` to `XFOIL`.
- That exceeded the then-authorized D2 scope, which was limited to a DOI
  reference adjustment, so D2 was recorded as FAIL.
- The logo file was a valid PNG with a measured graphical change, but no
  correction was made.
- No repository file was changed in D2, and staged paths remained 0.

## Owner scope closure — X6996-U0-D2-F1

Decision: PASS

The repository owner explicitly resolved the D2 scope finding:

> The DOI update was already committed before this work.
>
> The current README worktree diff intentionally removes the explicit
> “6.99” qualifier from nine general product-description passages,
> changing “XFOIL 6.99” to “XFOIL”.
>
> The repository owner explicitly approves these nine changes. The generic
> wording is intended to remain valid while XFOIL 6.996 changes are
> integrated incrementally.
>
> The exact upstream baselines and provenance remain documented in NOTICE,
> VALIDATION.md, the Fortran source inventories and docs/xfoil-6.996/.
>
> The existing XFOIL.next.png worktree change is an explicitly approved
> minor graphical correction.

- Nine README replacements verified: all are `XFOIL 6.99` → `XFOIL` in
  general product description, motivation, feature scope,
  port/implementation and licence/heritage passages.
- The DOI link and badge are unchanged relative to HEAD; product version
  remains `1.0.0`; project URL and licence are unchanged; Windows, compiler
  and CI statements are untouched.
- Exact version-specific provenance remains in `NOTICE` and
  `docs/xfoil-6.996/`; `VALIDATION.md` is unchanged. The 6.996 source
  baseline remains diagnostic/in preparation; the observed upstream banner
  `XFOIL Version 7.00` remains an upstream observation; XFOIL.next is not
  renamed to version 7.
- No completed 6.996 integration is claimed.
- Logo verified again: valid PNG, 4233×4841, 8-bit RGBA, non-interlaced; full
  decode and all chunk CRCs OK; not executable; no local paths or sensitive
  metadata; README still references `XFOIL.next.png`.
- Measured logo pixel change, owner-approved: 727,084 pixels (~3.55 %),
  bounding box x=331–3911, y=1965–3206, maximum channel difference 255. No
  aesthetic assessment is made.
- `README.md` and `XFOIL.next.png` were not modified further in this pass.
- Gates: `git diff --check`, `make check-public-docs`,
  `make check-release-inventory`, `make check-license-headers` all exit 0.
- U0 base unchanged: `docs/xfoil-6.996/README.md`,
  `docs/xfoil-6.996/00-inventory.md`, `fortran-src/xfoil-6.996/**` (49/49),
  `fortran-bin/xfoil-6.996`, `tools/check-license-headers.py`,
  `tools/fortran-provenance.json`.
- Staged paths: 0. No commit, no push, no tag, no release, no network access.
- Decision: `X6996-U0-D2-F1: PASS`.

Next allowed step:
`X6996-U0-D2-R1-Independent-Owner-Branding-and-Logo-Acceptance`.

## Independent owner branding and logo acceptance — X6996-U0-D2-R1

Decision: PASS

- Nine README replacements independently verified: exactly `XFOIL 6.99` →
  `XFOIL`; no other text change; the DOI link and badge are unchanged
  relative to HEAD; product version remains `1.0.0`; project URL and licence
  are unchanged; no claim is made that the 6.996 integration is complete;
  `Planned Work` still lists 6.996 as a future integration.
- Precise provenance preserved: `NOTICE` documents XFOIL 6.99, Mark Drela
  and Harold Youngren; `docs/xfoil-6.996/` documents XFOIL 6.996 as a second
  upstream baseline in preparation; `fortran-bin/xfoil-6.996` remains
  diagnostic; `XFOIL Version 7.00` remains an upstream observation only;
  the product name remains XFOIL.next and the product version `1.0.0`.
- Logo independently verified: HEAD `2b85be92…`, worktree `4a89a54f…`; valid
  PNG, 4233×4841, 8-bit RGBA, non-interlaced; all chunk CRCs correct; full
  decode; not executable; no sensitive metadata; README image reference
  valid. The measured pixel change (727,084 pixels, ~3.55 %, bounding box
  x=331–3911 / y=1965–3206, maximum channel difference 255) is accepted as
  the explicitly approved graphical correction, without aesthetic
  assessment.
- U0 foundation unchanged: 49 files, 1,154,891 bytes, 49× `0644`, 49/49
  byte-identical to the local archive; `fortran-bin/xfoil-6.996`
  `000b0468…` (diagnostic, `0755`); `fortran-bin/xfoil` `f5779d69…`;
  `fortran-src/SHA256SUMS` `38c6503d…`; the provenance gate reports
  `xfoil-6.99: 48 files, 23 notice-bearing files` and
  `xfoil-6.996: 49 files, 23 notice-bearing files`.
- Gates: `git diff --check`, `make check-public-docs`,
  `make check-release-inventory`, `make check-license-headers` and
  `make check-fortran-sha` all exit 0.
- Git status: staged 0; no commit and no push.
- Publication policy until 1.1.0: see the section below.

## Publication policy update

Normal local commits are allowed.
No push takes place before final acceptance of the complete 1.1.0 integration.
The `v1.0.0` tag remains unchanged.

## Local commit closure — X6996-U0-C1

Decision: PASS

Following independent acceptance of U0, the accepted worktree is recorded in
two separate normal local commits:

1. generic XFOIL branding and the approved logo correction;
2. the XFOIL 6.996 inventory, source freeze and version-aware provenance gate.

The commits remain local under the XFOIL.next 1.1.0 publication hold. No push,
tag or release is performed in this step.
