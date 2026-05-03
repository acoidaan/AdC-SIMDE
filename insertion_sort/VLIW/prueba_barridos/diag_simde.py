#!/usr/bin/env python3
"""Diagnóstico v2: usa -s y -v en vez de -i, y -c con 5 valores."""

import subprocess
import sys
from pathlib import Path

JAR = 'SIMDE-VLIW-Lite.jar'


def main():
    if len(sys.argv) < 2:
        print(f'Uso: {sys.argv[0]} <basename> [mem]', file=sys.stderr)
        sys.exit(1)

    basename = sys.argv[1]
    mem = sys.argv[2] if len(sys.argv) > 2 else 'order1.mem'

    if not Path(JAR).exists():
        print(f'ERROR: no se encuentra "{JAR}"', file=sys.stderr)
        sys.exit(1)

    cmd = ['java', '-jar', JAR,
           '-s', f'{basename}.pla',
           '-v', f'{basename}.vliw',
           '-m', mem,
           '-c', '2,2,2,2,2']
    print(f'Comando: {" ".join(cmd)}')
    print('=' * 60)

    try:
        out = subprocess.check_output(cmd, stderr=subprocess.STDOUT,
                                      text=True, timeout=120)
    except subprocess.CalledProcessError as e:
        print(f'EXIT CODE: {e.returncode}')
        print(e.output)
        sys.exit(1)

    print('--- repr ---')
    print(repr(out))
    print()
    print('--- bonito ---')
    print(out)


if __name__ == '__main__':
    main()