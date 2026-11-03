"""Check relocatable source references and vendor-library provenance."""
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[1]
errors=[]
for project in (root/'firmware').rglob('*.uvprojx'):
    xml=ET.parse(project)
    for tag in ('FilePath','IncludePath'):
        for item in xml.iter(tag):
            for value in (item.text or '').split(';'):
                if not value: continue
                value=value.replace('\\','/')
                candidate=Path(value)
                if candidate.is_absolute() or ':' in value:
                    errors.append(f'Absolute reference: {project.name}: {value}')
                elif not (project.parent/candidate).exists():
                    # Include entries unused by the original project are listed
                    # separately; remove those stale entries, never copy guesses.
                    errors.append(f'Missing {tag}: {project.name}: {value}')
manifest=json.loads((root/'docs/migration_sha256.json').read_text())
checked=0
for name,digest in manifest.items():
    if '/Libraries/' not in name and '/0_Library/' not in name: continue
    path=root/name
    if hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
        errors.append('Vendor file modified: '+name)
    checked+=1
print(f'Vendor files unchanged: {checked}; errors: {len(errors)}')
for error in errors: print(error)
raise SystemExit(bool(errors))
