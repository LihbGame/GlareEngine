# Codex Launcher Rider Plugin - Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a JetBrains Rider toolbar button that opens a built-in terminal tab and launches OpenAI Codex CLI in the project root.

**Architecture:** Single IntelliJ Platform plugin with one `AnAction` class. Clicking the toolbar button creates a new terminal tab via `TerminalToolWindowManager`, then executes `cd <projectRoot> && codex` through `ShellTerminalWidget.executeCommand()`.

**Tech Stack:** Kotlin, IntelliJ Platform Gradle Plugin 2.x, Rider 2024.1+

## File Structure

```text
C:/Users/22528/RiderProjects/codex-launcher-rider-plugin/
|-- build.gradle.kts
|-- settings.gradle.kts
|-- gradle.properties
|-- src/main/
|   |-- resources/
|   |   `-- META-INF/
|   |       `-- plugin.xml
|   `-- kotlin/
|       `-- com/codexlauncher/
|           `-- LaunchCodexAction.kt
`-- gradle/
    `-- wrapper/
        `-- gradle-wrapper.properties
```

## Task 1: Scaffold the Gradle project

**Files:**
- Create: `C:/Users/22528/RiderProjects/codex-launcher-rider-plugin/settings.gradle.kts`
- Create: `C:/Users/22528/RiderProjects/codex-launcher-rider-plugin/gradle.properties`
- Create: `C:/Users/22528/RiderProjects/codex-launcher-rider-plugin/build.gradle.kts`
- Create: `C:/Users/22528/RiderProjects/codex-launcher-rider-plugin/src/main/resources/META-INF/plugin.xml`

- [ ] **Step 1: Create project root directory**

```bash
mkdir -p "C:/Users/22528/RiderProjects/codex-launcher-rider-plugin/src/main/kotlin/com/codexlauncher"
mkdir -p "C:/Users/22528/RiderProjects/codex-launcher-rider-plugin/src/main/resources/META-INF"
```

- [ ] **Step 2: Create settings.gradle.kts**

```kotlin
rootProject.name = "codex-launcher-rider-plugin"
```

- [ ] **Step 3: Create gradle.properties**

```properties
kotlin.code.style=official
org.gradle.jvmargs=-Xmx2g -XX:MaxMetaspaceSize=512m
```

- [ ] **Step 4: Create build.gradle.kts**

```kotlin
plugins {
    id("java")
    id("org.jetbrains.kotlin.jvm") version "1.9.25"
    id("org.jetbrains.intellij.platform") version "2.11.0"
}

group = "com.codexlauncher"
version = "1.0.0"

repositories {
    mavenCentral()
    intellijPlatform {
        defaultRepositories()
    }
}

dependencies {
    intellijPlatform {
        rider("2024.1")
        bundledPlugin("org.jetbrains.plugins.terminal")
        instrumentationTools()
    }
}

intellijPlatform {
    pluginConfiguration {
        id = "com.codexlauncher.rider-plugin"
        name = "Codex Launcher"
        description = "Launches OpenAI Codex CLI in Rider's built-in terminal."
        vendor {
            name = "CodexLauncher"
        }
        ideaVersion {
            sinceBuild = "241"
            untilBuild = "252.*"
        }
        changeNotes = "Initial version."
    }
}

tasks {
    withType<JavaCompile> {
        sourceCompatibility = "17"
        targetCompatibility = "17"
    }
    withType<org.jetbrains.kotlin.gradle.tasks.KotlinCompile> {
        kotlinOptions.jvmTarget = "17"
    }
}
```

- [ ] **Step 5: Create plugin.xml**

```xml
<idea-plugin>
    <id>com.codexlauncher.rider-plugin</id>
    <name>Codex Launcher</name>
    <vendor>CodexLauncher</vendor>
    <description>Launches OpenAI Codex CLI in Rider's built-in terminal.</description>

    <depends>com.intellij.modules.platform</depends>
    <depends>org.jetbrains.plugins.terminal</depends>

    <actions>
        <action id="CodexLauncher.Launch"
                class="com.codexlauncher.LaunchCodexAction"
                text="Launch Codex"
                description="Open terminal and launch Codex CLI">
            <add-to-group group-id="MainToolBar" anchor="last"/>
        </action>
    </actions>
</idea-plugin>
```

- [ ] **Step 6: Initialize Gradle wrapper**

```bash
cd "C:/Users/22528/RiderProjects/codex-launcher-rider-plugin" && gradle wrapper --gradle-version 8.11
```

- [ ] **Step 7: Verify project compiles**

```bash
cd "C:/Users/22528/RiderProjects/codex-launcher-rider-plugin" && ./gradlew buildPlugin
```

Expected: BUILD SUCCESSFUL. If the action class is missing at this stage, the failure is expected and will be fixed in Task 2.

- [ ] **Step 8: Commit scaffold**

```bash
cd "C:/Users/22528/RiderProjects/codex-launcher-rider-plugin" && git init && git add -A && git commit -m "feat: scaffold Gradle project with IntelliJ Platform plugin"
```

## Task 2: Implement LaunchCodexAction

**Files:**
- Create: `C:/Users/22528/RiderProjects/codex-launcher-rider-plugin/src/main/kotlin/com/codexlauncher/LaunchCodexAction.kt`

- [ ] **Step 1: Create LaunchCodexAction.kt**

