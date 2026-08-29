# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

# Relocatable LLVM development archives can retain the DIA SDK path from the
# machine that produced them. Static C++ component linkage reaches that target,
# unlike LLVM-C, so repair only a missing exported diaguids path by discovering
# the SDK library installed on the current Windows host.
function(xs_repair_llvm_dia_dependency)
  if(NOT WIN32 OR NOT TARGET LLVMDebugInfoPDB)
    return()
  endif()

  get_target_property(llvm_pdb_links LLVMDebugInfoPDB INTERFACE_LINK_LIBRARIES)
  set(repair_required FALSE)
  foreach(link_item IN LISTS llvm_pdb_links)
    if(link_item MATCHES "[/\\\\]diaguids[.]lib$" AND IS_ABSOLUTE "${link_item}" AND NOT EXISTS "${link_item}")
      set(repair_required TRUE)
    endif()
  endforeach()
  if(NOT repair_required)
    return()
  endif()

  set(dia_hints)
  if(DEFINED ENV{VSINSTALLDIR})
    list(APPEND dia_hints "$ENV{VSINSTALLDIR}/DIA SDK/lib/amd64")
  endif()
  file(GLOB visual_studio_installations LIST_DIRECTORIES TRUE
    "$ENV{ProgramFiles}/Microsoft Visual Studio/*/*"
  )
  foreach(installation IN LISTS visual_studio_installations)
    list(APPEND dia_hints "${installation}/DIA SDK/lib/amd64")
  endforeach()
  find_library(XS_LLVM_DIA_GUIDS_LIBRARY NAMES diaguids HINTS ${dia_hints} NO_DEFAULT_PATH)
  if(NOT XS_LLVM_DIA_GUIDS_LIBRARY)
    message(FATAL_ERROR
      "LLVM C++ libraries require diaguids.lib, but the relocated LLVM package points to a missing DIA SDK path."
    )
  endif()

  set(repaired_links)
  foreach(link_item IN LISTS llvm_pdb_links)
    if(link_item MATCHES "[/\\\\]diaguids[.]lib$" AND IS_ABSOLUTE "${link_item}" AND NOT EXISTS "${link_item}")
      list(APPEND repaired_links "${XS_LLVM_DIA_GUIDS_LIBRARY}")
    else()
      list(APPEND repaired_links "${link_item}")
    endif()
  endforeach()
  set_property(TARGET LLVMDebugInfoPDB PROPERTY INTERFACE_LINK_LIBRARIES "${repaired_links}")
endfunction()

function(xs_resolve_llvm_cpp_libraries output_variable)
  xs_repair_llvm_dia_dependency()
  llvm_map_components_to_libnames(llvm_cpp_libraries
    core bitwriter passes support targetparser nativecodegen
  )
  set(${output_variable} ${llvm_cpp_libraries} PARENT_SCOPE)
endfunction()
