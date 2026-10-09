# Building Rexy Sandbox levels — instructions for Claude Code

You are helping someone build a **Sandbox practice level**: an Unreal Engine 5.8 level with a crane, a free camera and repeatable background action, which **Rexy Bridge** launches and drives so camera operators can practise. Read `README.md` for the level rules; this file is how to do the work well.

## How you work with Unreal

- The user's **Unreal Editor must be open** on their project. You drive it with **Python remote execution**:
  `python Scripts/ue_run.py <script.py>` or `python Scripts/ue_run.py "<python code>"`. Exit 0 = OK, 1 = a Python error (traceback printed), 2 = no editor answered (ask them to open it / enable *Enable Remote Execution*).
- **Write each level as a re-runnable script** `Scripts/build_<levelid>.py`, modelled on `Scripts/build_pl_street.py`: first run builds the level; later runs only **re-time the action** in place (clear the sequence's bindings, re-key), keeping the level, cameras, preset and anything the user changed by hand. Never delete or overwrite the user's work without asking.
- Before building, check the open level has **no unsaved changes** (`unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()` / `get_dirty_content_packages()`). Creating or loading a level could prompt to save and hang a remote script — ask the user to save first.
- **Check every step's result.** `LevelEditorSubsystem.new_level_from_template` and `save_current_level` return False on failure — raise if so, and confirm the editor world's path is the new map before spawning anything.
- **If a build fails halfway**, the half-made map stays in the editor's memory and the next `new_level_from_template` on that path silently fails. Delete the level's assets, **restart the editor** (save-check, `unreal.SystemLibrary.quit_editor()`, relaunch), then rebuild. Detect "already exists" with the asset registry (`get_assets_by_path(folder)`), not `does_asset_exist` (which also sees the in-memory ghost).

## Levels

- Folder `/Game/PracticeLevels/<LevelId>/` (e.g. `PL_Park`), map `<LevelId>`, sequence `LS_Action`, preset `<LevelId>_RC`. Start from `/Engine/Maps/Templates/Template_Default` (floor, sun, sky).
- Cameras as in the template: `CameraRig_Crane` at about (-900, 0, 0), arm 500, pitch 10, a CineCamera **snapped to its `CraneCameraMount`**, plus a free CineCamera placed **out of the action's paths**, looking at it.
- The action: one `LevelSequence` (30 fps), a `LevelSequenceActor` labelled `RexyAction`, **tagged `RexyAction`**, playback settings auto play on, loop count -1, disable camera cuts on.
- The Remote Control preset: expose `RelativeLocation` and `RelativeRotation` on both cameras' root components, `CraneYaw`, `CranePitch`, `CraneArmLength`, `bLockMountYaw` on the crane, and `PlaybackSettings` on the action actor (`unreal.RemoteControlFunctionLibrary.expose_property`). Report which exposures succeeded.
- Write the manifest `Manifests/<LevelId>.json` (copy `Manifests/PL_Street.json`, change `levelId`, `displayName`, `description`, `levelAsset`, `launch.mapArgument`, the preset and the sequence paths). If you can find Bridge's levels folder (Windows `%APPDATA%\rexy-bridge\sandbox\levels\<LevelId>\level.json`, macOS `~/Library/Application Support/rexy-bridge/sandbox/levels/...`), install it there too — or tell the user to use **+ Add level…**.

## Animating — the action must be identical every time

Keyframes and animation clips only — no AI, physics, randomness or anything time-of-day. Recorded camera moves are lined up against it.

- **Characters**: the UE5 mannequins (`/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple`, `SKM_Quinn_Simple`) as `SkeletalMeshActor`s, animation mode *single node*. Clips in `/Game/Characters/Mannequins/Anims/Unarmed/`: `Walk/MF_Unarmed_Walk_Fwd`, `Jog/MF_Unarmed_Jog_Fwd`, `MM_Idle` (+ strafes, turns aren't included). If they're missing, ask the user to add the *Third Person* content pack.
- **No foot sliding**: measure each clip's root-motion speed (`unreal.AnimationLibrary.get_bone_pose_for_time(anim, 'root', t, False)` at 0 and at `get_play_length()`); move the actor at exactly that speed × the play rate. Measured on the UE5 mannequin: **walk 300 cm/s, jog ≈ 600 cm/s** at rate 1 (`force_root_lock` is on, so the mesh stays on the actor). Slow a walker with play rate (0.8–0.9 reads as an unhurried walk); don't slow the actor alone.
- **Facing**: the mannequin faces **+Y** in its own space, so actor yaw = heading − 90° (walking +Y → yaw 0; −Y → 180; towards a camera looking down +X → 90).
- **Stops and turns**: walk → idle → walk on one skeletal-animation track, overlapping sections by ~10 frames with ease in/out ~12 frames; hold the position keys still during the idle; key the yaw to turn during the idle.
- **UE 5.8 Python specifics**: a section's play rate is a `MovieSceneTimeWarpVariant` — `pr = unreal.MovieSceneTimeWarpVariant(); pr.set_fixed_play_rate(rate)` then set `params.play_rate = pr`. Easing is `section.set_ease_in_duration(frames)` / `set_ease_out_duration(frames)`. Transform channels from `section.get_all_channels()` are loc X, Y, Z, rot roll, pitch, yaw, scale X, Y, Z.
- **Vehicles**: the vehicle template's `/Game/Vehicles/SportsCar/SM_SportsCar` (+ `SM_SportsCar_Glass` attached) as static meshes, length along +X (yaw 90 drives along +Y). Its wheels don't turn (static mesh) — say so; a rigged car is a bigger job.
- **Frame the action for the cameras.** The default crane lens is narrow (~37° across): 3–7 m out it sees only ~4–5 m. Spread events across the sequence so the frame isn't empty for long, and put each beat where the camera can find it. Typical length 20 s (600 frames); the sequence doesn't need a seamless loop — it jumps back to the start.

## Check your work by looking

- Render the **crane camera's view** at the key moments: open the sequence (`LevelSequenceEditorBlueprintLibrary.open_level_sequence`), then **one remote call per frame**: `set_current_time(frame)`, `refresh_current_level_sequence()`, `unreal.AutomationLibrary.take_high_res_screenshot(960, 540, path, crane_cam_actor)` — the screenshot is written on the editor's next tick, so wait ~2–3 s between calls. (A temporary SceneCapture2D comes out white — no exposure history — don't use it.) Close the sequence afterwards.
- Look at the images yourself: are characters on the floor, facing their direction of travel, inside the frame when they should be, and is anything empty for too long? Show the user a contact sheet and describe what happens when.
- Also print each mover's position at those frames — it catches path mistakes the pictures don't.

## Packaging and testing

- The user packages (or you, with their OK): *Platforms → Package Project*, or `RunUAT BuildCookRun -project=... -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory=<project>/Builds -unattended`. If only content changed and the editor is open, add `-skipbuild -nocompileeditor`. Don't package while the game is running (the exe is locked).
- Test the packaged level: **close the editor first** (it holds Remote Control's ports 30010/30020), launch `<exe> /Game/PracticeLevels/<Id>/<Id> -RCWebControlEnable -windowed -ResX=1280 -ResY=720`, then over HTTP `PUT http://127.0.0.1:30010/remote/object/call`: the action player (`<Level>:PersistentLevel.LevelSequenceActor_0.AnimationPlayer`) answers `IsPlaying` / `GetDuration`, actors move (`K2_GetActorLocation` twice), and `GET /remote/preset/<Id>_RC` lists the cameras, crane and action. Quit with `KismetSystemLibrary.QuitGame`.

## Style

Explain what you'll build in a few lines first (beats with times), then build, check, and show. Keep levels light — they run next to camera monitors on ordinary PCs. Ask before installing content packs, deleting assets, restarting the editor or packaging.
