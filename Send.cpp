/*
 * Send.cpp
 *
 *  Created on: Dec 16,2015
 *      Author: zhangyalei
 */

#include "Svc.h"
#include "Lib_Log.h"
#include "Common_Func.h"
#include "Send.h"
#include "Server.h"
#include "Connector.h"

Send::Send(void):
	server_(0),
	connector_(0),
	reactor_(0),
  svc_map_(max_fd()),
  interval_(Time_Value::zero),
  is_register_self_(false)
{ }

Send::~Send(void) { }

void Send::set(Server *server, Connector *connector, Time_Value &interval) {
	server_ = server;
	connector_ = connector;
	interval_ = interval;
}

int Send::init(void) {
	if ((reactor_ = new Epoll_Watcher) == 0) {
		LIB_LOG_FATAL("Sender new Epoll_Watcher");
	}
	return 0;
}

int Send::fini(void) {
	if (reactor_) {
		reactor_->end_loop();
		delete reactor_;
		reactor_ = 0;
	}
	return 0;
}

void Send::run_handler(void) {
	LIB_LOG_DEBUG("start sender");
	register_self_timer();
	reactor_->loop();
	return ;
}

/// 获取、释放一个buf
Block_Buffer *Send::pop_block(int cid) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Send::push_block(int cid, Block_Buffer *buf) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Send::push_data_block_with_len(int cid, Block_Buffer &rbuf) {
	if (rbuf.readable_bytes() <= 0) {
		LIB_LOG_TRACE("block readable_bytes = %ul.", rbuf.readable_bytes());
		return -1;
	}

	Block_Buffer *buf = pop_block(cid);
	if (! buf) {
		LIB_LOG_TRACE("buf == 0");
		return -1;
	}

	buf->reset();
	buf->write_int32(cid);
	buf->copy(&rbuf);
	append_list_.push_back(buf);

	return 0;
}


int Send::append_send_block(void) {
	int32_t cid = 0;
	Block_Buffer *buf = 0;
	Svc *svc = 0;

	while ((buf = append_list_.pop_front()) != 0) {
		cid = buf->read_int32();
		if ((svc = find_svc(cid)) != 0) {
			if ((svc->push_send_block(buf)) != 0)
				push_block(cid, buf);
		} else {
			push_block(cid, buf);
		}
	}
	return 0;
}

int Send::push_drop(int cid) {
	drop_list_.push_back(cid);
	reactor_->notify();
	return 0;
}

int Send::process_drop(void) {
	int cid = 0;
	Svc *svc = 0;

	while (! drop_list_.empty()) {
		cid = drop_list_.pop_front();
		if ((svc = find_svc(cid)) != 0) {
			if (svc->get_reg_send()) {
				svc->unregister_send_handler();
				svc->set_reg_send(false);
			}
			drop_handler(cid);
		}
	}
	return 0;
}

int Send::register_svc(Svc *svc) {
	GUARD(Svc_Map_Lock, mon, svc_map_lock_);
	svc_map_.insert(std::make_pair(svc->get_cid(), svc));
	return 0;
}

int Send::unregister_svc(Svc *svc) {
	GUARD(Svc_Map_Lock, mon, svc_map_lock_);
	svc_map_.erase(svc->get_cid());
	return 0;
}

int Send::register_self_timer(void) {
	if (interval_ == Time_Value::zero) {
		LIB_LOG_FATAL("interval_ == Time_Value::zero");
		return -1;
	}

	if (! is_register_self_) {
		reactor_->add(this, Epoll_Watcher::EVENT_TIMEOUT, &interval_);
		is_register_self_ = true;
	}

	return 0;
}

int Send::drop_handler(int cid) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

Svc *Send::find_svc(int cid) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

Epoll_Watcher *Send::reactor(void) {
	return reactor_;
}

Time_Value &Send::get_interval(void) {
	return interval_;
}

void Send::set_interval(Time_Value &tv) {
	interval_ = tv;
}

int Send::handle_timeout(const Time_Value &tv) {
	append_send_block();
	process_drop();

	GUARD(Svc_Map_Lock, mon, svc_map_lock_);
	for (Svc_Map::iterator it = svc_map_.begin(); it != svc_map_.end(); ++it) {
		if (it->second->is_closed())
			continue;
		it->second->send_data();
	}

	return 0;
}
