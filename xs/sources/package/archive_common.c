/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

#include "archive_internal.h"

#include <openssl/evp.h>

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

XsPackageStatus xs_package_error(XsPackageError *error, XsPackageStatus status, const char *message)
{
  if(error != nullptr)
  {
    error->status = status;
    (void)snprintf(error->message, sizeof(error->message), "%s",
                   message == nullptr ? "package operation failed" : message);
  }
  return status;
}

bool xs_package_archive_name_valid(const char *path)
{
  if(path == nullptr)
    return false;
  const size_t path_length = strlen(path);
  const size_t suffix_length = sizeof(XS_PACKAGE_ARCHIVE_SUFFIX) - 1U;
  return path_length > suffix_length && strcmp(path + path_length - suffix_length, XS_PACKAGE_ARCHIVE_SUFFIX) == 0;
}

bool xs_package_archive_path_valid(const char *path)
{
  if(path == nullptr || path[0] == '\0' || path[0] == '/' || strchr(path, '\\') != nullptr)
    return false;

  const char *segment = path;
  for(const char *cursor = path;; ++cursor)
  {
    if(*cursor != '/' && *cursor != '\0')
      continue;
    const size_t length = (size_t)(cursor - segment);
    if(length == 0U || (length == 1U && segment[0] == '.') || (length == 2U && segment[0] == '.' && segment[1] == '.'))
      return false;
    if(*cursor == '\0')
      return true;
    segment = cursor + 1;
  }
}

XsPackageStatus xs_package_archive_digest(const char *path, XsPackageArchiveInfo *info, XsPackageError *error)
{
  FILE *stream = fopen(path, "rb");
  if(stream == nullptr)
    return xs_package_error(error, XS_PACKAGE_IO_ERROR, "package artifact cannot be opened for hashing");

  EVP_MD_CTX *context = EVP_MD_CTX_new();
  if(context == nullptr)
  {
    (void)fclose(stream);
    return xs_package_error(error, XS_PACKAGE_OUT_OF_MEMORY, "SHA-256 context allocation failed");
  }

  XsPackageStatus status = XS_PACKAGE_OK;
  if(EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1)
    status = xs_package_error(error, XS_PACKAGE_IO_ERROR, "SHA-256 initialization failed");

  unsigned char buffer[16384];
  while(status == XS_PACKAGE_OK)
  {
    const size_t count = fread(buffer, 1U, sizeof(buffer), stream);
    if(count != 0U && EVP_DigestUpdate(context, buffer, count) != 1)
      status = xs_package_error(error, XS_PACKAGE_IO_ERROR, "SHA-256 update failed");
    if(count != sizeof(buffer))
    {
      if(ferror(stream) != 0)
        status = xs_package_error(error, XS_PACKAGE_IO_ERROR, "package artifact read failed");
      break;
    }
  }

  unsigned int digest_size = 0;
  if(status == XS_PACKAGE_OK &&
     (EVP_DigestFinal_ex(context, info->sha256, &digest_size) != 1 || digest_size != XS_PACKAGE_SHA256_SIZE))
    status = xs_package_error(error, XS_PACKAGE_IO_ERROR, "SHA-256 finalization failed");

  EVP_MD_CTX_free(context);
  if(fclose(stream) != 0 && status == XS_PACKAGE_OK)
    status = xs_package_error(error, XS_PACKAGE_IO_ERROR, "package artifact close failed");

  struct stat metadata = {0};
  if(status == XS_PACKAGE_OK && (stat(path, &metadata) != 0 || metadata.st_size < 0))
    status = xs_package_error(error, XS_PACKAGE_IO_ERROR, "package artifact size cannot be read");
  if(status == XS_PACKAGE_OK)
    info->compressed_size = (uint64_t)metadata.st_size;
  return status;
}

void xs_package_sha256_hex(const uint8_t digest[XS_PACKAGE_SHA256_SIZE], char output[XS_PACKAGE_SHA256_HEX_SIZE])
{
  static const char digits[] = "0123456789abcdef";
  if(digest == nullptr || output == nullptr)
    return;
  for(size_t index = 0; index < XS_PACKAGE_SHA256_SIZE; ++index)
  {
    output[index * 2U] = digits[digest[index] >> 4U];
    output[index * 2U + 1U] = digits[digest[index] & 0x0fU];
  }
  output[XS_PACKAGE_SHA256_HEX_SIZE - 1U] = '\0';
}
