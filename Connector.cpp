/*
 * Connector.cpp
 *
 *  Created on: Jan 16,2016
 *      Author: zhangyalei
 */

#include "Connector.h"
#include "Misc.h"

int Connector_Connect::connect_svc(int connfd) {
	LOG_DEBUG("connfd=%d", connfd);

	Connector_Svc *svc = connector_->svc_pool().pop();
	if (! svc) {
		LOG_USER("svc == NULL");
		return -1;
	}

	int cid = connector_->svc_list().record_svc(svc);
	if (cid == -1) {
		LOG_USER("cid == -1");
		connector_->svc_pool().push(svc);
		return -1;
	}

	svc->reset();
	svc->set_max_list_size(Connector::svc_max_list_size);
	svc->set_max_pack_size(Connector::svc_max_pack_size);
	svc->set_cid(cid);
	svc->set_fd(connfd);
	svc->set_peer_addr();
	svc->set_connector(connector_);

	svc->register_recv_handler();
	svc->register_send_handler();

	return cid;
}

////////////////////////////////////////////////////////////////////////////////

int Connector_Receive::drop_handler(int cid) {
	return connector_->send().push_drop(cid);
}

Svc *Connector_Receive::find_svc(int cid) {
	return connector_->find_svc(cid);
}

////////////////////////////////////////////////////////////////////////////////


Block_Buffer *Connector_Send::pop_block(int cid) {
	return connector_->pop_block(cid);
}

int Connector_Send::push_block(int cid, Block_Buffer *buf) {
	return connector_->push_block(cid, buf);
}

int Connector_Send::drop_handler(int cid) {
	return connector_->pack().push_drop(cid);
}

Svc *Connector_Send::find_svc(int cid) {
	return connector_->find_svc(cid);
}

////////////////////////////////////////////////////////////////////////////////

Svc *Connector_Pack::find_svc(int cid) {
	return connector_->find_svc(cid);
}

Block_Buffer *Connector_Pack::pop_block(int cid) {
	return connector_->pop_block(cid);
}

int Connector_Pack::push_block(int cid, Block_Buffer *block) {
	return connector_->push_block(cid, block);
}

int Connector_Pack::packed_data_handler(Block_Vector &block_vec) {
	for (Block_Vector::iterator it = block_vec.begin(); it != block_vec.end(); ++it) {
		connector_->block_list().push_back(*it);
	}
	return 0;
}

int Connector_Pack::drop_handler(int cid) {
	LOG_DEBUG("drop_handler, cid = %d.", cid);
	connector_->recycle_svc(cid);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////

Block_Buffer *Connector_Svc::pop_block(int cid) {
	return connector_->pop_block(cid);
}

int Connector_Svc::push_block(int cid, Block_Buffer *block) {
	return connector_->push_block(cid, block);
}

int Connector_Svc::register_recv_handler(void) {
	if (! get_reg_recv()) {
		connector_->receive().register_svc(this);
		set_reg_recv(true);
	}
	return 0;
}

int Connector_Svc::unregister_recv_handler(void) {
	if (get_reg_recv()) {
		connector_->receive().unregister_svc(this);
		set_reg_recv(false);
	}
	return 0;
}

int Connector_Svc::register_send_handler(void) {
	if (! get_reg_send()) {
		connector_->send().register_svc(this);
		set_reg_send(true);
	}
	return 0;
}

int Connector_Svc::unregister_send_handler(void) {
	if (get_reg_send()) {
		connector_->send().unregister_svc(this);
		set_reg_send(true);
	}
	return 0;
}

int Connector_Svc::recv_handler(int cid) {
	connector_->pack().push_packing_cid(cid);
	return 0;
}

int Connector_Svc::close_handler(int cid) {
	connector_->receive().push_drop(cid);
	return 0;
}

////////////////////////////////////////////////////////////////////////////////

Connector::Connector(void):cid_(-1),port_(0) { }

Connector::~Connector(void) { }

void Connector::run_handler(void) {
	process_list();
}

void Connector::process_list(void) {
	LOG_USER_TRACE("SHOULD NOT HERE");
}

void Connector::set(std::string ip, int port, Time_Value &send_interval) {
	ip_ = ip;
	port_ = port;
	connect_.set(this);
	receive_.set(0, this);
	send_.set(0, this, send_interval);
	pack_.set(0, this);
}

int Connector::init(void) {
	receive_.init();
	send_.init();
	return 0;
}

int Connector::start(void) {
	receive_.thr_create();
	send_.thr_create();
	pack_.thr_create();
	return 0;
}

int Connector::connect_server(void) {
	cid_ = connect_.connect(ip_.c_str(), port_);
	LOG_DEBUG("cid_ = %d", cid_);
	return cid_;
}

Block_Buffer *Connector::pop_block(int cid) {
	return block_pool_group_.pop_block(cid);
}

int Connector::push_block(int cid, Block_Buffer *block) {
	return block_pool_group_.push_block(cid, block);
}

int Connector::send_block(int cid, Block_Buffer &buf) {
	return send_.push_data_block_with_len(cid, buf);
}

Svc *Connector::find_svc(int cid) {
	Connector_Svc *svc = 0;
	svc_static_list_.get_used_svc(cid, svc);
	return svc;
}

int Connector::recycle_svc(int cid) {
	Connector_Svc *svc = 0;
	svc_static_list_.get_used_svc(cid, svc);
	if (svc) {
		svc->close_fd();
		svc->reset();
		svc_static_list_.erase_svc(cid);
		svc_pool_.push(svc);
	}
	return 0;
}

int Connector::get_server_info(Server_Info &info) {
	info.svc_pool_free_list_size_ = svc_pool_.free_obj_list_size();
	info.svc_pool_used_list_size_ = svc_pool_.used_obj_list_size();
	info.svc_list_size_ = svc_static_list_.size();
	block_pool_group_.block_group_info(info.block_group_info_);
	return 0;
}

void Connector::free_cache(void) {
	block_pool_group_.shrink_all();
	svc_pool_.shrink_all();
}
