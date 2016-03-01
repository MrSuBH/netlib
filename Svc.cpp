/*
 * Svc.cpp
 *
 *  Created on: Dec 16,2015
 *      Author: zhangyalei
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <limits.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include "Svc.h"
#include "Block_Buffer.h"
#include "Lib_Log.h"
#include "Server.h"
#include "Connector.h"

Svc::Svc(void):
	server_(0),
	connector_(0),
	cid_(0),
  max_list_size_(MAX_LIST_SIZE),
  max_pack_size_(MAX_PACK_SIZE),
  is_closed_(false),
  is_reg_recv_(false),
  is_reg_send_(false),
  peer_port_(0),
  role_id_(0)
{ }

Svc::~Svc(void) { }

void Svc::set_server(Server *server) {
	server_ = server;
}

void Svc::set_connector(Connector *connector) {
	connector_ = connector;
}

Block_Buffer *Svc::pop_block(int cid) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::push_block(int cid, Block_Buffer *block) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::register_recv_handler(void) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::unregister_recv_handler(void) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::register_send_handler(void) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::unregister_send_handler(void) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::recv_handler(int cid) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::close_handler(int cid) {
	LIB_LOG_TRACE("SHOULD NOT HERE");
	return 0;
}

int Svc::handle_input(void) {
	return recv_data();
}

int Svc::handle_close(void) {
	if (is_closed_) {
		return 0;
	} else {
		is_closed_ = true;
		return close_handler(cid_);
	}
}

int Svc::close_fd(void) {
	if (is_closed_) {
		LIB_LOG_DEBUG("close fd = %d", this->get_fd());
		::close(this->get_fd());
	}
	return 0;
}

int Svc::get_peer_addr(std::string &ip, int &port) {
	struct sockaddr_in sa;
	socklen_t len = sizeof(sa);

	if (::getpeername(this->get_fd(), (struct sockaddr *)&sa, &len) < 0) {
		LIB_LOG_TRACE("getpeername wrong, fd:%d", this->get_fd());
		return -1;
	}

	std::stringstream stream;
	stream << inet_ntoa(sa.sin_addr);

	ip = stream.str();
	port = ntohs(sa.sin_port);

	return 0;
}

int Svc::get_local_addr(std::string &ip, int &port) {
	struct sockaddr_in sa;
	socklen_t len = sizeof(sa);

	if (::getsockname(this->get_fd(), (struct sockaddr *)&sa, &len) < 0) {
		LIB_LOG_TRACE("getsockname wrong, fd:%d", this->get_fd());
		return -1;
	}

	std::stringstream stream;
	stream << inet_ntoa(sa.sin_addr);

	ip = stream.str();
	port = ntohs(sa.sin_port);

	return 0;
}

int Svc::recv_data(void) {
	if (is_closed_)
		return 0;

	Block_Buffer *buf = pop_block(cid_);
	if (! buf) {
		LIB_LOG_TRACE("pop_block return 0");
		return -1;
	}
	buf->reset();
	buf->write_int32(cid_);

	int n = 0;
	while (1) {
		buf->ensure_writable_bytes(2000); /// every read system call try to read 2k data
		n = 0;
		if ((n = ::read(get_fd(), buf->get_write_ptr(), buf->writable_bytes())) < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EWOULDBLOCK)
				break;

			LIB_LOG_INFO("role_id:%ld read", role_id_);

			push_block(cid_, buf);
			handle_close();

			return 0;
		} else if (n == 0) { /// EOF
			LIB_LOG_DEBUG("role_id:%ld fd=%d, read return 0, EOF close", role_id_, get_fd());

			push_block(cid_, buf);
			handle_close();

			return 0;
		} else {
			buf->set_write_idx(buf->get_write_idx() + n);
		}
	}

	//LIB_LOG_DEBUG("recv %d data", buf->readable_bytes());

	if (push_recv_block(buf) == 0) {
		recv_handler(cid_);
	} else {
		push_block(cid_, buf);
	}

	return 0;
}

int Svc::send_data(void) {
	if (is_closed_)
		return 0;

	while (1) {
		size_t buf_size = send_block_list_.size();
		if (! buf_size)
			return 0;

		size_t sum_bytes = 0;
		std::vector<iovec> iov_vec;
		std::vector<Block_Buffer *> iov_buff;

		iov_vec.clear();
		iov_buff.clear();

		if (send_block_list_.construct_iov(iov_vec, iov_buff, sum_bytes) == 0) {
			LIB_LOG_TRACE("construct_iov return 0");
			return 0;
		}

		int ret = ::writev(this->get_fd(), &*iov_vec.begin(), iov_vec.size());
		if (ret == -1) {
			LIB_LOG_INFO("writev cid:%d ip = [%s], port = %d", cid_, peer_ip_.c_str(), peer_port_);
			if (errno == EINTR) { /// 被打断, 重写
				continue;
			} else if (errno == EWOULDBLOCK) { /// EAGAIN,下一次超时再写
				return ret;
			} else { /// 其他错误，丢掉该客户端全部数据
				LIB_LOG_INFO("role_id:%ld writev cid:%d ip = [%s], port = %d handle_colse", role_id_, cid_, peer_ip_.c_str(), peer_port_);
				handle_close();
				return ret;
			}
		} else {
			if ((size_t)ret == sum_bytes) { /// 本次全部写完, 尝试继续写
				for (std::vector<Block_Buffer *>::iterator it = iov_buff.begin(); it != iov_buff.end(); ++it) {
					push_block(cid_, *it);
				}
				send_block_list_.pop_front(iov_buff.size());
				continue;
			} else { /// 未写完, 丢掉已发送的数据, 下一次超时再写
				size_t writed_bytes = ret, remove_count = 0;
				Block_Buffer *buf = 0;
				for (std::vector<Block_Buffer *>::iterator it = iov_buff.begin(); it != iov_buff.end(); ++it) {
					buf = *it;
					if (writed_bytes >= buf->readable_bytes()) {
						++remove_count;
						writed_bytes -= buf->readable_bytes();
						push_block(cid_, buf);
					} else {
						break;
					}
				}
				send_block_list_.pop_front(remove_count, writed_bytes);
				return ret;
			}
		}
	}

	return 0;
}

int Svc::pack_recv_data(Block_Vector &block_vec) {
	size_t rd_idx_orig = 0;
	int32_t cid = 0;
	uint16_t len = 0;
	Block_Buffer *front_buf = 0;
	Block_Buffer *free_buf = 0;

	while (! recv_block_list_.empty()) {
		front_buf = recv_block_list_.front();
		if (! front_buf) {
			LIB_LOG_TRACE("front_buf == 0");
			continue;
		}

		rd_idx_orig = front_buf->get_read_idx();
		cid = front_buf->read_int32();
		if (front_buf->readable_bytes() <= 0) { /// 数据块异常, 关闭该连接
			LIB_LOG_TRACE("role_id:%ld cid:%d data block error.", role_id_, cid);
			recv_block_list_.pop_front();
			front_buf->reset();
			push_block(cid_, front_buf);
			handle_close();
			return -1;
		}

		if (front_buf->readable_bytes() < sizeof(len)) { /// 2字节的包长度标识都不够
			front_buf->set_read_idx(rd_idx_orig);
			if ((free_buf = recv_block_list_.merge_first_second()) == 0) {
				return 0;
			} else {
				push_block(cid_, free_buf);
				continue;
			}
		}

		len = front_buf->peek_uint16();
		size_t data_len = front_buf->readable_bytes() - sizeof(len);

		if (len == 0 || len > max_pack_size_) { /// 包长度标识为0, 包长度标识超过max_pack_size_, 即视为无效包, 异常, 关闭该连接
			LIB_LOG_TRACE("cid:%d role_id:%ld data block len = %u", cid_, role_id_, len);
			front_buf->log_binary_data(512);
			recv_block_list_.pop_front();
			front_buf->reset();
			push_block(cid_, front_buf);
			this->handle_close();
			return -1;
		}

		if (data_len == (size_t)len) {
			front_buf->set_read_idx(rd_idx_orig);
			recv_block_list_.pop_front();
			block_vec.push_back(front_buf);
			continue;
		} else {
			if (data_len < (size_t)len) {		//半包，需要合并前两个包
				front_buf->set_read_idx(rd_idx_orig);
				if ((free_buf = recv_block_list_.merge_first_second()) == 0) {
					return 0;
				} else {
					push_block(cid_, free_buf);
					continue;
				}
			}

			if (data_len > len) {				//粘包，需要拆分包
				size_t wr_idx_orig = front_buf->get_write_idx();

				Block_Buffer *data_buf = pop_block(cid_);
				data_buf->reset();
				data_buf->write_int32(cid_);
				data_buf->copy(front_buf->get_read_ptr(), sizeof(len) + len);

				block_vec.push_back(data_buf);

				size_t cid_idx = front_buf->get_read_idx() + sizeof(len) + len - sizeof(int32_t);
				front_buf->set_read_idx(cid_idx);
				front_buf->set_write_idx(cid_idx);
				front_buf->write_int32(cid_);
				front_buf->set_write_idx(wr_idx_orig);

				continue;
			}
		}
	}

	return 0;
}
