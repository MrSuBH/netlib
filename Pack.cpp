/*
 * Pack.h
 *
 *  Created on: Dec 16,2015
 *      Author: zhangyalei
 */

#include <iostream>
#include "Pack.h"
#include "Server.h"
#include "Connector.h"

Pack::Pack(void):server_(0),connector_(0) { }

Pack::~Pack(void) { }

int Pack::set(Server *server, Connector *connector) {
	server_ = server;
	connector_ = connector;
	return 0;
}

int Pack::push_packing_cid(int cid) {
	packing_list_.push_back(cid);
	return 0;
}

Svc *Pack::find_svc(int cid) {
	LOG_USER_TRACE("SHOULD NOT HERE");
	return 0;
}

Block_Buffer *Pack::pop_block(int cid) {
	LOG_USER_TRACE("SHOULD NOT HERE");
	return 0;
}

int Pack::push_block(int cid, Block_Buffer *block) {
	LOG_USER_TRACE("SHOULD NOT HERE");
	return 0;
}

int Pack::packed_data_handler(Block_Vector &block_vec) {
	LOG_USER_TRACE("SHOULD NOT HERE");
	return 0;
}

int Pack::drop_handler(int cid) {
	LOG_USER_TRACE("SHOULD NOT HERE");
	return 0;
}

int Pack::push_drop(int cid) {
	drop_list_.push_back(cid);
	return 0;
}

int Pack::process_drop(void) {
	int cid = 0;
	while (! drop_list_.empty()) {
		cid = drop_list_.pop_front();
		drop_handler(cid);
	}
	return 0;
}

void Pack::run_handler(void) {
	LOG_DEBUG("start packer");
	process();
}

int Pack::process(void) {
	while (1) {
		process_packing_list();
		process_drop();
		Time_Value::sleep(Time_Value(0, 100));
	}
	return 0;
}

int Pack::process_packing_list(void) {
	Svc *svc = 0;
	int cid = 0;
	Block_Vector block_vec;

	while (! packing_list_.empty()) {
		cid = packing_list_.pop_front();
		if ((svc = find_svc(cid)) == 0) {
			LOG_USER_TRACE("cannot find svc cid = %d.", cid);
			continue;
		}
		block_vec.clear();
		svc->pack_recv_data(block_vec);

		if (block_vec.size())
			packed_data_handler(block_vec);
	}
	return 0;
}

void Pack::dump_info(void) {
		std::cout << packing_list_.size() << std::endl;
}
