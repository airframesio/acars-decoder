/* CRC-16 helpers — bitwise, table-free. Mirrors runtimes/rust/src/crc.rs and
 * runtimes/typescript/utils/arinc_702_helper.ts (post-fix). */

#include <stddef.h>
#include <stdint.h>

uint16_t ads_crc16_ibm_sdlc_rev(const uint8_t *data, size_t len) {
    uint32_t crc = 0xffff;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0x8408;
            else              crc = crc >> 1;
        }
    }
    crc = (crc ^ 0xffff) & 0xffff;
    uint16_t n1 = (uint16_t)((crc >> 12) & 0xf);
    uint16_t n2 = (uint16_t)((crc >> 8)  & 0xf);
    uint16_t n3 = (uint16_t)((crc >> 4)  & 0xf);
    uint16_t n4 = (uint16_t)(crc & 0xf);
    return (uint16_t)((n4 << 12) | (n3 << 8) | (n2 << 4) | n1);
}

uint16_t ads_crc16_genibus(const uint8_t *data, size_t len) {
    uint32_t crc = 0xffff;
    const uint32_t polynomial = 0x1021;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint32_t)data[i]) << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000) crc = ((crc << 1) ^ polynomial) & 0xffff;
            else              crc = (crc << 1) & 0xffff;
        }
    }
    return (uint16_t)((crc ^ 0xffff) & 0xffff);
}
