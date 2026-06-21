#!/usr/bin/env python3
# check-keycode-digit-patterns.py — Phase 6F static guard against unsafe KeyCode
# digit handling.
#
# The KeyCode enum (src/input/KeyCodes.h) declares NUM_0..NUM_9 in physical
# keypad order, NOT numeric order, so NUM_0 > NUM_9 numerically. That makes two
# idioms silently WRONG and they must never come back:
#
#     code >= KeyCode::NUM_0 && code <= KeyCode::NUM_9   // always false
#     code - KeyCode::NUM_0                              // meaningless offset
#
# The correct, order-independent idiom is the helper keyCodeDigitValue(code)
# (returns 0-9, or -1). This script scans src/ and fails (exit 1) if any unsafe
# range comparison or enum-offset arithmetic over the digit codes reappears.
#
# Stdlib only, cross-platform. Comments (// and /* */) are stripped before
# matching so the explanatory notes in KeyCodes.h / ProbabilityApp.cpp do not
# trip the guard. The patterns are anchored on comparison/subtraction operators,
# so legitimate `case KeyCode::NUM_0:` switch labels are never flagged.
#
# Usage:
#   python scripts/check-keycode-digit-patterns.py            # scan src/
#   python scripts/check-keycode-digit-patterns.py --root src # scan a subtree
#   python scripts/check-keycode-digit-patterns.py --selftest # prove the matcher
#
# Exit codes:
#   0  clean (no unsafe pattern), or --selftest passed
#   1  at least one unsafe pattern found, or --selftest failed
#   2  bad invocation / unreadable root

import argparse
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SCAN_EXTENSIONS = (".cpp", ".h", ".hpp", ".cc", ".cxx", ".ino")

# Each rule: (compiled regex, human message). Matching is done on a
# comment-stripped copy of each line, so `//`/`/* */` notes never match.
RULES = [
    (re.compile(r">=\s*(?:KeyCode::)?NUM_0\b"),
     "range check `>= NUM_0` over non-contiguous digit codes"),
    (re.compile(r"<=\s*(?:KeyCode::)?NUM_9\b"),
     "range check `<= NUM_9` over non-contiguous digit codes"),
    (re.compile(r"NUM_9\s*(?:&&|>=|<=|>|<)"),
     "comparison against NUM_9 over non-contiguous digit codes"),
    (re.compile(r"-\s*(?:static_cast\s*<\s*int\s*>\s*\(\s*)?(?:KeyCode::)?NUM_0\b"),
     "enum-offset arithmetic `- NUM_0` over non-contiguous digit codes"),
]

REMEDIATION = "use keyCodeDigitValue(code) from src/input/KeyCodes.h (0-9, or -1)"


def strip_comments(text):
    """Return a list of lines with C/C++ comments blanked out, preserving line
    count and column-ish structure so reported line numbers stay accurate.

    Conservative: does not track string/char literals (the digit patterns we
    search never legitimately occur inside a string), so it can only ever cause
    a false NEGATIVE on contrived `"// ..."`-style code, never a false positive
    on a real comment. That trade-off is intentional and safe here."""
    out = []
    in_block = False
    for line in text.split("\n"):
        res = []
        i, n = 0, len(line)
        while i < n:
            if in_block:
                end = line.find("*/", i)
                if end == -1:
                    i = n
                else:
                    in_block = False
                    i = end + 2
            elif line[i] == "/" and i + 1 < n and line[i + 1] == "/":
                break  # line comment: drop the rest
            elif line[i] == "/" and i + 1 < n and line[i + 1] == "*":
                in_block = True
                i += 2
            else:
                res.append(line[i])
                i += 1
        out.append("".join(res))
    return out


def scan_text(text):
    """Yield (line_no, rule_message, raw_line) for each unsafe match in text."""
    raw_lines = text.split("\n")
    stripped = strip_comments(text)
    for idx, code in enumerate(stripped):
        for regex, message in RULES:
            if regex.search(code):
                yield (idx + 1, message, raw_lines[idx].rstrip())
                break  # one finding per line is enough


