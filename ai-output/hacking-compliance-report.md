# HACKING Compliance Report

Plugins assessed: `geanycli`, `geanyagent`, `geanyservers`, `locate`
Date: 2026-05-26

---

## Summary

All four plugins are partially integrated into the build system but are missing
several items required by HACKING.  The gaps are identical across all four
plugins, suggesting they were bootstrapped together and the checklist was not
completed.

---

## Build System Integration

### configure.ac — PASS

All four plugins have a `GP_CHECK_*` macro call:

| Plugin        | Line |
|---------------|------|
| geanyagent    | 57   |
| geanycli      | 58   |
| geanyservers  | 59   |
| locate        | 64   |

### Root Makefile.am — PASS

All four have correctly guarded `SUBDIRS +=` entries.

### build/*.m4 — PASS

All four have M4 scripts in `build/`:
- `build/geanyagent.m4` — includes VTE dependency check
- `build/geanycli.m4` — includes VTE dependency check
- `build/geanyservers.m4` — no extra deps (correct)
- `build/locate.m4` — no extra deps (correct)

### Plugin root Makefile.am — PASS

All four follow the recommended pattern with
`include $(top_srcdir)/build/vars.auxfiles.mk`, `SUBDIRS = src`, `plugin = <name>`.

### src/Makefile.am — PASS

All four follow the pattern with `vars.build.mk`, `LTLIBRARIES`, `SOURCES`,
`CPPFLAGS`, `CFLAGS`, `LIBADD`, and `cppcheck.mk`.  `geanycli` correctly
includes a header file (`geanycli.h`) in `SOURCES`.

---

## MAINTAINERS — FAIL (all four plugins)

None of the four plugins appear in the `MAINTAINERS` file.

**Action required:** Add an entry for each plugin.  Minimum required fields are
`P:` (Person) and `S:` (Status).  Example:

```
geanycli
P: teknopaul <teknopaul@fastmail.es>
M: teknopaul <teknopaul@fastmail.es>
S: Maintained

geanyagent
P: teknopaul <teknopaul@fastmail.es>
M: teknopaul <teknopaul@fastmail.es>
S: Maintained

geanyservers
P: teknopaul <teknopaul@fastmail.es>
M: teknopaul <teknopaul@fastmail.es>
S: Maintained

locate
P: teknopaul <teknopaul@fastmail.es>
M: teknopaul <teknopaul@fastmail.es>
S: Maintained
```

---

## po/POTFILES.in — FAIL (all four plugins)

None of the four plugins have any entries in `po/POTFILES.in`, and none of the
source files use the `_()` or `N_()` gettext macros.

**There are two valid paths here:**

1. **Add i18n support** — wrap user-visible strings in `_()`, then add source
   files to `POTFILES.in`.  This is the correct approach for any string a
   translator should handle (button labels, menu items, error messages).

2. **Explicitly skip i18n** — if the plugins are intentionally not translated
   (e.g. purely technical output), document that decision in the README and
   leave `POTFILES.in` alone.  Most established plugins do translate at least
   menu items and preference labels.

Given that all four plugins expose UI elements (tabs, menu items, preference
dialogs), option 1 is strongly recommended.

---

## README Documentation — PARTIAL FAIL (all four plugins)

HACKING requires README files to contain:
- author(s) and mail addresses
- external web site (if any)
- known issues
- bug tracker
- dependencies

### geanyagent/README

- Dependencies: PASS (Requirements section present)
- Author/mail: MISSING
- Web site: MISSING
- Known issues: MISSING
- Bug tracker: MISSING

### geanycli/README

- Dependencies: PASS (Requirements section present)
- License: PASS
- Author/mail: MISSING
- Web site: MISSING
- Known issues: MISSING
- Bug tracker: MISSING

### geanyservers/README

- Dependencies: PASS (Requirements section present)
- License: PASS
- Author/mail: MISSING
- Web site: MISSING
- Known issues: MISSING
- Bug tracker: MISSING

### locate/README

- Dependencies: PASS (implicit — mlocate/plocate mentioned)
- Author/mail: MISSING
- Web site: MISSING
- Known issues: MISSING
- Bug tracker: MISSING

**Action required for all four:** Add at minimum an Author section and a
bug-tracker URL.  The GitHub issues page for geany-plugins is the natural
tracker.  Example addition:

```
Author
------
teknopaul <teknopaul@fastmail.es>

Bugs
----
https://github.com/geany/geany-plugins/issues
```

---

## README Format — PASS

All four README files use reStructuredText heading underlines compatible with
`rst2html`.  The locate README uses a Unicode em-dash in the title which is
valid RST.

---

## Directory Structure — PARTIAL

HACKING recommends `src/`, `data/`, `doc/` subdirectories.  None of the four
plugins have a `data/` or `doc/` directory.

- `data/` — only needed if the plugin ships data files; none of these do.
  **Not a blocker.**
- `doc/` — only needed for additional documentation beyond README.  None have
  extra docs.  **Not a blocker.**

Two non-standard files were found inside `src/` directories:

| File | Issue |
|------|-------|
| `geanyagent/src/skill-md.template` | Non-source file inside `src/`; should move to `data/` or project root |
| `locate/src/README.md ` | Documentation inside `src/`; should be at plugin root or in `doc/` |

---

## Stray Files in Plugin Roots — NOTE

`geanycli/` contains test files in the plugin root that are not referenced by
any Makefile target: `test.js`, `test.json`, `test.sh`.  These are not
prohibited by HACKING but could be moved to a `test/` subdirectory for clarity.

---

## Action Checklist

| Task | Affects |
|------|---------|
| Add entries to `MAINTAINERS` | all four |
| Add `_()` to user-visible strings, add files to `po/POTFILES.in` | all four |
|  Add Author + Bug tracker sections to each README | all four |
| Move `locate/src/README.md` to plugin root or `doc/` | locate |
| Optionally organise `test.*` files in `geanycli/` | geanycli |
