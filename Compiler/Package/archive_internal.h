/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

#ifndef XS_PACKAGE_ARCHIVE_INTERNAL_H
#define XS_PACKAGE_ARCHIVE_INTERNAL_H

#include <Visual/XSharp/package.h>
#include <archive.h>

XsPackageStatus
xs_package_error(XsPackageError *error, XsPackageStatus status, const char *message);
XsPackageStatus
xs_package_archive_digest(const char *path, XsPackageArchiveInfo *info, XsPackageError *error);
bool
xs_package_archive_path_valid(const char *path);
bool
xs_package_archive_name_valid(const char *path);

#endif
