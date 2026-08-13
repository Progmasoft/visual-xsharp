/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

import org.gradle.api.JavaVersion
import org.gradle.jvm.application.tasks.CreateStartScripts
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
  kotlin("jvm") version "2.4.0"
  kotlin("plugin.serialization") version "2.4.0"
  id("com.diffplug.spotless") version "8.9.0"
  application
}

group = "org.progmasoft.visual.xsharp"
version = "0.3.1"

repositories {
  mavenCentral()
}

dependencies {
  implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.11.0")
  implementation("org.xerial:sqlite-jdbc:3.53.1.0")
  testImplementation(kotlin("test"))
}

kotlin {
  jvmToolchain(25)
  compilerOptions {
    jvmTarget = JvmTarget.JVM_25
    allWarningsAsErrors = true
  }
}

java {
  sourceCompatibility = JavaVersion.VERSION_25
  targetCompatibility = JavaVersion.VERSION_25
}

application {
  mainClass = "org.progmasoft.visual.xsharp.project.MainKt"
  applicationName = "xs-project-runtime"
  applicationDefaultJvmArgs = listOf("--enable-native-access=ALL-UNNAMED")
}

// VXDC shares the project evaluator and SQLite driver with the runtime, but it is
// intentionally exposed as a separate command. Keeping both launchers in one
// distribution avoids shipping two copies of the Kotlin runtime and JDBC driver.
val vxdcStartScripts =
  tasks.register<CreateStartScripts>("vxdcStartScripts") {
    applicationName = "vxdc"
    mainClass = "org.progmasoft.visual.xdc.VxdcKt"
    defaultJvmOpts = application.applicationDefaultJvmArgs
    outputDir =
      layout.buildDirectory
        .dir("vxdc-start-scripts")
        .get()
        .asFile
    classpath = files(tasks.named("jar"), configurations.runtimeClasspath)
  }

distributions {
  main {
    contents {
      from(vxdcStartScripts) { into("bin") }
    }
  }
}

tasks.named("installDist") { dependsOn(vxdcStartScripts) }

sourceSets {
  main { kotlin.srcDir("sources/main/kotlin") }
  test { kotlin.srcDir("sources/test/kotlin") }
}

spotless {
  kotlin {
    target("sources/**/*.kt")
    ktfmt().googleStyle()
    trimTrailingWhitespace()
    endWithNewline()
  }
  kotlinGradle {
    target("*.gradle.kts")
    ktlint()
    trimTrailingWhitespace()
    endWithNewline()
  }
}

tasks.test {
  useJUnitPlatform()
  maxHeapSize = "512m"
  jvmArgs("--enable-native-access=ALL-UNNAMED")
}
