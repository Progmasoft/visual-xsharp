/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

plugins {
  `java-library`
  `maven-publish`
}

group = "org.xsslang"
version = "2026.1"

repositories {
  mavenCentral()
}

dependencies {
  testImplementation("org.junit.jupiter:junit-jupiter:5.13.4")
  testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

java {
  toolchain {
    languageVersion = JavaLanguageVersion.of(25)
  }
  withSourcesJar()
  withJavadocJar()
}

sourceSets {
  main {
    java.srcDir("sources/main/java")
  }
  test {
    java.srcDir("sources/test/java")
  }
}

tasks.withType<JavaCompile>().configureEach {
  options.release = 25
  options.compilerArgs.addAll(listOf("-Xlint:all,-restricted", "-Werror"))
  options.encoding = "UTF-8"
}

tasks.withType<Javadoc>().configureEach {
  (options as StandardJavadocDocletOptions).addBooleanOption("Werror", true)
  (options as StandardJavadocDocletOptions).addBooleanOption("Xdoclint:all,-missing", true)
  options.encoding = "UTF-8"
}

tasks.test {
  useJUnitPlatform()
  maxHeapSize = "384m"
  jvmArgs(
      "--enable-native-access=ALL-UNNAMED",
      "-Dorg.xsslang.xlil.library=${rootProject.projectDir.resolve("../build/clang-debug/projects/xs/libxs_lil.so")}")
}

publishing {
  repositories {
    maven {
      name = "repository"
      url = uri(layout.buildDirectory.dir("repository"))
    }
  }
  publications {
    create<MavenPublication>("xlil") {
      from(components["java"])
      pom {
        name = "XLIL Java"
        description = "Java 25 FFM reader and writer bindings for the XLIL C ABI"
        url = "https://xsharp-lang.xyz/"
        licenses {
          license {
            name = "Mozilla Public License 2.0"
            url = "https://www.mozilla.org/MPL/2.0/"
            distribution = "repo"
          }
        }
        developers {
          developer {
            id = "leitwolf"
            name = "Leitwolf"
          }
        }
        scm {
          connection = "scm:git:https://github.com/xss-lang/xs-project.git"
          developerConnection = "scm:git:ssh://git@github.com/xss-lang/xs-project.git"
          url = "https://github.com/xss-lang/xs-project"
        }
      }
    }
  }
}
