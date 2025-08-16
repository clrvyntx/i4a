#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void client_open();
void client_close();
bool client_send_message(const uint8_t *msg, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif // _CLIENT_H_
