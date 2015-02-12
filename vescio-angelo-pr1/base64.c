#include <stdint.h>
#include <stdlib.h>
#include "base64.h"

int base64_encode(const unsigned char *data,
    char ** encoded_data_ptr,
                    int input_length,
                    int *output_length) {

    *output_length = 4 * ((input_length + 2) / 3);
    int length =*output_length;
    *encoded_data_ptr = calloc(length,sizeof(char*));
    char* encoded_data = *encoded_data_ptr;
    //memset(encoded_data,0,*output_length);
    printf("encoded_data %p\n", encoded_data);
    if (encoded_data == NULL) return NULL;
    for (int i = 0, j = 0; i < input_length;) {
        
        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;
        // printf("\t1 i was %d j was %d\n", i,j);
        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        // printf("\t2 i was %d j was %d\n", i,j);
        char encChar = encoding_table[(triple >> 3 * 6) & 0x3F];
        // printf("\t2a i was %d j was %d\n", i,j);
        encoded_data[j++] = encChar;
        // printf("\t3 i was %d j was %d\n", i,j);
        encChar = encoding_table[(triple >> 2 * 6) & 0x3F];
        //printf("\t3a i was %d j was %d\n", i,j);
        encoded_data[j++] = encChar;
        // printf("\t4 i was %d j was %d\n", i,j);
        encChar = encoding_table[(triple >> 1 * 6) & 0x3F];
        // printf("\t4a i was %d j was %d\n", i,j);
        encoded_data[j++] = encChar;
        // printf("\t5 i was %d j was %d\n", i,j);
        encChar = encoding_table[(triple >> 0 * 6) & 0x3F];
        // printf("\t5a i was %d j was %d\n", i,j);
        encoded_data[j++] = encChar;
        // printf("\t6 i was %d j was %d\n", i,j);
    }
    for (int i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[*output_length - 1 - i] = '=';
    // printf("Here is the message:\n");
    // for (int i = 0; i < length; i++)
    // {
    //     printf("%02X", encoded_data[i]);
    // }
    return 0;
}


int base64_decode(const char *data,
    uint8_t ** decoded_data_ptr,
                             size_t input_length,
                             size_t *output_length) {

    if (decoding_table == NULL) build_decoding_table();

    if (input_length % 4 != 0) return NULL;

    *output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') (*output_length)--;
    if (data[input_length - 2] == '=') (*output_length)--;

    *decoded_data_ptr = calloc(*output_length,sizeof(char*));
    char* decoded_data = *decoded_data_ptr;
    if (decoded_data == NULL) return NULL;

    for (int i = 0, j = 0; i < input_length;) {

        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < *output_length) (decoded_data)[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < *output_length) (decoded_data)[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < *output_length) (decoded_data)[j++] = (triple >> 0 * 8) & 0xFF;
    }

    return 0;
}


void build_decoding_table() {

    decoding_table = malloc(256);

    for (int i = 0; i < 64; i++)
        decoding_table[(unsigned char) encoding_table[i]] = i;
}


void base64_cleanup() {
    free(decoding_table);
}