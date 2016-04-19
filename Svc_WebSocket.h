/*
 * Svc_Websocket.h
 *
 *  Created on: Apr 19,2016
 *      Author: zhangyalei
 */

#ifndef SVC_WEBSOCKET_H_
#define SVC_WEBSOCKET_H_

#include "Block_Buffer.h"
#include "Svc.h"
#include "Object_Pool.h"

#define MAGIC_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

enum WEBSOCKET_FRAME_CODE{
	FRAME_NORMAL = 0x0,
	FRAME_FINAL = 0x1,
};

enum WEBSOCKET_OPCODE_CODE{
	OPCODE_CONTINUATION = 0x0,
	OPCODE_TEXT = 0x1,
	OPCODE_BINARY = 0x2,
	OPCODE_CLOSE = 0x8,
	OPCODE_PING = 0x9,
	OPCODE_PONG = 0xa,
};

class Svc_Websocket : public Svc_Handler {
public:
	Svc_Websocket(void);
	virtual ~Svc_Websocket(void);

	static Svc_Websocket *create_object();
	static void reclaim_object(Svc_Websocket *svc_websocket);
	void reset(void);
	virtual int handle_recv(void);
	virtual int handle_send(void);
	virtual int handle_pack(Block_Vector &block_vec);

private:
	Block_Buffer *get_websocket_payload(int16_t payload_length, uint8_t *masking_key, Block_Buffer *buf);
	int websocket_handshake(Block_Buffer *buf);
	Block_Buffer *make_websocket_frame(Block_Buffer *buf, uint8_t *op = NULL);

private:
	bool websocket_connected_;
};

#endif

