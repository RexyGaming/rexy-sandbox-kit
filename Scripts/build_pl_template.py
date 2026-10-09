"""
Build PL_Template — the standard Rexy practice-level template (Bridge 3.7).

Run inside the UE 5.8 editor (Tools -> Execute Python Script, or remote
execution). Creates /Game/PracticeLevels/PL_Template/ with:
  - PL_Template        level from the engine Basic template (floor, sun, sky)
  - Crane + Crane Cam  CameraRig_Crane with a CineCamera on CraneCameraMount
  - Free Cam           free CineCamera
  - Stand-ins          capsule walkers + a box vehicle (placeholders for real
                       characters — swap them for your own content)
  - LS_Action          20 s looping Level Sequence animating the stand-ins
  - RexyAction         LevelSequenceActor: autoplay, loop forever, camera
                       cuts off (never steals the view from Bridge)
  - PL_Template_RC     Remote Control preset — how Bridge discovers the rigs
                       and the action in a packaged build
Refuses to run if PL_Template already exists (never overwrites your work).
"""
import math
import unreal

FOLDER = '/Game/PracticeLevels/PL_Template'
MAP = FOLDER + '/PL_Template'
SEQ_NAME = 'LS_Action'
PRESET_NAME = 'PL_Template_RC'
FPS = 30
LENGTH = 600  # frames = 20 s

EAL = unreal.EditorAssetLibrary
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
AT = unreal.AssetToolsHelpers.get_asset_tools()

if EAL.does_asset_exist(MAP):
    raise RuntimeError(MAP + ' already exists — not overwriting. Delete it first to rebuild.')

LES.new_level_from_template(MAP, '/Engine/Maps/Templates/Template_Default')


def spawn(cls, loc, rot=(0, 0, 0), label=None):
    a = EAS.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]))
    if label:
        a.set_actor_label(label)
    return a


def look_at_yaw_pitch(src, dst):
    dx, dy, dz = dst[0] - src[0], dst[1] - src[1], dst[2] - src[2]
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    return yaw, pitch


# ── cameras ─────────────────────────────────────────────────────────────────
crane = spawn(unreal.CameraRig_Crane, (-900, 0, 0), label='Crane')
crane.set_editor_property('crane_arm_length', 500.0)
crane.set_editor_property('crane_pitch', 10.0)
mount = [c for c in crane.get_components_by_class(unreal.SceneComponent) if c.get_name() == 'CraneCameraMount'][0]
crane_cam = spawn(unreal.CineCameraActor, (0, 0, 0), label='Crane Cam')
crane_cam.root_component.attach_to_component(mount, '', unreal.AttachmentRule.SNAP_TO_TARGET,
                                             unreal.AttachmentRule.SNAP_TO_TARGET,
                                             unreal.AttachmentRule.KEEP_WORLD, False)

free_pos = (700, -700, 160)
yaw, pitch = look_at_yaw_pitch(free_pos, (0, 0, 100))
free_cam = spawn(unreal.CineCameraActor, free_pos, (0, pitch, yaw), label='Free Cam')

# ── stand-ins ───────────────────────────────────────────────────────────────
EAL.make_directory(FOLDER + '/Materials')
base_mat = unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial')


def stand_in_material(name, rgb):
    mi = AT.create_asset(name, FOLDER + '/Materials', unreal.MaterialInstanceConstant,
                         unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, base_mat)
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
        mi, 'Color', unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return mi


def stand_in(label, mesh, loc, scale, mat):
    a = spawn(unreal.StaticMeshActor, loc, label=label)
    smc = a.static_mesh_component
    smc.set_static_mesh(unreal.load_asset(mesh))
    smc.set_material(0, mat)
    smc.set_mobility(unreal.ComponentMobility.MOVABLE)
    a.set_actor_scale3d(unreal.Vector(*scale))
    a.tags = ['RexyStandIn']
    return a


CYL = '/Engine/BasicShapes/Cylinder'
CUBE = '/Engine/BasicShapes/Cube'
walker_a = stand_in('StandIn_WalkerA', CYL, (-300, -300, 90), (0.5, 0.5, 1.8), stand_in_material('MI_StandIn_A', (0.9, 0.25, 0.1)))
walker_b = stand_in('StandIn_WalkerB', CYL, (-500, 500, 90), (0.5, 0.5, 1.8), stand_in_material('MI_StandIn_B', (0.1, 0.45, 0.9)))
vehicle = stand_in('StandIn_Vehicle', CUBE, (900, -3000, 75), (4.0, 2.0, 1.5), stand_in_material('MI_StandIn_Vehicle', (0.95, 0.8, 0.1)))

