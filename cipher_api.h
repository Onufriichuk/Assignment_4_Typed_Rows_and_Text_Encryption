#ifndef CIPHER_API_H
#define CIPHER_API_H

#include <stddef.h>

#ifdef _WIN32
    #define EXPORT __declspec(dllexport)
#else
    #define EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    EXPORT unsigned char* cipher_encrypt_bytes(
        const unsigned char* data,
        size_t data_size,
        const char* key,
        size_t* out_size
    );

    EXPORT unsigned char* cipher_decrypt_bytes(
        const unsigned char* data,
        size_t data_size,
        const char* key,
        size_t* out_size
    );

    EXPORT void cipher_free_bytes(unsigned char* data);

#ifdef __cplusplus
}
#endif

#endif