/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "archive_internal.h"

#include <archive_entry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int compare_inputs(const void *left_pointer, const void *right_pointer)
{
  const XsPackageInput *const *left = left_pointer;
  const XsPackageInput *const *right = right_pointer;
  return strcmp((*left)->archive_path, (*right)->archive_path);
}

static XsPackageStatus write_file_data(struct archive *writer, const char *source_path, XsPackageError *error)
{
  FILE *source = fopen(source_path, "rb");
  if(source == nullptr)
    return xs_package_error(error, XS_PACKAGE_IO_ERROR, "package input cannot be opened");

  XsPackageStatus status = XS_PACKAGE_OK;
  unsigned char buffer[16384];
  for(;;)
  {
    const size_t count = fread(buffer, 1U, sizeof(buffer), source);
    if(count != 0U && archive_write_data(writer, buffer, count) != (la_ssize_t)count)
    {
      status = xs_package_error(error, XS_PACKAGE_IO_ERROR, archive_error_string(writer));
      break;
    }
    if(count != sizeof(buffer))
    {
      if(ferror(source) != 0)
        status = xs_package_error(error, XS_PACKAGE_IO_ERROR, "package input read failed");
      break;
    }
  }

  if(fclose(source) != 0 && status == XS_PACKAGE_OK)
    status = xs_package_error(error, XS_PACKAGE_IO_ERROR, "package input close failed");
  return status;
}

static XsPackageStatus write_entry(struct archive *writer, const XsPackageInput *input, uint64_t *unpacked_size,
                                   XsPackageError *error)
{
  struct stat metadata = {0};
  if(stat(input->source_path, &metadata) != 0 || !S_ISREG(metadata.st_mode) || metadata.st_size < 0)
    return xs_package_error(error, XS_PACKAGE_IO_ERROR, "package input must be a regular file");

  const uint64_t size = (uint64_t)metadata.st_size;
  if(size > XS_PACKAGE_MAX_UNPACKED_SIZE - *unpacked_size)
    return xs_package_error(error, XS_PACKAGE_SIZE_LIMIT, "package unpacked size limit exceeded");

  struct archive_entry *entry = archive_entry_new();
  if(entry == nullptr)
    return xs_package_error(error, XS_PACKAGE_OUT_OF_MEMORY, "package archive entry allocation failed");
  archive_entry_set_pathname(entry, input->archive_path);
  archive_entry_set_filetype(entry, AE_IFREG);
  archive_entry_set_perm(entry, 0644);
  archive_entry_set_size(entry, metadata.st_size);
  archive_entry_set_mtime(entry, 0, 0);
  archive_entry_set_uid(entry, 0);
  archive_entry_set_gid(entry, 0);
  archive_entry_set_uname(entry, "");
  archive_entry_set_gname(entry, "");

  XsPackageStatus status = XS_PACKAGE_OK;
  if(archive_write_header(writer, entry) != ARCHIVE_OK)
    status = xs_package_error(error, XS_PACKAGE_IO_ERROR, archive_error_string(writer));
  else
    status = write_file_data(writer, input->source_path, error);
  archive_entry_free(entry);

  if(status == XS_PACKAGE_OK)
    *unpacked_size += size;
  return status;
}

XsPackageStatus xs_package_archive_write(const char *output_path, const XsPackageInput *inputs, size_t input_count,
                                         XsPackageArchiveInfo *info, XsPackageError *error)
{
  if(!xs_package_archive_name_valid(output_path) || inputs == nullptr || info == nullptr || input_count == 0U ||
     input_count > XS_PACKAGE_MAX_ENTRIES)
    return xs_package_error(error, XS_PACKAGE_INVALID_ARGUMENT, "package writer arguments are invalid");

  const XsPackageInput **ordered = malloc(input_count * sizeof(*ordered));
  if(ordered == nullptr)
    return xs_package_error(error, XS_PACKAGE_OUT_OF_MEMORY, "package input ordering allocation failed");

  bool has_manifest = false;
  for(size_t index = 0; index < input_count; ++index)
  {
    if(!xs_package_archive_path_valid(inputs[index].archive_path) || inputs[index].source_path == nullptr)
    {
      free(ordered);
      return xs_package_error(error, XS_PACKAGE_INVALID_PATH, "package archive path is unsafe");
    }
    has_manifest = has_manifest || strcmp(inputs[index].archive_path, XS_PACKAGE_MANIFEST_PATH) == 0;
    ordered[index] = &inputs[index];
  }
  qsort(ordered, input_count, sizeof(*ordered), compare_inputs);
  for(size_t index = 1; index < input_count; ++index)
  {
    if(strcmp(ordered[index - 1U]->archive_path, ordered[index]->archive_path) == 0)
    {
      free(ordered);
      return xs_package_error(error, XS_PACKAGE_INVALID_PATH, "package archive paths must be unique");
    }
  }
  if(!has_manifest)
  {
    free(ordered);
    return xs_package_error(error, XS_PACKAGE_MANIFEST_MISSING, "package archive requires xspkg.json");
  }

  struct archive *writer = archive_write_new();
  if(writer == nullptr)
  {
    free(ordered);
    return xs_package_error(error, XS_PACKAGE_OUT_OF_MEMORY, "package archive writer allocation failed");
  }

  XsPackageStatus status = XS_PACKAGE_OK;
  if(archive_write_add_filter_zstd(writer) != ARCHIVE_OK ||
     archive_write_set_format_pax_restricted(writer) != ARCHIVE_OK ||
     archive_write_open_filename(writer, output_path) != ARCHIVE_OK)
    status = xs_package_error(error, XS_PACKAGE_IO_ERROR, archive_error_string(writer));

  *info = (XsPackageArchiveInfo){0};
  for(size_t index = 0; status == XS_PACKAGE_OK && index < input_count; ++index)
    status = write_entry(writer, ordered[index], &info->unpacked_size, error);

  if(archive_write_close(writer) != ARCHIVE_OK && status == XS_PACKAGE_OK)
    status = xs_package_error(error, XS_PACKAGE_IO_ERROR, archive_error_string(writer));
  if(archive_write_free(writer) != ARCHIVE_OK && status == XS_PACKAGE_OK)
    status = xs_package_error(error, XS_PACKAGE_IO_ERROR, "package archive writer cleanup failed");
  free(ordered);

  if(status != XS_PACKAGE_OK)
  {
    (void)remove(output_path);
    return status;
  }
  info->entry_count = (uint32_t)input_count;
  return xs_package_archive_digest(output_path, info, error);
}
