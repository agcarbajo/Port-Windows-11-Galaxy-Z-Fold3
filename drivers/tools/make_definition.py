#!/usr/bin/env python3
"""Generate the DriverUpdater definition for the Galaxy Z Fold3 (q2q).

Lists every INF of the q2q BSP (drivers/bsp) followed by the q2q-specific components
(drivers/components). Components go last on purpose: the PEP component replaces the
`qcpep` service binary installed by the BSP.

Usage: python make_definition.py [--bsp PATH] [--check]
"""
import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

DRIVERS = Path(__file__).resolve().parents[1]
OUTPUT = DRIVERS / 'definitions' / 'q2q.xml'
NAMESPACE = 'http://schemas.microsoft.com/embedded/2004/10/ImageUpdate'
BSP_FOLDERS = {'Drivers', 'Extensions', 'Libraries', 'ProvisioningPkgs', 'Services', 'Settings'}
COMPONENTS = ['components/Touch/q2q_stm_spi', 'components/PEP/q2q_pep_lmh', 'components/WLAN/qcwlanhsp8380']
CATALOG = re.compile(r'^\s*CatalogFile(?:\.[^=\s]+)?\s*=\s*([^;\r\n]+)', re.I | re.M)


def decode(raw):
    for bom, codec in ((b'\xff\xfe', 'utf-16-le'), (b'\xfe\xff', 'utf-16-be'), (b'\xef\xbb\xbf', 'utf-8-sig')):
        if raw.startswith(bom):
            return raw.decode(codec)
    return raw.decode('utf-8', errors='replace')


def package_entries(bsp):
    entries = []
    for inf in sorted(bsp.rglob('*.inf'), key=lambda path: path.relative_to(bsp).as_posix().lower()):
        relative = inf.relative_to(bsp)
        if relative.parts[0] not in BSP_FOLDERS:
            continue
        for name in CATALOG.findall(decode(inf.read_bytes())):
            if not (inf.parent / name.strip().strip('"')).is_file():
                sys.exit(f'Missing catalog for {relative}: {name}')
        entries.append(('bsp/' + relative.parent.as_posix(), inf.name))
    if not entries:
        sys.exit(f'No BSP packages found in {bsp}; run Fetch-Dependencies.ps1 first')
    for component in COMPONENTS:
        infs = sorted((DRIVERS / component).glob('*.inf'))
        if len(infs) != 1 and not component.startswith('components/WLAN'):
            sys.exit(f'Expected exactly one INF in {component}')
        entries.append((component, infs[0].name if infs else 'qcwlanhsp8380.inf'))
    return entries


def build(entries):
    ET.register_namespace('', NAMESPACE)
    manifest = ET.Element(f'{{{NAMESPACE}}}FeatureManifest', Revision='1', SchemaVersion='1.3')
    base = ET.SubElement(ET.SubElement(manifest, f'{{{NAMESPACE}}}Drivers'), f'{{{NAMESPACE}}}BaseDriverPackages')
    for folder, name in entries:
        ET.SubElement(base, f'{{{NAMESPACE}}}DriverPackageFile', {'Path': '$(mspackageroot)\\' + folder.replace('/', '\\'), 'Name': name})
    ET.indent(manifest, space='    ')
    return ET.tostring(manifest, encoding='utf-8', xml_declaration=True) + b'\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--bsp', type=Path, default=DRIVERS / 'bsp')
    parser.add_argument('--check', action='store_true', help='fail if definitions/q2q.xml is out of date')
    args = parser.parse_args()
    content = build(package_entries(args.bsp.resolve()))
    if args.check:
        if OUTPUT.read_bytes() != content:
            sys.exit('definitions/q2q.xml is out of date')
        print('definitions/q2q.xml is up to date')
        return
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(content)
    print(f'Wrote {OUTPUT} ({content.count(b"<DriverPackageFile")} packages)')


if __name__ == '__main__':
    main()