```kotlin
package com.codexlauncher

import com.intellij.notification.NotificationGroupManager
import com.intellij.notification.NotificationType
import com.intellij.openapi.actionSystem.AnAction
import com.intellij.openapi.actionSystem.AnActionEvent
import com.intellij.openapi.actionSystem.CommonDataKeys
import com.intellij.openapi.project.Project
import org.jetbrains.plugins.terminal.TerminalToolWindowManager
import org.jetbrains.plugins.terminal.ShellTerminalWidget

class LaunchCodexAction : AnAction() {

    override fun actionPerformed(e: AnActionEvent) {
        val project = e.getData(CommonDataKeys.PROJECT) ?: return
        val basePath = project.basePath ?: return

        val terminalManager = TerminalToolWindowManager.getInstance(project)

        terminalManager.toolWindow?.activate(null)
        val widget = terminalManager.createLocalTerminalWidget("Codex")
        val command = "cd \"$basePath\" && codex"

        if (widget is ShellTerminalWidget) {
            widget.executeCommand(command)
        } else {
            com.intellij.openapi.application.ApplicationManager.getApplication().invokeLater {
                terminalManager.terminalWidgets.firstOrNull()?.let { w ->
                    if (w is ShellTerminalWidget) {
                        w.executeCommand(command)
                    }
                }
            }
        }
    }

    override fun update(e: AnActionEvent) {
        val project = e.getData(CommonDataKeys.PROJECT)
        e.presentation.isEnabledAndVisible = project != null
    }

    companion object {
        private fun showNotification(project: Project, message: String) {
            NotificationGroupManager.getInstance()
                .getNotificationGroup("Codex Launcher")
                .createNotification(message, NotificationType.ERROR)
                .notify(project)
        }
    }
}
```

- [ ] **Step 2: Add notification group to plugin.xml**

Add inside `<idea-plugin>` in plugin.xml:

```xml
<extensions defaultExtensionNs="com.intellij">
    <notificationGroup id="Codex Launcher" displayType="BALLOON"/>
</extensions>
```

- [ ] **Step 3: Verify project compiles**

```bash
cd "C:/Users/22528/RiderProjects/codex-launcher-rider-plugin" && ./gradlew buildPlugin
```

Expected: BUILD SUCCESSFUL

- [ ] **Step 4: Commit**

```bash
cd "C:/Users/22528/RiderProjects/codex-launcher-rider-plugin" && git add -A && git commit -m "feat: implement LaunchCodexAction with terminal integration"
```

## Task 3: Add toolbar icon

**Files:**
- Create: `C:/Users/22528/RiderProjects/codex-launcher-rider-plugin/src/main/resources/icons/codex.svg`
- Modify: `src/main/resources/META-INF/plugin.xml`

- [ ] **Step 1: Create a simple Codex icon (SVG, 16x16)**

Create `src/main/resources/icons/codex.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 16 16">
  <rect width="16" height="16" rx="2" fill="#1a1a2e"/>
  <text x="3" y="12" font-family="monospace" font-size="11" font-weight="bold" fill="#00d4ff">C</text>
  <text x="9" y="12" font-family="monospace" font-size="11" font-weight="bold" fill="#7c3aed">&gt;</text>
</svg>
```

- [ ] **Step 2: Update plugin.xml to use the icon**

Change the action declaration in plugin.xml:

```xml
<action id="CodexLauncher.Launch"
        class="com.codexlauncher.LaunchCodexAction"
        text="Launch Codex"
        icon="/icons/codex.svg"
        description="Open terminal and launch Codex CLI">
    <add-to-group group-id="MainToolBar" anchor="last"/>
</action>
```

- [ ] **Step 3: Verify build**

```bash
cd "C:/Users/22528/RiderProjects/codex-launcher-rider-plugin" && ./gradlew buildPlugin
```

Expected: BUILD SUCCESSFUL

- [ ] **Step 4: Commit**

```bash
cd "C:/Users/22528/RiderProjects/codex-launcher-rider-plugin" && git add -A && git commit -m "feat: add toolbar icon for Codex Launcher"
```

## Task 4: Manual testing in Rider

- [ ] **Step 1: Build the plugin distribution**

```bash
cd "C:/Users/22528/RiderProjects/codex-launcher-rider-plugin" && ./gradlew buildPlugin
```

The plugin zip will be at `build/distributions/codex-launcher-rider-plugin-1.0.0.zip`.

- [ ] **Step 2: Install in Rider**

1. Open Rider -> Settings -> Plugins -> gear menu -> Install Plugin from Disk...
2. Select the zip file from Step 1.
3. Restart Rider.

- [ ] **Step 3: Verify behavior**

1. Open any project in Rider.
2. Look for the Codex icon on the main toolbar.
3. Click it. A new terminal tab titled "Codex" should open.
4. Terminal should auto-execute `cd <projectRoot> && codex`.
5. If `codex` is not installed, it should show "command not found", which is expected.

## Task 5: Fix compilation/runtime issues

This is a catch-all task for any issues found during Task 4.

- [ ] **Step 1: Fix any compilation errors found during build**

Re-run build and fix issues iteratively:

```bash
cd "C:/Users/22528/RiderProjects/codex-launcher-rider-plugin" && ./gradlew buildPlugin
```

- [ ] **Step 2: Fix any runtime issues found during manual testing**

Common issues to check:
- `TerminalToolWindowManager` or `ShellTerminalWidget` API mismatch: check exact method signatures.
- `ShellTerminalWidget` import path may differ across versions.
- Terminal tool window may need explicit activation before creating tab.
- `executeCommand` may need a delay if the shell has not initialized.

- [ ] **Step 3: Commit fixes**

```bash
cd "C:/Users/22528/RiderProjects/codex-launcher-rider-plugin" && git add -A && git commit -m "fix: resolve compilation and runtime issues"
```
