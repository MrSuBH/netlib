/*
 * Svc_Websocket.h
 *
 *  Created on: Apr 19,2016
 *      Author: zhangyalei
 */

#ifndef SVC_WEBSOCKET_H_
#define SVC_WEBSOCKET_H_

#include "Svc.h"

#define MAGIC_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

class Svc_Websocket : public Svc_Handler {
public:
	Svc_Websocket(void);
	virtual ~Svc_Websocket(void);

	void reset(void);
	virtual int handle_send(void);
	virtual int handle_pack(Block_Vector &block_vec);

private:
	Block_Buffer *get_websocket_payload(int16_t payload_length, uint8_t *masking_key, Block_Buffer *buf);
	int websocket_handshake(Block_Buffer *buf);
	Block_Buffer *make_websocket_frame(Block_Buffer *buf, uint8_t *op = NULL);

private:
	bool connected_;
};

#endif

