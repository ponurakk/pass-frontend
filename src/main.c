#include "crypto.h"
#include <gpgme.h>
#include <stdio.h>

int main() {
  gpgme_check_version(NULL);
  char *password;

  if (decrypt_file("./tmp.gpg", &password) != 0) {
    fprintf(stderr, "Failed to decrypt file\n");
    return 1;
  }

  printf("%s\n", password);

  return 0;
}