# ── the looping action ──────────────────────────────────────────────────────
seq = AT.create_asset(SEQ_NAME, FOLDER, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
seq.set_display_rate(unreal.FrameRate(FPS, 1))
seq.set_playback_start(0)
seq.set_playback_end(LENGTH)

LINEAR = unreal.MovieSceneKeyInterpolation.LINEAR
CONSTANT = unreal.MovieSceneKeyInterpolation.CONSTANT


def animate(actor, z, scale, xy_keys, yaw_keys, interp=LINEAR):
    """xy_keys: [(frame, x, y)], yaw_keys: [(frame, yaw)] — first and last keys
    match so the loop is seamless."""
    binding = seq.add_possessable(actor)
    track = binding.add_track(unreal.MovieScene3DTransformTrack)
    section = track.add_section()
    section.set_range(0, LENGTH)
    ch = section.get_all_channels()  # loc X,Y,Z, rot X(roll),Y(pitch),Z(yaw), scale X,Y,Z
    ch[2].set_default(z)
    ch[3].set_default(0.0)
    ch[4].set_default(0.0)
    for i, s in enumerate(scale):
        ch[6 + i].set_default(s)
    for f, x, y in xy_keys:
        ch[0].add_key(unreal.FrameNumber(f), x, interpolation=interp)
        ch[1].add_key(unreal.FrameNumber(f), y, interpolation=interp)
    for f, yv in yaw_keys:
        ch[5].add_key(unreal.FrameNumber(f), yv, interpolation=LINEAR)


# Walker A: a 6 m square circuit, 5 s per side, turning at each corner.
sq = [(-300, -300), (300, -300), (300, 300), (-300, 300), (-300, -300)]
animate(walker_a, 90, (0.5, 0.5, 1.8),
        [(i * 150, x, y) for i, (x, y) in enumerate(sq)],
        [(0, 0), (140, 0), (150, 90), (290, 90), (300, 180), (440, 180), (450, 270), (590, 270), (600, 360)])
# Walker B: walks 10 m across, pauses, walks back, pauses.
animate(walker_b, 90, (0.5, 0.5, 1.8),
        [(0, -500, 500), (250, 500, 500), (300, 500, 500), (550, -500, 500), (600, -500, 500)],
        [(0, 0), (250, 0), (265, 180), (550, 180), (565, 360), (600, 360)])
# Vehicle: waits off-screen, crosses 60 m in 6 s, waits off-screen again.
animate(vehicle, 75, (4.0, 2.0, 1.5),
        [(0, 900, -3000), (200, 900, -3000), (380, 900, 3000), (599, 900, 3000), (600, 900, -3000)],
        [(0, 90), (600, 90)])

action = spawn(unreal.LevelSequenceActor, (0, 0, 0), label='RexyAction')
action.set_sequence(seq)
ps = action.get_editor_property('playback_settings')
ps.set_editor_property('auto_play', True)
loop = ps.get_editor_property('loop_count')
loop.set_editor_property('value', -1)          # loop forever
ps.set_editor_property('loop_count', loop)
ps.set_editor_property('disable_camera_cuts', True)
action.set_editor_property('playback_settings', ps)
action.tags = ['RexyAction']

# ── Remote Control preset: how Bridge finds everything in a packaged game ───
preset = AT.create_asset(PRESET_NAME, FOLDER, unreal.RemoteControlPreset, unreal.RemoteControlPresetFactory())
RCFL = unreal.RemoteControlFunctionLibrary
args = unreal.RemoteControlOptionalExposeArgs()
exposed = []
for cam in (crane_cam, free_cam):
    for prop in ('RelativeLocation', 'RelativeRotation'):
        exposed.append((cam.get_actor_label() + ' ' + prop, RCFL.expose_property(preset, cam.root_component, prop, args)))
for prop in ('CraneYaw', 'CranePitch', 'CraneArmLength', 'bLockMountYaw'):
    exposed.append(('Crane ' + prop, RCFL.expose_property(preset, crane, prop, args)))
exposed.append(('RexyAction PlaybackSettings', RCFL.expose_property(preset, action, 'PlaybackSettings', args)))

LES.save_current_level()
EAL.save_directory(FOLDER, only_if_is_dirty=False, recursive=True)

print('BUILT', MAP)
for name, ok in exposed:
    print('  exposed' if ok else '  FAILED ', name)
print('actors:', [a.get_name() + '=' + a.get_actor_label() for a in (crane, crane_cam, free_cam, walker_a, walker_b, vehicle, action)])
