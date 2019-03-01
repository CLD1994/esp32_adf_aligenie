
#ifndef BLUFI_H
#define BLUFI_H

#if CONFIG_BT_ENABLED

void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free);
int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len);
int blufi_security_init(void);
void blufi_security_deinit(void);
esp_err_t agWifiCfgThruBtInit();

#endif

#endif
