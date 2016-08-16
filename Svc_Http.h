/*
 * Svc_Http.h
 *
 *  Created on: Aug 16,2016
 *      Author: zhangyalei
 */

#ifndef SVC_HTTP_H_
#define SVC_HTTP_H_

#include "Svc.h"
#include "Object_Pool.h"

class Svc_Http : public Svc_Handler {
public:
	Svc_Http(void);
	virtual ~Svc_Http(void);

	virtual int handle_recv(void);
	virtual int handle_send(void);
	virtual int handle_pack(Block_Vector &block_vec);
};

#endif /* SVC_HTTP_H_ */
