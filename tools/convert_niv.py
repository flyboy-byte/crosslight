#!/usr/bin/env python3
"""Convert an NIV Bible source into CrossLight's canonical translation JSON.

CrossLight reads a translation as ONE file at /Bible/NIV/niv.json in the schema
BibleChapterLoader expects:

    {"books":[{"name": "...",
               "chapters":[{"chapter": <int>,
                            "verses":[{"verse": <int>, "text": "..."}]}]}]}

Two hard requirements come straight from the firmware loader
(src/bible/BibleChapterLoader.cpp):
  * chapter and verse numbers MUST be JSON *numbers*, not strings -- the loader
    parses them only in onBuildNumber(); string values are silently dropped,
    giving empty chapters.
  * text is a JSON string; UTF-8 is preserved verbatim.

Supported input (auto-detected):
  * aruljohn/Bible-niv layout: a directory with Books.json + one "<Book>.json"
    per book (chapter/verse as strings). THE recommended source.
  * canonical layout: a single JSON file already shaped like the output (used to
    re-validate / normalise an existing niv.json).

NOT supported: the Sermonator plain-text NIV (rotarydialer/Sermonator). It has no
chapter/verse markers and jams the book name onto the first verse, so verse
boundaries can't be reconstructed reliably. Use the aruljohn JSON.

LICENSING: the NIV is copyrighted (Biblica). This tool converts YOUR OWN copy for
YOUR OWN device. Do not commit or redistribute the output. See PLAN.md
"Planned update: Bible expansion".

Usage:
  tools/convert_niv.py sources/niv/aruljohn -o niv.json
  tools/convert_niv.py existing/niv.json -o niv.json     # revalidate a file
"""

import argparse
import json
import os
import sys

# The 66-book Protestant canon, in order. Used to validate names/order when the
# source doesn't carry its own order list.
CANON = [
    "Genesis", "Exodus", "Leviticus", "Numbers", "Deuteronomy", "Joshua", "Judges", "Ruth",
    "1 Samuel", "2 Samuel", "1 Kings", "2 Kings", "1 Chronicles", "2 Chronicles", "Ezra",
    "Nehemiah", "Esther", "Job", "Psalms", "Proverbs", "Ecclesiastes", "Song of Solomon",
    "Isaiah", "Jeremiah", "Lamentations", "Ezekiel", "Daniel", "Hosea", "Joel", "Amos",
    "Obadiah", "Jonah", "Micah", "Nahum", "Habakkuk", "Zephaniah", "Haggai", "Zechariah",
    "Malachi", "Matthew", "Mark", "Luke", "John", "Acts", "Romans", "1 Corinthians",
    "2 Corinthians", "Galatians", "Ephesians", "Philippians", "Colossians", "1 Thessalonians",
    "2 Thessalonians", "1 Timothy", "2 Timothy", "Titus", "Philemon", "Hebrews", "James",
    "1 Peter", "2 Peter", "1 John", "2 John", "3 John", "Jude", "Revelation",
]


class ConversionError(Exception):
    pass


def to_int(value, what):
    """Coerce a chapter/verse value (often a string in aruljohn) to int."""
    if isinstance(value, bool):  # bool is an int subclass; reject it explicitly
        raise ConversionError(f"{what} is a boolean, expected a number: {value!r}")
    if isinstance(value, int):
        return value
    if isinstance(value, str) and value.strip().isdigit():
        return int(value.strip())
    raise ConversionError(f"{what} is not a whole number: {value!r}")


def load_aruljohn(dir_path):
    """Return [{name, chapters:[{chapter:int, verses:[{verse:int, text}]}]}] from an
    aruljohn/Bible-niv directory."""
    books_json = os.path.join(dir_path, "Books.json")
    if not os.path.exists(books_json):
        raise ConversionError(f"No Books.json in {dir_path} -- not an aruljohn layout")
    order = json.load(open(books_json, encoding="utf-8"))
    out = []
    for name in order:
        path = os.path.join(dir_path, f"{name}.json")
        if not os.path.exists(path):
            raise ConversionError(f"Missing book file: {path}")
        raw = json.load(open(path, encoding="utf-8"))
        src_name = raw.get("book", name)
        chapters = []
        for ch in raw["chapters"]:
            cnum = to_int(ch["chapter"], f"{src_name} chapter")
            verses = [{"verse": to_int(v["verse"], f"{src_name} {cnum}:verse"),
                       "text": v["text"]} for v in ch["verses"]]
            chapters.append({"chapter": cnum, "verses": verses})
        out.append({"name": src_name, "chapters": chapters})
    return out


def load_canonical(file_path):
    """Load an already-canonical single-file translation, coercing numbers to int."""
    data = json.load(open(file_path, encoding="utf-8"))
    books = data["books"] if isinstance(data, dict) else data
    out = []
    for b in books:
        chapters = []
        for ch in b["chapters"]:
            cnum = to_int(ch["chapter"], f"{b['name']} chapter")
            verses = [{"verse": to_int(v["verse"], f"{b['name']} {cnum}:verse"),
                       "text": v["text"]} for v in ch["verses"]]
            chapters.append({"chapter": cnum, "verses": verses})
        out.append({"name": b["name"], "chapters": chapters})
    return out


