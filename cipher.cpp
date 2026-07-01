#include "cipher_api.h"

#include <cstring>
#include <cstdlib>

static unsigned char* processBytes(
    const unsigned char* data,
    size_t data_size,
    const char* key,
    size_t* out_size
) {
    if (data == nullptr || key == nullptr || out_size == nullptr) {
        return nullptr;
    }

    size_t key_length = std::strlen(key);

    if (key_length == 0) {
        return nullptr;
    }

    unsigned char* result = new unsigned char[data_size];

    for (size_t i = 0; i < data_size; i++) {
        result[i] = data[i] ^ static_cast<unsigned char>(key[i % key_length]);
    }

    *out_size = data_size;
    return result;
}

extern "C" {

    EXPORT unsigned char* cipher_encrypt_bytes(
        const unsigned char* data,
        size_t data_size,
        const char* key,
        size_t* out_size
    ) {
        return processBytes(data, data_size, key, out_size);
    }

    EXPORT unsigned char* cipher_decrypt_bytes(
        const unsigned char* data,
        size_t data_size,
        const char* key,
        size_t* out_size
    ) {
        return processBytes(data, data_size, key, out_size);
    }

    EXPORT void cipher_free_bytes(unsigned char* data) {
        delete[] data;
    }

}