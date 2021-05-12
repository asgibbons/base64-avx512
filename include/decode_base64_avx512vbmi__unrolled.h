#pragma once

#include <stdint.h>
#include <stddef.h>

size_t decodeBlock(unsigned char *src, int start_offset, int end_offset, unsigned char *dst, int dst_offset, int isURL);
size_t decode_base64_avx512vbmi__unrolled(uint8_t* output, const uint8_t* input, size_t size);
