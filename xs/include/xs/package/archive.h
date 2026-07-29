/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef XS_PACKAGE_ARCHIVE_H
#define XS_PACKAGE_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

#define XS_PACKAGE_ARCHIVE_SUFFIX ".xspkg.tar.zst"
#define XS_PACKAGE_MANIFEST_PATH "xspkg.json"
#define XS_PACKAGE_SHA256_SIZE 32U
#define XS_PACKAGE_SHA256_HEX_SIZE 65U
#define XS_PACKAGE_MAX_ENTRIES 100000U
#define XS_PACKAGE_MAX_UNPACKED_SIZE UINT64_C(2147483648)

typedef enum XsPackageStatus
{
  XS_PACKAGE_OK = 0,
  XS_PACKAGE_INVALID_ARGUMENT = 1,
  XS_PACKAGE_INVALID_PATH = 2,
  XS_PACKAGE_INVALID_ARCHIVE = 3,
  XS_PACKAGE_MANIFEST_MISSING = 4,
  XS_PACKAGE_IO_ERROR = 5,
  XS_PACKAGE_SIZE_LIMIT = 6,
  XS_PACKAGE_OUT_OF_MEMORY = 7
} XsPackageStatus;

typedef struct XsPackageError
{
  XsPackageStatus status;
  char message[256];
} XsPackageError;

typedef struct XsPackageInput
{
  const char *archive_path;
  const char *source_path;
} XsPackageInput;

typedef struct XsPackageArchiveInfo
{
  uint8_t sha256[XS_PACKAGE_SHA256_SIZE];
  uint64_t compressed_size;
  uint64_t unpacked_size;
  uint32_t entry_count;
} XsPackageArchiveInfo;

XsPackageStatus xs_package_archive_write(const char *output_path, const XsPackageInput *inputs, size_t input_count,
                                         XsPackageArchiveInfo *info, XsPackageError *error);
XsPackageStatus xs_package_archive_verify(const char *archive_path, XsPackageArchiveInfo *info, XsPackageError *error);
void xs_package_sha256_hex(const uint8_t digest[XS_PACKAGE_SHA256_SIZE], char output[XS_PACKAGE_SHA256_HEX_SIZE]);

#endif
