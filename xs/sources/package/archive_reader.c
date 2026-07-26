/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "archive_internal.h"

#include <archive_entry.h>

#include <stdlib.h>
#include <string.h>

static XsPackageStatus read_entry_data(struct archive *reader, XsPackageError *error)
{
  unsigned char buffer[16384];
  for(;;)
  {
    const la_ssize_t count = archive_read_data(reader, buffer, sizeof(buffer));
    if(count == 0)
      return XS_PACKAGE_OK;
    if(count < 0)
      return xs_package_error(error, XS_PACKAGE_INVALID_ARCHIVE, archive_error_string(reader));
  }
}

XsPackageStatus xs_package_archive_verify(const char *archive_path, XsPackageArchiveInfo *info, XsPackageError *error)
{
  if(!xs_package_archive_name_valid(archive_path) || info == nullptr)
    return xs_package_error(error, XS_PACKAGE_INVALID_ARGUMENT, "package verifier arguments are invalid");

  struct archive *reader = archive_read_new();
  if(reader == nullptr)
    return xs_package_error(error, XS_PACKAGE_OUT_OF_MEMORY, "package archive reader allocation failed");
  archive_read_support_filter_zstd(reader);
  archive_read_support_format_tar(reader);
  if(archive_read_open_filename(reader, archive_path, 16384U) != ARCHIVE_OK)
  {
    const XsPackageStatus status = xs_package_error(error, XS_PACKAGE_INVALID_ARCHIVE, archive_error_string(reader));
    archive_read_free(reader);
    return status;
  }

  *info = (XsPackageArchiveInfo){0};
  XsPackageStatus status = XS_PACKAGE_OK;
  bool has_manifest = false;
  char *previous_path = nullptr;
  struct archive_entry *entry = nullptr;
  int header_status = ARCHIVE_OK;
  while(status == XS_PACKAGE_OK && (header_status = archive_read_next_header(reader, &entry)) == ARCHIVE_OK)
  {
    const char *path = archive_entry_pathname(entry);
    if(!xs_package_archive_path_valid(path) || archive_entry_filetype(entry) != AE_IFREG ||
       archive_entry_symlink(entry) != nullptr || archive_entry_hardlink(entry) != nullptr)
    {
      status = xs_package_error(error, XS_PACKAGE_INVALID_ARCHIVE, "package contains an unsafe entry");
      break;
    }
    if(previous_path != nullptr && strcmp(previous_path, path) >= 0)
    {
      status = xs_package_error(error, XS_PACKAGE_INVALID_ARCHIVE, "package entries are not uniquely sorted");
      break;
    }

    const la_int64_t signed_size = archive_entry_size(entry);
    if(signed_size < 0 || (uint64_t)signed_size > XS_PACKAGE_MAX_UNPACKED_SIZE - info->unpacked_size ||
       info->entry_count == XS_PACKAGE_MAX_ENTRIES)
    {
      status = xs_package_error(error, XS_PACKAGE_SIZE_LIMIT, "package archive limit exceeded");
      break;
    }

    const size_t path_length = strlen(path);
    char *next_path = malloc(path_length + 1U);
    if(next_path == nullptr)
    {
      status = xs_package_error(error, XS_PACKAGE_OUT_OF_MEMORY, "package path allocation failed");
      break;
    }
    memcpy(next_path, path, path_length + 1U);
    free(previous_path);
    previous_path = next_path;
    has_manifest = has_manifest || strcmp(path, XS_PACKAGE_MANIFEST_PATH) == 0;
    info->unpacked_size += (uint64_t)signed_size;
    ++info->entry_count;
    status = read_entry_data(reader, error);
  }

  if(status == XS_PACKAGE_OK && header_status != ARCHIVE_EOF)
    status = xs_package_error(error, XS_PACKAGE_INVALID_ARCHIVE, archive_error_string(reader));
  if(status == XS_PACKAGE_OK && archive_filter_code(reader, 0) != ARCHIVE_FILTER_ZSTD)
    status = xs_package_error(error, XS_PACKAGE_INVALID_ARCHIVE, "package compression must be Zstandard");
  if(status == XS_PACKAGE_OK && (archive_format(reader) & ARCHIVE_FORMAT_BASE_MASK) != ARCHIVE_FORMAT_TAR)
    status = xs_package_error(error, XS_PACKAGE_INVALID_ARCHIVE, "package container must be tar");
  if(status == XS_PACKAGE_OK && !has_manifest)
    status = xs_package_error(error, XS_PACKAGE_MANIFEST_MISSING, "package archive requires xspkg.json");

  free(previous_path);
  if(archive_read_close(reader) != ARCHIVE_OK && status == XS_PACKAGE_OK)
    status = xs_package_error(error, XS_PACKAGE_INVALID_ARCHIVE, archive_error_string(reader));
  archive_read_free(reader);
  if(status == XS_PACKAGE_OK)
    status = xs_package_archive_digest(archive_path, info, error);
  return status;
}
