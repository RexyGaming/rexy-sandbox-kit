"""
Build PL_Street — three walkers and a car (Bridge 3.7 practice level).

Run inside the UE 5.8 editor (Tools -> Execute Python Script, or remote
execution). Needs the UE5 mannequins in /Game/Characters/Mannequins and the
vehicle template's sports car in /Game/Vehicles/SportsCar (engine template
content, copied in like "Add Feature or Content Pack").

Creates /Game/PracticeLevels/PL_Street/ with:
  - PL_Street         level from the engine Basic template (floor, sun, sky)
  - Crane + Crane Cam / Free Cam   same rigs as PL_Template
  - Walker A/B/C      mannequins walking at stride speed (no foot slide)
  - Car               sports car crossing behind them
  - LS_Action         20 s Level Sequence; it plays to the end and jumps back
                      (no seamless loop needed — the camera move restarts too)
  - RexyAction        LevelSequenceActor (autoplay, camera cuts off)
  - PL_Street_RC      Remote Control preset — how Bridge finds it all

Run it again on an existing PL_Street and it only re-times the action (the
walker + car keys in LS_Action), keeping the level, cameras, preset and
anything you added. Edit the timings below and rerun to iterate.

The walk clip moves its root 300 cm/s at play rate 1 (measured), so each
walker's actor moves at 300 x play-rate cm/s along its heading.
"""
import math
import unreal

FOLDER = '/Game/PracticeLevels/PL_Street'
MAP = FOLDER + '/PL_Street'
PRESET_NAME = 'PL_Street_RC'
FPS = 30
LENGTH = 600                      # frames = 20 s
WALK_SPEED = 300.0                # cm/s of MF_Unarmed_Walk_Fwd at rate 1 (root motion, measured)

MANNY = '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'
QUINN = '/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple'
WALK = '/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd'
IDLE = '/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle'
CAR = '/Game/Vehicles/SportsCar/SM_SportsCar'
CAR_GLASS = '/Game/Vehicles/SportsCar/SM_SportsCar_Glass'

EAL = unreal.EditorAssetLibrary
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
AT = unreal.AssetToolsHelpers.get_asset_tools()

for p in (MANNY, QUINN, WALK, IDLE, CAR, CAR_GLASS):
    if not EAL.does_asset_exist(p):
        raise RuntimeError('missing ' + p + ' — copy the mannequin / vehicle template content in first')


def world_path():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name().split('.')[0]


# Saved work counts (the asset registry), not a deleted level the editor still holds in memory.
UPDATE = bool(unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(FOLDER, recursive=True))
if UPDATE:
    if not EAL.does_asset_exist(FOLDER + '/LS_Action'):
        raise RuntimeError(FOLDER + ' exists but has no LS_Action — not touching it')
    if world_path() != MAP:
        LES.load_level(MAP)
    if world_path() != MAP:
        raise RuntimeError('could not open ' + MAP)
else:
    if not LES.new_level_from_template(MAP, '/Engine/Maps/Templates/Template_Default'):
        raise RuntimeError('could not create ' + MAP + ' (a deleted copy still in memory? restart the editor)')
    if world_path() != MAP:
        raise RuntimeError('the new level did not open — nothing built')
EXISTING = {a.get_actor_label(): a for a in EAS.get_all_level_actors()}


def spawn(cls, loc, yaw=0.0, pitch=0.0, label=None):
    if label and label in EXISTING:                  # re-time run: reuse what's there
        return EXISTING[label]
    a = EAS.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    if label:
        a.set_actor_label(label)
    return a


def look_at(src, dst):
    dx, dy, dz = dst[0] - src[0], dst[1] - src[1], dst[2] - src[2]
    return math.degrees(math.atan2(dy, dx)), math.degrees(math.atan2(dz, math.hypot(dx, dy)))


# ── cameras (as PL_Template; the free cam stands clear of the walkers) ──────
crane = spawn(unreal.CameraRig_Crane, (-900, 0, 0), label='Crane')
crane_cam = spawn(unreal.CineCameraActor, (0, 0, 0), label='Crane Cam')
if not UPDATE:
    crane.set_editor_property('crane_arm_length', 500.0)
    crane.set_editor_property('crane_pitch', 10.0)
    mount = [c for c in crane.get_components_by_class(unreal.SceneComponent) if c.get_name() == 'CraneCameraMount'][0]
    crane_cam.root_component.attach_to_component(mount, '', unreal.AttachmentRule.SNAP_TO_TARGET,
                                                 unreal.AttachmentRule.SNAP_TO_TARGET,
                                                 unreal.AttachmentRule.KEEP_WORLD, False)
