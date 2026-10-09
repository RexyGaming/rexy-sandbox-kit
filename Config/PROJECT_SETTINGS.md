# Project settings for a Sandbox project

Set these in the editor (**Edit → Project Settings**) or paste the ini lines into your project's `Config/` files. Restart the editor afterwards.

## Plugins (Edit → Plugins)

- **Remote Control API** — how Bridge drives the game.
- **Python Editor Script Plugin** + **Sequencer Scripting** — how scripts (and Claude Code) build levels.
- **RexySandbox** — copied into `Plugins/` from this kit.

## Remote Control — `Config/DefaultRemoteControl.ini`

```ini
[/Script/RemoteControlCommon.RemoteControlSettings]
RemoteControlHttpServerPort=30010
RemoteControlWebSocketServerPort=30020
RemoteControlWebsocketServerBindAddress=0.0.0.0
bAllowAnyRemoteFunctionCall=True
```

*Project Settings → Plugins → Remote Control → Security → **Allow Any Remote Function Call*** — UE 5.8 blocks remote function calls without it.

## Python remote execution — `Config/DefaultEngine.ini`

```ini
[/Script/PythonScriptPlugin.PythonScriptPluginSettings]
bRemoteExecution=True
```

*Project Settings → Plugins → Python → **Enable Remote Execution***.

## Packaged game behaviour — `Config/DefaultEngine.ini`

```ini
[/Script/Engine.UserInterfaceSettings]
bAllowHighDPIInGameMode=True
```

Without it, high-DPI screens (laptops at 150–250 %) render the game blurry and oversized.

## Packaging — `Config/DefaultGame.ini`

```ini
[/Script/UnrealEd.ProjectPackagingSettings]
+DirectoriesToAlwaysCook=(Path="/Game/PracticeLevels")
+DirectoriesToAlwaysCook=(Path="/Engine/EditorMeshes/Camera")
```

Every level under `/Game/PracticeLevels` is cooked; the camera model is what the plugin shows for rigs in the blocking view.

## Input — `Config/DefaultInput.ini`

```ini
[/Script/Engine.InputSettings]
bAlwaysShowTouchInterface=False
```
