"""
Make the Sandbox player zip that Rexy Bridge's "Install Sandbox" downloads.

    python make_player_zip.py --build 1 --platform win  --packaged Builds/Windows
    python make_player_zip.py --build 1 --platform mac  --packaged Builds/Mac

Writes RexySandbox-Player-<platform>-b<build>.zip (next to the packaged folder
unless --out says otherwise). Attach it to a GitHub release on
RexyGaming/rexy-bridge-releases; Bridge picks the newest build for its
platform across releases.

The zip holds:
  RexySandbox/...            the packaged game, minus Saved/ (your logs and
                             settings) and debug symbols (.pdb / .dSYM)
  sandbox-player.json        {build, platform, exe, levels:[manifests]}

Levels: every Manifests/<LevelId>.json (next to this Scripts folder) listed
with --levels (default: PL_Template PL_Street). They must be cooked into this
build.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
MANIFESTS = os.path.abspath(os.path.join(HERE, '..', 'Manifests'))   # Docs/Manifests in a project, Manifests/ in the kit
SKIP_DIRS = {'Saved'}
SKIP_EXT = ('.pdb', '.debug', '.sym')


def find_exe(packaged, platform):
    if platform == 'win':
        exes = [f for f in os.listdir(packaged) if f.lower().endswith('.exe')]
        if len(exes) != 1:
            sys.exit('expected one .exe at the top of %s, found %s' % (packaged, exes))
        return exes[0]
    apps = [f for f in os.listdir(packaged) if f.endswith('.app')]
    if len(apps) != 1:
        sys.exit('expected one .app in %s, found %s' % (packaged, apps))
    return apps[0]


def keep(rel):
    parts = rel.replace('\\', '/').split('/')
    if any(p in SKIP_DIRS for p in parts) or any(p.endswith('.dSYM') for p in parts):
        return False
    if rel.lower().endswith(SKIP_EXT) or os.path.basename(rel).startswith('Manifest_'):
        return False
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', type=int, required=True)
    ap.add_argument('--platform', choices=['win', 'mac'], required=True)
    ap.add_argument('--packaged', required=True, help='the packaged folder (Builds/Windows or Builds/Mac)')
    ap.add_argument('--levels', nargs='*', default=['PL_Template', 'PL_Street'])
    ap.add_argument('--out', default=None)
    a = ap.parse_args()

    packaged = os.path.abspath(a.packaged)
    exe = find_exe(packaged, a.platform)
    levels = []
    for lid in a.levels:
        with open(os.path.join(MANIFESTS, lid + '.json'), encoding='utf-8') as f:
            levels.append(json.load(f))
    manifest = {'build': a.build, 'platform': a.platform, 'exe': 'RexySandbox/' + exe, 'levels': levels}
    name = 'RexySandbox-Player-%s-b%d.zip' % (a.platform, a.build)
    out = os.path.abspath(a.out or os.path.join(os.path.dirname(packaged), name))
    if os.path.exists(out):
        os.remove(out)

    if a.platform == 'mac':
        # ditto keeps the .app's permissions and attributes; Python's zipfile doesn't.
        stage = out + '.stage'
        shutil.rmtree(stage, ignore_errors=True)
        dst = os.path.join(stage, 'RexySandbox')
        shutil.copytree(packaged, dst, symlinks=True,
                        ignore=lambda d, names: [n for n in names if not keep(os.path.relpath(os.path.join(d, n), packaged))])
        with open(os.path.join(stage, 'sandbox-player.json'), 'w', encoding='utf-8') as f:
            json.dump(manifest, f, indent=2, ensure_ascii=False)
        subprocess.check_call(['ditto', '-c', '-k', '--sequesterRsrc', stage, out])
        shutil.rmtree(stage, ignore_errors=True)
    else:
        n = 0
        with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
            z.writestr('sandbox-player.json', json.dumps(manifest, indent=2, ensure_ascii=False))
            for root, dirs, files in os.walk(packaged):
                dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
                for fn in files:
                    full = os.path.join(root, fn)
                    rel = os.path.relpath(full, packaged)
                    if keep(rel):
                        z.write(full, 'RexySandbox/' + rel.replace('\\', '/'))
                        n += 1
        print('files:', n)
    print('wrote', out, '%.0f MB' % (os.path.getsize(out) / 1048576))
    print('exe:', manifest['exe'], '| levels:', [l['levelId'] for l in levels])


if __name__ == '__main__':
    main()
