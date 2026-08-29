# SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
# SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0

# These targets are not part of the renewed compiler graph. Keeping the list
# explicit makes every remaining legacy island visible and independently
# removable.
add_subdirectory(xlil)
add_subdirectory(lil)
add_subdirectory(backend)
