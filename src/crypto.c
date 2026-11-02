#include "crypto.h"
#include <gpgme.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

int decrypt_file(const char *path, char **out, char *home, size_t line) {
  gpgme_ctx_t ctx = NULL;
  gpgme_data_t cipher = NULL;
  gpgme_data_t plain = NULL;
  int result = 1;

  gpgme_error_t err = gpgme_new(&ctx);

  if (!err)
    err = gpgme_set_protocol(ctx, GPGME_PROTOCOL_OpenPGP);

  char *new_path = NULL;
  if (asprintf(&new_path, "%s/.password-store%s", home, path) == -1) {
    result = 1;
    return result;
  };

  if (!err)
    err = gpgme_data_new_from_file(&cipher, new_path, 1);

  if (!err)
    err = gpgme_data_new(&plain);

  if (!err)
    err = gpgme_op_decrypt(ctx, cipher, plain);

  if (err) {
    fprintf(stderr, "GPGME: %s\n", gpgme_strerror(err));
  } else if (gpgme_data_seek(plain, 0, SEEK_SET) == -1) {
    perror("gpgme_data_seek");
  } else {
    char password[256] = {0};
    ssize_t n = gpgme_data_read(plain, password, sizeof(password) - 1);

    if (n < 0) {
      fprintf(stderr, "Failed to read decrypted data\n");
      result = 1;
    } else {
      password[n] = '\0';

      // Keep only the first line (the password).
      char *newline = strpbrk(password, "\r\n");
      while (line) {
        if (newline) {
          char *second = newline + 1;

          if (*newline == '\r' && *second == '\n')
            second++;

          memmove(password, second, strlen(second) + 1);
        }

        newline = strpbrk(password, "\r\n");
        line--;
      }

      if (newline) {
        *newline = '\0';
      }

      *out = strdup(password);

      result = 0;
    }
  }

  if (plain)
    gpgme_data_release(plain);

  if (cipher)
    gpgme_data_release(cipher);

  if (ctx)
    gpgme_release(ctx);

  return result;
}
