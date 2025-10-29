#ifndef _SERVER_H_
#define _SERVER_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void server_create();
void server_close();
bool server_send_message(const uint8_t *msg, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif // _SERVER_H_
