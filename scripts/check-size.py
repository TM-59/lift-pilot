from pathlib import Path

files = sorted(p for p in Path("dist").rglob("*") if p.is_file())
total = sum(p.stat().st_size for p in files)
for p in files:
    print(f"{p}: {p.stat().st_size:,} B")
print(f"Browser distribution: {total:,} B ({len(files)} files)")
if total > 10_000_000:
    raise SystemExit("Browser distribution exceeds 10,000,000 B")
