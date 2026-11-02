#ifndef CRYPTO_H
#define CRYPTO_H
#include <stddef.h>

int decrypt_file(const char *path, char **out, char *home, size_t line);

#endif // !CRYPTO_H
