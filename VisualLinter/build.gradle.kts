/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

import org.gradle.api.JavaVersion
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    kotlin("jvm") version "2.4.0"
    id("com.diffplug.spotless") version "8.9.0"
}

group = "org.progmasoft.visual.linter"
version = "0.1.0"

repositories { mavenCentral() }

dependencies { testImplementation(kotlin("test")) }

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

tasks.test { useJUnitPlatform() }
