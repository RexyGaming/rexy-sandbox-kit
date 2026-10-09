# Rexy Sandbox Kit

Build your own **Sandbox practice levels** for [Rexy Bridge](https://github.com/RexyGaming/rexy-bridge-releases) — Unreal levels with a crane, a free camera and repeatable background action that Rexy Bridge launches and drives, so camera operators can practise on them.

You can build levels by hand in Unreal, or let **Claude Code** build them from a description ("three people cross the street, one stops and looks at camera, a car passes at 12 s") — this kit has the plugin, the scripts and the instructions that make that work.

**What's here**

| Folder | What it is |
|---|---|
| `Plugins/RexySandbox/` | The Unreal plugin every Sandbox level needs: blocking viewport, camera monitor windows, rig models, crane smoothing, the in-game action transport, motion blur / frame lock / monitor resolution / fast lighting. C++ source. |
| `Scripts/` | `build_pl_template.py` (the standard template), `build_pl_street.py` (a worked example: three mannequins and a car), `ue_run.py` (run Python in the open editor), `make_player_zip.py` (package for Bridge's installer). |
| `Manifests/` | The level manifest schema and two examples — how Bridge knows a level exists, which map to open and where its action is. |
| `Config/PROJECT_SETTINGS.md` | The project settings a Sandbox project needs. |
| `CLAUDE.md` | Instructions Claude Code reads automatically when you run it in this folder. |
| `PROMPT.md` | Ready-to-paste prompts for Claude Code. |

---

## What you need

- **Unreal Engine 5.8** (Windows or Mac) and a C++ toolchain: **Visual Studio 2022 Build Tools** with the C++ workload on Windows, **Xcode** on Mac (the plugin is C++).
- **Rexy Bridge 3.7+** to drive your level.
- **[Claude Code](https://claude.com/claude-code)** if you want Claude to build levels for you (optional).
- **Python 3.9+** on your PATH (for `ue_run.py` and `make_player_zip.py`).

## 1. Set up an Unreal project (once)

1. Create a new **Blank** project in UE 5.8 (C++ or Blueprint — the plugin compiles either way).
2. Copy `Plugins/RexySandbox` from this kit into your project's `Plugins/` folder.
3. Enable these plugins (**Edit → Plugins**): **Remote Control API**, **Python Editor Script Plugin**, **Sequencer Scripting**.
4. Apply the settings in **[Config/PROJECT_SETTINGS.md](Config/PROJECT_SETTINGS.md)** — most importantly *Enable Remote Execution* (Python) and *Allow Any Remote Function Call* (Remote Control).
5. Restart the editor; let it compile the plugin.
6. For characters and a car to animate: **Add → Add Feature or Content Pack →** *Third Person* (UE5 mannequins + animations) and *Vehicle* (sports car). Or bring your own.

## 2. Build a level

**With Claude Code** — open a terminal in this kit folder, start `claude`, and paste a prompt from **[PROMPT.md](PROMPT.md)**. Claude talks to your open editor through `Scripts/ue_run.py`, builds the level as a re-runnable Python script, renders the camera's view at key moments to check its work, and shows you. Give it notes ("the car should come closer", "make C stop for longer") and it re-times the action in place.

**By hand** — start from the template: run `python Scripts/ue_run.py Scripts/build_pl_template.py` with your project open, then replace the stand-ins with your own content. The rules a level must follow are below.

### What makes a level a Sandbox level

- Lives in **`/Game/PracticeLevels/<LevelId>/`** (that folder is always cooked).
- Has a **crane** (`CameraRig_Crane`) with a **CineCamera** attached to its `CraneCameraMount`, and a **free CineCamera**. (Other camera setups work; this is the standard.)
- Its background action is **one Level Sequence** played by a **LevelSequenceActor tagged `RexyAction`** — *Auto Play* on, *Loop* forever, *Disable Camera Cuts* on. It must play **the same every time** (keyframes and animation clips — no AI, physics or randomness), because recorded camera moves are lined up against it. It doesn't need to loop seamlessly: at the end it jumps back to the start, and so does the camera move.
- Has a **Remote Control preset** exposing the cameras' `RelativeLocation` / `RelativeRotation`, the crane's `CraneYaw`, `CranePitch`, `CraneArmLength`, `bLockMountYaw`, and the `RexyAction` actor's `PlaybackSettings`. That's how Bridge finds everything in a packaged game.
- Has a **manifest** (`Manifests/<LevelId>.json`, see the examples) naming the map, the preset and the sequence.

Keep it light: Sandbox runs alongside camera monitors on ordinary PCs. Check the **fps** readout in Bridge's Sandbox Views.

## 3. Try it in Bridge

1. **Package** the project (*Platforms → Windows / Mac → Package Project*, Development).
2. In Rexy Bridge's **Sandbox** panel: **Use local build…** → pick the packaged game (`.exe` / `.app`).
3. **+ Add level…** → pick your level's manifest.
4. **▶ Launch.**

## 4. Share it (optional)

`python Scripts/make_player_zip.py --build <n> --platform win --packaged <your Builds/Windows folder>` makes the zip Bridge's **Install Sandbox** understands (`sandbox-player.json` inside lists the exe and your levels). Want your level in the official Sandbox? Open an issue or email **rob@realprogear.com**.

---

## Troubleshooting

- **`ue_run.py` says no editor answered** — the editor must be open with *Enable Remote Execution* ticked (Project Settings → Plugins → Python). Firewalls can block its local multicast; allow the editor on private networks.
- **Bridge finds no cameras in your packaged level** — the Remote Control preset is missing or doesn't expose the cameras; or the game was launched without `-RCWebControlEnable` (Bridge adds it for you).
- **The action doesn't show in Bridge** — the LevelSequenceActor must be exposed in the preset and tagged `RexyAction`.
- **Testing a packaged build while the editor is open** — the editor holds Remote Control's ports (30010 / 30020); close it first.
- **A level build failed halfway** — the half-made map can stay in the editor's memory; restart the editor before running the build again.

*Rexy Sandbox Kit · © RealProGear / Rexy Gaming. Questions: rob@realprogear.com*
