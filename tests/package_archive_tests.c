/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

#include <xs/package.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures = 0;

static void check(bool condition, const char *message)
{
  if(condition)
    return;
  fprintf(stderr, "check failed: %s\n", message);
  ++failures;
}

static bool write_text(const char *path, const char *text)
{
  FILE *stream = fopen(path, "wb");
  if(stream == nullptr)
    return false;
  const size_t length = strlen(text);
  const bool written = fwrite(text, 1U, length, stream) == length;
  return fclose(stream) == 0 && written;
}

static void join_path(char *output, size_t output_size, const char *directory, const char *name)
{
  (void)snprintf(output, output_size, "%s/%s", directory, name);
}

static void test_archive_roundtrip(const char *directory)
{
  char manifest_path[512];
  char source_path[512];
  char archive_path[512];
  join_path(manifest_path, sizeof(manifest_path), directory, "manifest.json");
  join_path(source_path, sizeof(source_path), directory, "main.xs");
  join_path(archive_path, sizeof(archive_path), directory, "Example-0.1.0.xspkg.tar.zst");
  check(write_text(manifest_path, "{\"format\":0,\"name\":\"Example\",\"version\":\"0.1.0\"}\n"),
        "manifest fixture is written");
  check(write_text(source_path, "fn main() -> Long { 0 }\n"), "source fixture is written");

  const XsPackageInput inputs[] = {
      {.archive_path = "Sources/main.xs", .source_path = source_path},
      {.archive_path = XS_PACKAGE_MANIFEST_PATH, .source_path = manifest_path},
  };
  XsPackageArchiveInfo written = {0};
  XsPackageError error = {0};
  check(xs_package_archive_write(archive_path, inputs, 2U, &written, &error) == XS_PACKAGE_OK, error.message);
  check(written.entry_count == 2U && written.unpacked_size != 0U && written.compressed_size != 0U,
        "writer reports package sizes");

  XsPackageArchiveInfo verified = {0};
  check(xs_package_archive_verify(archive_path, &verified, &error) == XS_PACKAGE_OK, error.message);
  check(verified.entry_count == written.entry_count && verified.unpacked_size == written.unpacked_size,
        "verifier preserves archive accounting");
  check(memcmp(verified.sha256, written.sha256, XS_PACKAGE_SHA256_SIZE) == 0,
        "writer and verifier agree on compressed artifact digest");

  char digest[XS_PACKAGE_SHA256_HEX_SIZE] = {0};
  xs_package_sha256_hex(verified.sha256, digest);
  check(strlen(digest) == XS_PACKAGE_SHA256_HEX_SIZE - 1U, "SHA-256 renders as lowercase hexadecimal");
  (void)remove(archive_path);
  (void)remove(source_path);
  (void)remove(manifest_path);
}

static void test_rejections(const char *directory)
{
  char source_path[512];
  char archive_path[512];
  join_path(source_path, sizeof(source_path), directory, "input.txt");
  join_path(archive_path, sizeof(archive_path), directory, "Invalid.xspkg.tar.zst");
  check(write_text(source_path, "content"), "rejection fixture is written");

  XsPackageArchiveInfo info = {0};
  XsPackageError error = {0};
  const XsPackageInput missing_manifest = {.archive_path = "Sources/input.txt", .source_path = source_path};
  check(xs_package_archive_write(archive_path, &missing_manifest, 1U, &info, &error) == XS_PACKAGE_MANIFEST_MISSING,
        "writer requires xspkg.json");

  const XsPackageInput unsafe[] = {
      {.archive_path = XS_PACKAGE_MANIFEST_PATH, .source_path = source_path},
      {.archive_path = "../escape", .source_path = source_path},
  };
  check(xs_package_archive_write(archive_path, unsafe, 2U, &info, &error) == XS_PACKAGE_INVALID_PATH,
        "writer rejects path traversal");
  check(xs_package_archive_verify("wrong-extension.tar.zst", &info, &error) == XS_PACKAGE_INVALID_ARGUMENT,
        "verifier requires the complete xspkg suffix");
  (void)remove(source_path);
}

int main(void)
{
  char directory[] = "/tmp/xs-package-tests-XXXXXX";
  check(mkdtemp(directory) != nullptr, "temporary package directory is created");
  if(failures == 0)
  {
    test_archive_roundtrip(directory);
    test_rejections(directory);
  }
  check(rmdir(directory) == 0, "temporary package directory is removed");
  return failures == 0 ? 0 : 1;
}
