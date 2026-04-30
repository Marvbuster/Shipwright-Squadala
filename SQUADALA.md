# Squadala — AI Dungeon Generator for Ship of Harkinian

> *"Squadala! We're off!"*

**Squadala** is an experimental mod for [Ship of Harkinian](https://github.com/HarbourMasters/Shipwright) that generates new Zelda OoT dungeons using a local LLM, directly from within the game.

## How It Works

1. Open the **Squadala** panel in-game (ESC → Enhancements → Squadala)
2. Describe a dungeon: *"A 4 room ice dungeon with a Freezard boss"*
3. The AI architect designs rooms, enemies, chests, and connections
4. Click **Enter Dungeon** → walk through any door → you're in!

## Architecture

```
┌────────────────────────┐         ┌──────────────────────────┐
│  Ship of Harkinian     │  HTTP   │  Python Sidecar          │
│  (this fork)           │ <─────> │  (FastAPI on localhost)   │
│                        │  :7777  │                          │
│  ┌──────────────────┐  │         │  ┌────────────────────┐  │
│  │ Squadala Panel   │  │         │  │ LLM (Gemma 4 /     │  │
│  │ (ImGui)          │  │         │  │  Claude / Ollama)   │  │
│  └──────────────────┘  │         │  └────────────────────┘  │
│  ┌──────────────────┐  │         │  ┌────────────────────┐  │
│  │ Entrance Override│  │         │  │ Scene Compiler      │  │
│  │ (Portal System)  │  │         │  │ (DungeonSpec → .o2r)│  │
│  └──────────────────┘  │         │  └────────────────────┘  │
│  ┌──────────────────┐  │         │  ┌────────────────────┐  │
│  │ Hot-Reload       │  │         │  │ Dungeon Store       │  │
│  │ (Runtime .o2r)   │  │         │  │ (~/.squadala/)      │  │
│  └──────────────────┘  │         │  └────────────────────┘  │
└────────────────────────┘         └──────────────────────────┘
```

## Features

- **Local LLM**: Runs on your machine via MLX (Apple Silicon), Ollama, or Anthropic API
- **Structured JSON generation**: 100% success rate dungeon specs
- **Hot-Reload**: No restart needed — generate and enter immediately
- **Dungeon persistence**: Saved dungeons in `~/.squadala/dungeons/`
- **Portal system**: Entrance override redirects any door to your dungeon
- **Dungeon notification**: Shows dungeon name when entering

## Building

Standard SoH build instructions apply. Additional dependency: `libcurl` (linked automatically on macOS).

For the Python sidecar, see the `sidecar/` directory in the companion repo.

## Acknowledgments

This project would not be possible without:

- **[Ship of Harkinian](https://github.com/HarbourMasters/Shipwright)** by HarbourMasters — the incredible OoT PC port that makes all of this possible
- **The SoH Randomizer** — the entrance override and dungeon shuffling systems that Squadala's portal system is built upon. The randomizer team's work on entrance tracking, logic validation, and scene management was essential
- **[OoT Decompilation](https://github.com/zeldaret/oot)** by zeldaret — the complete reverse-engineering of OoT that provides actor IDs, object mappings, and scene formats
- **[OoT Randomizer](https://github.com/OoTRandomizer/OoT-Randomizer)** — dungeon logic and connectivity data
- **[Fast64](https://github.com/HarbourMasters/fast64)** — Blender tools for OoT scene creation
- **libultraship** — the runtime framework that makes mod loading and resource management possible

## Status

This is an early experiment. Currently:
- ✅ LLM dungeon generation (local Gemma 4 or cloud Claude)
- ✅ In-game UI (Squadala panel)
- ✅ Portal system (entrance override)
- ✅ Hot-reload (.o2r at runtime)
- ✅ Dungeon persistence & list
- 🔧 Actor injection (pots work, enemies WIP)
- ⬜ Custom geometry generation
- ⬜ Streaming UI
- ⬜ Multiple connected rooms

## License

This fork inherits SoH's license. See [LICENSE](LICENSE) for details.
