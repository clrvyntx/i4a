#ifndef _PYSIM_PROTOCOL_H_
#define _PYSIM_PROTOCOL_H_

#define PS_STATUS_MASK_ERROR 0x80

#define PS_CMD_LONG_POLL      0xF4
#define PS_CMD_RETRIEVE_EVENT 0xF5

#define PS_PACK_CMD(cmd, payload_len)   (((cmd) << 24) | (payload_len))
#define PS_RESPONSE_LEN(response)       ((response) & 0x00FFFFFF)
#define PS_RESPONSE_STATUS(response)    ((response) >> 24)
#define PS_IS_ERROR(response)           (PS_RESPONSE_STATUS(response) & PS_STATUS_MASK_ERROR)
#define PS_IS_SUCCESS(response)         (!PS_IS_ERROR(response))

#endif // _PYSIM_PROTOCOL_H_
