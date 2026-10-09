"""
Run Python inside the open Unreal Editor (Python remote execution).

    python ue_run.py path/to/script.py
    python ue_run.py "import unreal; print(unreal.SystemLibrary.get_engine_version())"

Needs, in the project: the Python Editor Script Plugin, and
Project Settings -> Plugins -> Python -> Enable Remote Execution (ticked).
Finds Epic's remote_execution.py in the engine; set UE_ENGINE_DIR to the
engine folder (the one containing Engine/) if it's somewhere unusual.
Prints the script's output and exits 0 on success, 1 on a Python error,
2 if no editor answered.
"""
import glob
import os
import sys
import time

CANDIDATES = [
    os.environ.get('UE_ENGINE_DIR', ''),
    *sorted(glob.glob(r'C:/Program Files/Epic Games/UE_5.*'), reverse=True),
    *sorted(glob.glob('/Users/Shared/Epic Games/UE_5.*'), reverse=True),
]
REL = 'Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python'
for root in CANDIDATES:
    if root and os.path.exists(os.path.join(root, REL, 'remote_execution.py')):
        sys.path.insert(0, os.path.join(root, REL))
        break
else:
    sys.exit('remote_execution.py not found — set UE_ENGINE_DIR to your Unreal Engine folder')

import remote_execution as rx  # noqa: E402

if len(sys.argv) < 2:
    sys.exit(__doc__)
src = sys.argv[1]
code = open(src, encoding='utf-8').read() if src.endswith('.py') and os.path.exists(src) else src

r = rx.RemoteExecution()
r.start()
try:
    node = None
    for _ in range(50):                      # ~10 s for the editor to answer
        if r.remote_nodes:
            node = r.remote_nodes[0]
            break
        time.sleep(0.2)
    if not node:
        print('No Unreal Editor answered. Is it open, with Python remote execution enabled?')
        sys.exit(2)
    r.open_command_connection(node.get('node_id') if isinstance(node, dict) else node)
    res = r.run_command(code, unattended=True, exec_mode=rx.MODE_EXEC_FILE)
    for o in res.get('output', []):
        print(o.get('output', '').rstrip())
    if res.get('success'):
        print('OK')
    else:
        print('FAILED:', res.get('result'))
        sys.exit(1)
finally:
    r.stop()
