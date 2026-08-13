/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xsharp.project

import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json

/** Encodes the versioned project plan consumed by the native project driver. */
object PlanWriter {
  private val encoder = Json { encodeDefaults = true }

  fun write(plan: ProjectPlan): String = encoder.encodeToString(ProjectPlanDocument.from(plan))
}