free_pos = (-200, -1100, 160)
fy, fp = look_at(free_pos, (450, 0, 100))
free_cam = spawn(unreal.CineCameraActor, free_pos, fy, fp, label='Free Cam')

# ── the action ───────────────────────────────────────────────────────────────
if UPDATE:
    seq = unreal.load_asset(FOLDER + '/LS_Action')
    for b in list(seq.get_bindings()):               # re-time from scratch
        b.remove()
else:
    seq = AT.create_asset('LS_Action', FOLDER, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
seq.set_display_rate(unreal.FrameRate(FPS, 1))
seq.set_playback_start(0)
seq.set_playback_end(LENGTH)

LINEAR = unreal.MovieSceneKeyInterpolation.LINEAR
AUTO = unreal.MovieSceneKeyInterpolation.AUTO


def transform_track(actor, z, xy_keys, yaw_keys, xy_interp=LINEAR):
    """xy_keys [(frame, x, y)], yaw_keys [(frame, yaw)]."""
    binding = seq.add_possessable(actor)
    track = binding.add_track(unreal.MovieScene3DTransformTrack)
    sec = track.add_section()
    sec.set_range(0, LENGTH)
    ch = sec.get_all_channels()      # loc X,Y,Z, rot roll,pitch,yaw, scale X,Y,Z
    ch[2].set_default(z)
    ch[3].set_default(0.0)
    ch[4].set_default(0.0)
    for i in range(3):
        ch[6 + i].set_default(1.0)
    for f, x, y in xy_keys:
        ch[0].add_key(unreal.FrameNumber(f), x, interpolation=xy_interp)
        ch[1].add_key(unreal.FrameNumber(f), y, interpolation=xy_interp)
    for f, yv in yaw_keys:
        ch[5].add_key(unreal.FrameNumber(f), yv, interpolation=AUTO)
    return binding


def anim_section(binding, anim_path, start, end, rate=1.0, ease_in=0, ease_out=0, track=None):
    track = track or binding.add_track(unreal.MovieSceneSkeletalAnimationTrack)
    sec = track.add_section()
    sec.set_range(start, end)
    params = sec.get_editor_property('params')
    params.set_editor_property('animation', unreal.load_asset(anim_path))
    pr = unreal.MovieSceneTimeWarpVariant()          # UE 5.8: play rate is a time-warp variant
    pr.set_fixed_play_rate(rate)
    params.set_editor_property('play_rate', pr)
    sec.set_editor_property('params', params)
    if ease_in:
        sec.set_ease_in_duration(ease_in)            # frames: blend from the clip before
    if ease_out:
        sec.set_ease_out_duration(ease_out)
    return track


def walker(label, mesh, x):
    a = spawn(unreal.SkeletalMeshActor, (x, 0, 0), label=label)
    smc = a.skeletal_mesh_component
    smc.set_skeletal_mesh_asset(unreal.load_asset(mesh))
    smc.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    a.tags = ['RexyWalker']
    return a


def stop_and_turn(binding, rate, walk_end, walk_again):
    """Walk -> idle (blended) -> walk again, on one animation track."""
    t = anim_section(binding, WALK, 0, walk_end + 8, rate, ease_out=12)
    anim_section(binding, IDLE, walk_end - 4, walk_again + 6, 1.0, ease_in=12, ease_out=12, track=t)
    anim_section(binding, WALK, walk_again - 6, LENGTH, rate, ease_in=12, track=t)


# The mannequin faces +Y in its own space: actor yaw = heading - 90.
# Heading +Y (screen left -> right from the crane) = yaw 0; heading -Y = yaw 180;
# facing the crane (heading -X) = yaw 90.
# The crane's lens is narrow (~37 degrees across): 3-7 m out it sees only
# ~4-5 m of street, so the events are spread across the 20 s.

# Walker A — in frame almost at once, walks out right, stops, turns round and
# walks back through frame (~11 s).
rate_a = 0.85
va = WALK_SPEED * rate_a
wa = walker('Walker A', MANNY, 300)
a_stop, a_go = 176, 226
ya0 = -400.0
ya_turn = ya0 + va * a_stop / FPS
ba = transform_track(wa, 0,
                     [(0, 300, ya0), (a_stop, 300, ya_turn), (a_go, 300, ya_turn),
                      (LENGTH, 300, ya_turn - va * (LENGTH - a_go) / FPS)],
                     [(0, 0.0), (186, 0.0), (210, 180.0)])
stop_and_turn(ba, rate_a, a_stop, a_go)

# Walker C — closest. Enters ~3 s, stops centre frame, turns to look at the
# camera, turns back, walks on.
rate_c = 0.8
vc = WALK_SPEED * rate_c
wc = walker('Walker C', MANNY, 120)
c_stop, c_go = 112, 232
yc0 = -vc * c_stop / FPS
bc = transform_track(wc, 0,
                     [(0, 120, yc0), (c_stop, 120, 0.0), (c_go, 120, 0.0),
                      (LENGTH, 120, vc * (LENGTH - c_go) / FPS)],
                     [(0, 0.0), (134, 0.0), (154, 90.0), (200, 90.0), (220, 0.0)])
stop_and_turn(bc, rate_c, c_stop, c_go)

# Walker B — right to left, further back, full brisk pace, crosses ~8-10 s.
rate_b = 1.0
vb = WALK_SPEED * rate_b
wb = walker('Walker B', QUINN, 700)
yb0 = 370 + vb * 8                                   # enters frame (~3.7 m right) at 8 s
bb = transform_track(wb, 0, [(0, 700, yb0), (LENGTH, 700, yb0 - vb * LENGTH / FPS)], [(0, 180.0)])
anim_section(bb, WALK, 0, LENGTH, rate_b)

# Car — crosses behind everyone, left to right, ~36 km/h, 11 s -> 19 s
# (in the crane's frame ~14.5-15.7 s).
CAR_X = 1400
car = spawn(unreal.StaticMeshActor, (CAR_X, -4000, 0), 90.0, label='Car')
car.static_mesh_component.set_static_mesh(unreal.load_asset(CAR))
car.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
glass = spawn(unreal.StaticMeshActor, (CAR_X, -4000, 0), 90.0, label='Car Glass')
glass.static_mesh_component.set_static_mesh(unreal.load_asset(CAR_GLASS))
glass.static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
if glass.root_component.get_attach_parent() is None:
    glass.root_component.attach_to_component(car.root_component, '', unreal.AttachmentRule.KEEP_WORLD,
                                             unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
car.tags = ['RexyVehicle']
transform_track(car, 0, [(0, CAR_X, -4000), (330, CAR_X, -4000), (570, CAR_X, 4000), (LENGTH, CAR_X, 4000)], [(0, 90.0)])

action = spawn(unreal.LevelSequenceActor, (0, 0, 0), label='RexyAction')
action.set_sequence(seq)
ps = action.get_editor_property('playback_settings')
ps.set_editor_property('auto_play', True)
loop = ps.get_editor_property('loop_count')
loop.set_editor_property('value', -1)            # plays to the end, jumps back (Bridge's loop button decides)
ps.set_editor_property('loop_count', loop)
ps.set_editor_property('disable_camera_cuts', True)
action.set_editor_property('playback_settings', ps)
action.tags = ['RexyAction']

# ── Remote Control preset: how Bridge finds everything in a packaged game ───
exposed = []
if not UPDATE:
    preset = AT.create_asset(PRESET_NAME, FOLDER, unreal.RemoteControlPreset, unreal.RemoteControlPresetFactory())
    RCFL = unreal.RemoteControlFunctionLibrary
    args = unreal.RemoteControlOptionalExposeArgs()
    for cam in (crane_cam, free_cam):
        for prop in ('RelativeLocation', 'RelativeRotation'):
            exposed.append((cam.get_actor_label() + ' ' + prop, RCFL.expose_property(preset, cam.root_component, prop, args)))
    for prop in ('CraneYaw', 'CranePitch', 'CraneArmLength', 'bLockMountYaw'):
        exposed.append(('Crane ' + prop, RCFL.expose_property(preset, crane, prop, args)))
    exposed.append(('RexyAction PlaybackSettings', RCFL.expose_property(preset, action, 'PlaybackSettings', args)))

if not LES.save_current_level():
    raise RuntimeError('saving ' + MAP + ' failed')
EAL.save_directory(FOLDER, only_if_is_dirty=False, recursive=True)

print('RE-TIMED' if UPDATE else 'BUILT', MAP)
for name, ok in exposed:
    print('  exposed' if ok else '  FAILED ', name)
print('speeds cm/s: A', va, 'B', vb, 'C', vc, '- car', 8000 / 8.0)
print('actors:', [a.get_name() + '=' + a.get_actor_label() for a in (crane, crane_cam, free_cam, wa, wb, wc, car, action)])
