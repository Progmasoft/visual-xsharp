/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

import org.gradle.api.JavaVersion
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
  kotlin("jvm") version "2.4.0"
  id("com.diffplug.spotless") version "8.9.0"
  application
}

group = "org.progmasoft.visual.xsharp"
version = "0.3.0"

repositories {
  mavenCentral()
}

dependencies {
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
}

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
