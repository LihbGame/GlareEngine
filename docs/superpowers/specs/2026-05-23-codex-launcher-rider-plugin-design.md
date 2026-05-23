# Codex Launcher Rider Plugin - Design Spec

## Goal

A lightweight JetBrains Rider toolbar button that opens the built-in terminal and launches OpenAI Codex CLI in the project root directory.

## Scope

- Single toolbar action, no settings UI
- Uses Rider's built-in terminal through the Terminal plugin API
- Works with Rider 2024.1+

## Architecture

```text
codex-launcher-rider-plugin/
|-- build.gradle.kts
|-- gradle.properties
|-- src/main/
|   |-- resources/
|   |   `-- META-INF/
|   |       `-- plugin.xml
|   `-- kotlin/
|       `-- com/codexlauncher/
|           `-- LaunchCodexAction.kt
```

## Components

### LaunchCodexAction.kt

Single `AnAction` subclass:

1. `actionPerformed(e)` core logic:
   - Get `project` from `e.getData(CommonDataKeys.PROJECT)`.
   - Get project root path from `project.basePath`.
   - Call `TerminalToolWindowManager.getInstance(project).createLocalTerminalWidget("Codex", ...)`.
   - Execute command via terminal widget: `cd <basePath> && codex`.
2. `update(e)` enables the action only when a project is open.

### plugin.xml

- Register `LaunchCodexAction` as an action with id `CodexLauncher.Launch`.
- Add it to the `MainToolBar` group.
- Use `AllIcons.Actions.Execute` as default icon.
- Declare dependency on `org.jetbrains.plugins.terminal`.

### build.gradle.kts

- Use `org.jetbrains.intellij.platform` Gradle plugin v2.x.
- Target Rider product (`intellijIdeaUltimate` or `rider` via custom repo).
- Set Kotlin JVM target to 17.
- Set plugin since-build to 241 (2024.1).

## Behavior

- User clicks toolbar button.
- A new terminal tab titled "Codex" opens in Rider's Terminal tool window.
- Terminal auto-executes `cd <projectRoot> && codex`.
- If the terminal API is unavailable, show a balloon notification.

## Out of Scope

- Settings/configuration UI
- Multiple AI tool presets
- Custom keybinding, which the user can bind through Rider's Keymap settings
- External terminal window
