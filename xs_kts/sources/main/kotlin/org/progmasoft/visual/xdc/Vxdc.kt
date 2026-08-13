/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xdc

import java.io.File
import java.nio.file.Path
import kotlin.system.exitProcess
import org.progmasoft.visual.xsharp.project.ProjectConfigurationException
import org.progmasoft.visual.xsharp.project.ProjectLockFile
import org.progmasoft.visual.xsharp.project.discoverProject
import org.progmasoft.visual.xsharp.project.evaluateWithKotlin
import org.progmasoft.visual.xsharp.project.requireSupportedJava

private const val VXDC_USAGE =
  "usage: vxdc -Projectfile Visual.XSharp.kts -Output <name>.sqlite3.dump"

internal data class VxdcCommand(
  val projectFile: File,
  val output: Path,
)

internal fun parseVxdcCommand(args: Array<String>): VxdcCommand {
  var projectFile: File? = null
  var output: Path? = null
  var index = 0
  while (index < args.size) {
    val option = args[index]
    if (option != "-Projectfile" && option != "-Output") {
      throw ProjectConfigurationException("unknown VXDC option '$option'\n$VXDC_USAGE")
    }
    if (index + 1 >= args.size) {
      throw ProjectConfigurationException("missing value for VXDC option '$option'\n$VXDC_USAGE")
    }
    val value = args[index + 1]
    when (option) {
      "-Projectfile" -> {
        if (projectFile != null) {
          throw ProjectConfigurationException("duplicate VXDC option '-Projectfile'")
        }
        projectFile = File(value)
      }
      "-Output" -> {
        if (output != null) throw ProjectConfigurationException("duplicate VXDC option '-Output'")
        output = Path.of(value)
      }
    }
    index += 2
  }

  val project =
    projectFile
      ?: throw ProjectConfigurationException(
        "missing required VXDC option '-Projectfile'\n$VXDC_USAGE"
      )
  val destination =
    output
      ?: throw ProjectConfigurationException("missing required VXDC option '-Output'\n$VXDC_USAGE")
  if (project.name != "Visual.XSharp.kts" || !project.isFile) {
    throw ProjectConfigurationException("-Projectfile must name an existing Visual.XSharp.kts file")
  }
  if (!destination.fileName.toString().endsWith(".sqlite3.dump")) {
    throw ProjectConfigurationException("-Output must end with '.sqlite3.dump'")
  }
  return VxdcCommand(project.canonicalFile, destination)
}

fun main(args: Array<String>) {
  try {
    requireSupportedJava()
    val command = parseVxdcCommand(args)
    val project = discoverProject(command.projectFile)
    val status = evaluateWithKotlin(command.projectFile, "resolve")
    if (status != 0) exitProcess(status)
    LockfileDump.write(project.root.toPath().resolve(ProjectLockFile.FILE_NAME), command.output)
    println("vxdc: wrote '${command.output.toAbsolutePath().normalize()}'")
  } catch (error: ProjectConfigurationException) {
    System.err.println("vxdc: ${error.message}")
    exitProcess(1)
  } catch (error: java.sql.SQLException) {
    System.err.println("vxdc: SQLite dump failed: ${error.message}")
    exitProcess(1)
  }
}
