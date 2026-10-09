# Prompts for Claude Code

Open a terminal **in this kit folder**, have your Unreal project **open in the editor**, run `claude`, and paste one of these. Claude reads `CLAUDE.md` automatically, so it already knows the rules — your job is to describe the scene.

---

## Build a new level

> Build a new Sandbox practice level called **PL_Park** (display name "Park — Joggers & Bench").
> 20 seconds. A jogger runs left to right in front of the crane at about 2 s. Two people walk in from the right, stop centre frame around 6 s, stand and talk (idle) for 4 s, then walk off left. At 12 s a fast walker crosses at the back, right to left. At 15 s the jogger comes back the other way.
> Use the UE5 mannequins. Keep events spread out so the crane's frame is never empty for more than 3 seconds. Show me the crane's view at the key moments when it's built.

Tips:

- **Describe beats with times** ("at 6 s", "for 4 s") and **where** ("in front of the crane", "at the back", "left to right").
- **Name the level** `PL_Something` — letters, numbers and underscores.
- Say what the practice is for: "for pan practice", "to time a crane move to the car".

## Change an existing level

> In PL_Park: make the two walkers stop for 6 seconds instead of 4, move the jogger 2 m further from camera, and make the whole thing 25 seconds long. Re-time it and show me the frames again.

Claude re-runs the level's build script in update mode — it re-times the action and keeps your cameras, preset and any changes you made by hand.

## Start from the template

> Build the standard template level (`Scripts/build_pl_template.py`), then replace the capsule stand-ins with two mannequins walking the same paths.

## Package and check it

> Package the project for Windows and test PL_Park in the packaged game: action playing, cameras and crane in the preset, everyone moving. Then write its manifest and tell me how to add it in Rexy Bridge.

## Make the Bridge installer zip

> Make the Sandbox player zip (build 1, Windows) with PL_Template, PL_Street and PL_Park.

---

**If something goes wrong**, tell Claude what you saw ("the walker is facing backwards", "the car is floating"). It can render the camera's view to check, and fix the script.