# Verses the NIV omits from the running text (modern critical text; footnoted, not
# printed). Empty text for these is expected, not a data error.
NIV_OMITTED = {
    ("Matthew", 17, 21), ("Matthew", 18, 11), ("Matthew", 23, 14), ("Mark", 7, 16),
    ("Mark", 9, 44), ("Mark", 9, 46), ("Mark", 11, 26), ("Mark", 15, 28),
    ("Luke", 17, 36), ("Luke", 23, 17), ("John", 5, 4), ("Acts", 8, 37),
    ("Acts", 15, 34), ("Acts", 24, 7), ("Acts", 28, 29), ("Romans", 16, 24),
}


def normalize_names(books):
    """Snap book names to the canonical English spelling when order matches, so NIV
    book names line up with CrossLight's other translations. Returns list of warnings."""
    warnings = []
    if len(books) == 66 and [b["name"].lower() for b in books] == [c.lower() for c in CANON]:
        for i, b in enumerate(books):
            if b["name"] != CANON[i]:
                warnings.append(f"Normalised book name {b['name']!r} -> {CANON[i]!r}")
                b["name"] = CANON[i]
    return warnings


def validate(books, strict):
    """Return (errors, warnings). errors block output unless --force."""
    errors, warnings = [], []

    names = [b["name"] for b in books]
    if len(books) != 66:
        errors.append(f"Expected 66 books, got {len(books)}")
    # Order/name check against the canon (warn, since name spellings vary a little).
    if names != CANON:
        for i, (got, want) in enumerate(zip(names, CANON)):
            if got != want:
                warnings.append(f"Book {i+1} name/order: got {got!r}, canon expects {want!r}")

    for b in books:
        chapters = b["chapters"]
        nums = [c["chapter"] for c in chapters]
        if nums != list(range(1, len(nums) + 1)):
            errors.append(f"{b['name']}: chapters not contiguous 1..{len(nums)}: {nums[:5]}...")
        for c in chapters:
            vnums = [v["verse"] for v in c["verses"]]
            if not vnums:
                errors.append(f"{b['name']} {c['chapter']}: no verses")
                continue
            # Verses should be contiguous from 1. Some translations legitimately
            # omit verses (footnoted), so treat gaps as a warning, not an error.
            expected = list(range(1, len(vnums) + 1))
            if vnums != expected:
                if sorted(set(vnums)) != vnums:
                    errors.append(f"{b['name']} {c['chapter']}: duplicate/unordered verses")
                else:
                    warnings.append(f"{b['name']} {c['chapter']}: verse gaps "
                                    f"(has {len(vnums)}, ends at {vnums[-1]})")
            for v in c["verses"]:
                if not v["text"] or not v["text"].strip():
                    if (b["name"], c["chapter"], v["verse"]) in NIV_OMITTED:
                        warnings.append(f"{b['name']} {c['chapter']}:{v['verse']}: empty "
                                        f"(known NIV omission)")
                    else:
                        errors.append(f"{b['name']} {c['chapter']}:{v['verse']}: unexpected empty text")
    return errors, warnings


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", help="aruljohn directory (with Books.json) OR a canonical .json file")
    ap.add_argument("-o", "--output", default="niv.json", help="output path (default niv.json)")
    ap.add_argument("--force", action="store_true", help="write output even if validation errors exist")
    ap.add_argument("--indent", type=int, default=None,
                    help="pretty-print with this indent (default: compact, smaller on SD)")
    args = ap.parse_args()

    try:
        if os.path.isdir(args.input):
            print(f"Source: aruljohn layout ({args.input})")
            books = load_aruljohn(args.input)
        else:
            print(f"Source: canonical file ({args.input})")
            books = load_canonical(args.input)
    except (ConversionError, KeyError, json.JSONDecodeError) as e:
        sys.exit(f"ERROR reading source: {e}")

    ch = sum(len(b["chapters"]) for b in books)
    vs = sum(len(c["verses"]) for b in books for c in b["chapters"])
    print(f"Parsed {len(books)} books, {ch} chapters, {vs} verses")

    name_warnings = normalize_names(books)
    errors, warnings = validate(books, args.force)
    warnings = name_warnings + warnings
    for w in warnings:
        print(f"  WARN: {w}")
    if warnings:
        print(f"{len(warnings)} warning(s)")
    if errors:
        for e in errors[:40]:
            print(f"  ERROR: {e}")
        if len(errors) > 40:
            print(f"  ... and {len(errors) - 40} more")
        if not args.force:
            sys.exit(f"{len(errors)} error(s) -- not writing output (use --force to override)")
        print(f"{len(errors)} error(s) -- writing anyway (--force)")

    with open(args.output, "w", encoding="utf-8") as f:
        json.dump({"books": books}, f, ensure_ascii=False, indent=args.indent,
                  separators=None if args.indent else (",", ":"))
    size = os.path.getsize(args.output)
    print(f"Wrote {args.output}  ({size/1024/1024:.2f} MB)")
    print("Copy it to your SD card at /Bible/NIV/niv.json")


if __name__ == "__main__":
    main()
