/*
 * Svc_Tcp.h
 *
 *  Created on: Apr 19,2016
 *      Author: zhangyalei
 */

#ifndef SVC_TCP_H_
#define SVC_TCP_H_

#include "Svc.h"
#include "Object_Pool.h"

class Svc_Tcp : public Svc_Handler {
public:
	Svc_Tcp(void);
	virtual ~Svc_Tcp(void);

	static Svc_Tcp *create_object();
	static void reclaim_object(Svc_Tcp *svc_tcp);
	virtual int handle_recv(void);
	virtual int handle_send(void);
	virtual int handle_pack(Block_Vector &block_vec);
};

#endif