def iter_source_files(root):
    for dirpath, _dirs, files in os.walk(root):
        for name in files:
            if name.endswith(SCAN_EXTENSIONS):
                yield os.path.join(dirpath, name)


def run_scan(root):
    if not os.path.isdir(root):
        print("ERROR: not a directory: %s" % root, file=sys.stderr)
        return 2
    findings = []
    scanned = 0
    for path in iter_source_files(root):
        scanned += 1
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                text = f.read()
        except OSError as exc:
            print("WARN: could not read %s: %s" % (path, exc), file=sys.stderr)
            continue
        for line_no, message, raw in scan_text(text):
            rel = os.path.relpath(path, REPO_ROOT).replace(os.sep, "/")
            findings.append((rel, line_no, message, raw))

    if findings:
        print("UNSAFE KeyCode digit pattern(s) found — %s" % REMEDIATION)
        for rel, line_no, message, raw in findings:
            print("  %s:%d: %s" % (rel, line_no, message))
            print("      | %s" % raw.strip())
        print("FAIL: %d unsafe pattern(s) in %d scanned file(s)." % (len(findings), scanned))
        return 1

    print("OK: no unsafe KeyCode digit patterns in %d scanned file(s) under %s"
          % (scanned, os.path.relpath(root, REPO_ROOT).replace(os.sep, "/")))
    return 0


def run_selftest():
    """Prove the matcher catches the bad idioms and clears the good ones,
    including the comment + switch-label cases — no committed fixture needed."""
    bad = [
        "if (ev.code >= KeyCode::NUM_0 && ev.code <= KeyCode::NUM_9) {",
        "bool isDigit = (ev.code >= KeyCode::NUM_0 && ev.code <= KeyCode::NUM_9);",
        "_buf[n++] = '0' + (static_cast<int>(ev.code) - static_cast<int>(KeyCode::NUM_0));",
        "char c = '0' + (code - KeyCode::NUM_0);",
    ]
    good = [
        "int digit = keyCodeDigitValue(ev.code);",
        "if (keyCodeDigitValue(ev.code) >= 0) { startEdit(); }",
        "case KeyCode::NUM_0: insertDigit('0'); break;",
        "case KeyCode::NUM_9: insertDigit('9'); break;",
        "// code >= NUM_0 && code <= NUM_9 is always false (KeyCodes.h)",
        "/* historical bug: code - KeyCode::NUM_0 was wrong */",
    ]
    ok = True
    for line in bad:
        hits = list(scan_text(line))
        if not hits:
            print("SELFTEST FAIL: expected a hit but got none:\n    %s" % line)
            ok = False
    for line in good:
        hits = list(scan_text(line))
        if hits:
            print("SELFTEST FAIL: expected NO hit but matched %r:\n    %s"
                  % (hits[0][1], line))
            ok = False
    # Multi-line block comment spanning the bad idiom must also be ignored.
    block = "/*\n code >= KeyCode::NUM_0 && code <= KeyCode::NUM_9\n*/\nint ok = 0;"
    if list(scan_text(block)):
        print("SELFTEST FAIL: multi-line block comment was not ignored")
        ok = False
    if ok:
        print("SELFTEST OK: matcher flags %d bad idioms and clears %d good lines."
              % (len(bad), len(good)))
        return 0
    return 1


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Fail if unsafe KeyCode digit range/arithmetic patterns exist.")
    parser.add_argument("--root", default=os.path.join(REPO_ROOT, "src"),
                        help="directory to scan (default: src/)")
    parser.add_argument("--selftest", action="store_true",
                        help="verify the matcher itself, then exit")
    args = parser.parse_args(argv)

    if args.selftest:
        return run_selftest()
    return run_scan(args.root)


if __name__ == "__main__":
    sys.exit(main())
