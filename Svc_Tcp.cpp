/*
 * Svc_Tcp.cpp
 *
 *  Created on: Apr 19,2016
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
#include <string.h>
#include "Svc_Tcp.h"

Svc_Tcp::Svc_Tcp():
	Svc_Handler()
{
}

Svc_Tcp::~Svc_Tcp(){
}

int Svc_Tcp::handle_recv(void){
	if (parent_->is_closed())
		return 0;
	
	int cid = parent_->get_cid();
	Block_Buffer *buf = parent_->pop_block(cid);
	if (! buf) {
		LIB_LOG_ERROR("tcp pop_block fail, cid:%d", cid);
		return -1;
	}
	buf->reset();
	buf->write_int32(cid);

	int n = 0;
	while (1) {
		buf->ensure_writable_bytes(2000); /// every read system call try to read 2k data
		n = 0;
		if ((n = ::read(parent_->get_fd(), buf->get_write_ptr(), buf->writable_bytes())) < 0) {
			if (errno == EINTR)
				continue;
			if (errno == EWOULDBLOCK)
				break;

			LIB_LOG_ERROR("tcp read < 0 cid:%d fd=%d,n:%d", cid, parent_->get_fd(), n);
			parent_->push_block(cid, buf);
			parent_->handle_close();
			return 0;
		} else if (n == 0) { /// EOF
			LIB_LOG_ERROR("tcp read eof close cid:%d fd=%d", cid, parent_->get_fd());
			parent_->push_block(cid, buf);
			parent_->handle_close();
			return 0;
		} else {
			buf->set_write_idx(buf->get_write_idx() + n);
		}
	}

	if (push_recv_block(buf) == 0) {
		parent_->recv_handler(cid);
	} else {
		parent_->push_block(cid, buf);
	}

	return 0;
}

int Svc_Tcp::handle_send(void){
	if (parent_->is_closed())
		return 0;

	while (!send_block_list_.empty()) {
		size_t sum_bytes = 0;
		std::vector<iovec> iov_vec;
		std::vector<Block_Buffer *> iov_buff;
		iov_vec.clear();
		iov_buff.clear();
		
		int cid = parent_->get_cid();
		if (send_block_list_.construct_iov(iov_vec, iov_buff, sum_bytes) == 0) {
			LIB_LOG_ERROR("construct_iov return 0");
			return 0;
		}

		int ret = ::writev(parent_->get_fd(), &*iov_vec.begin(), iov_vec.size());
		if (ret == -1) {
			LIB_LOG_ERROR("writev cid:%d fd:%d ip:%s port:%d errno:%d", cid, parent_->get_fd(), parent_->get_peer_ip().c_str(), parent_->get_peer_port(), errno);
			if (errno == EINTR) { /// 被打断, 重写
				continue;
			} else if (errno == EWOULDBLOCK) { /// EAGAIN,下一次超时再写
				return ret;
			} else { /// 其他错误，丢掉该客户端全部数据
				parent_->handle_close();
				return ret;
			}
		} else {
			if ((size_t)ret == sum_bytes) { /// 本次全部写完, 尝试继续写
				for (std::vector<Block_Buffer *>::iterator it = iov_buff.begin(); it != iov_buff.end(); ++it) {
					parent_->push_block(cid, *it);
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
						parent_->push_block(cid, buf);
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

int Svc_Tcp::handle_pack(Block_Vector &block_vec) {
	size_t rd_idx_orig = 0;
	int32_t cid = 0;
	uint16_t len = 0;
	Block_Buffer *front_buf = 0;
	Block_Buffer *free_buf = 0;

	while (! recv_block_list_.empty()) {
		front_buf = recv_block_list_.front();
		if (! front_buf) {
			LIB_LOG_ERROR("front_buf == 0");
			continue;
		}

		rd_idx_orig = front_buf->get_read_idx();
		cid = front_buf->read_int32();
		if (front_buf->readable_bytes() <= 0) { /// 数据块异常, 关闭该连接
			LIB_LOG_ERROR("cid:%d fd:%d, data block read bytes<0", cid, parent_->get_fd());
			recv_block_list_.pop_front();
			front_buf->reset();
			parent_->push_block(parent_->get_cid(), front_buf);
			parent_->handle_close();
			return -1;
		}

		if (front_buf->readable_bytes() < sizeof(len)) { /// 2字节的包长度标识都不够
			front_buf->set_read_idx(rd_idx_orig);
			if ((free_buf = recv_block_list_.merge_first_second()) == 0) {
				return 0;
			} else {
				parent_->push_block(parent_->get_cid(), free_buf);
				continue;
			}
		}

		len = front_buf->peek_uint16();
		size_t data_len = front_buf->readable_bytes() - sizeof(len);
		if (len == 0 || len > max_pack_size_) { /// 包长度标识为0, 包长度标识超过max_pack_size_, 即视为无效包, 异常, 关闭该连接
			LIB_LOG_ERROR("cid:%d fd:%d data block len = %u", cid, parent_->get_fd(), len);
			front_buf->log_binary_data(512);
			recv_block_list_.pop_front();
			front_buf->reset();
			parent_->push_block(parent_->get_cid(), front_buf);
			parent_->handle_close();
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
					parent_->push_block(parent_->get_cid(), free_buf);
					continue;
				}
			}

			if (data_len > len) {				//粘包，需要拆分包
				int cid = parent_->get_cid();
				size_t wr_idx_orig = front_buf->get_write_idx();

				Block_Buffer *data_buf = parent_->pop_block(cid);
				data_buf->reset();
				data_buf->write_int32(cid);
				data_buf->copy(front_buf->get_read_ptr(), sizeof(len) + len);

				block_vec.push_back(data_buf);

				size_t cid_idx = front_buf->get_read_idx() + sizeof(len) + len - sizeof(int32_t);
				front_buf->set_read_idx(cid_idx);
				front_buf->set_write_idx(cid_idx);
				front_buf->write_int32(cid);
				front_buf->set_write_idx(wr_idx_orig);

				continue;
			}
		}
	}

	return 0;
}

