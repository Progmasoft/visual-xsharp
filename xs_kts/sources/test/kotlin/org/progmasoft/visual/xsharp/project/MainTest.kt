/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xsharp.project

import java.nio.file.Files
import kotlin.test.Test
import kotlin.test.assertEquals

class MainTest {
  @Test
  fun honorsExplicitKotlinRunner() {
    assertEquals(
      "D:/tools/kotlin-custom.bat",
      kotlinCommand(mapOf("XS_KOTLIN" to "D:/tools/kotlin-custom.bat"), "Windows 11"),
    )
  }

  @Test
  fun resolvesWindowsBatchRunnerFromPath() {
    val root = Files.createTempDirectory("xs-kotlin-runner-")
    try {
      val runner = Files.createFile(root.resolve("kotlin.bat"))
      assertEquals(
        runner.toFile().absolutePath,
        kotlinCommand(mapOf("Path" to root.toString()), "Windows Server 2025"),
      )
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun keepsNativeCommandOnNonWindowsHosts() {
    assertEquals("kotlin", kotlinCommand(mapOf("PATH" to "/opt/kotlin/bin"), "Linux"))
  }
}
