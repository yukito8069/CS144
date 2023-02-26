#include "tcp_connection.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "tcp_state.hh"
#include "wrapping_integers.hh"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <sys/socket.h>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _remain_capacity;}

size_t TCPConnection::bytes_in_flight() const { 
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes();}

size_t TCPConnection::time_since_last_segment_received() const {
    return _time_since_last_segment_received;
}

//ACK包就是仅ACK 标记设为1的TCP包. 需要注意的是当三此握手完成、连接建立以后，TCP连接的每个包都会设置ACK位
void TCPConnection::segment_received(const TCPSegment &seg) { 
    _time_since_last_segment_received = 0;
    //把收到的段传给reciver, 对fin段的处理也会直接交给receiver
    _receiver.segment_received(seg);
    //如果收到的段的ack字段是1，那么它的确认号有效，说明sender发出的段可能有新的被确认的部分
    if(seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        
        //如果正在发送 keep-alive 用的ack包，现在收到了新的包，应该停止这个动作
        if(_need_send_ack == true) {
            _need_send_ack = false;
        }
    }
    //如果这个段占用了序列号，也就是有字节，也就是不是ack空包，那么我们需要给对等体回复一个ack段
    if(seg.length_in_sequence_space()) {
        // _sender.send_empty_segment(1, 0);
        send_ack();
    }

    //如果收到了一个rst段，就终止连接 (tcp不会给ack段发ack)
    if(seg.header().rst) {
        dispose_rst();
        return;
    }


    // keep-alive 段的 seqno 会是不合法的，且它会是一个不占用序列号的空段,下面是lab文档里直接搬下来的对 keep-alive 的处理
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) && seg.header().seqno == _receiver.ackno().value() - 1) {
        send_ack();
    }
    // 想在这里写收到最新段的时间，应该是要先更新cur_time然后赋值给pre_time，但是tick里不知道要传什么数值，所以应该是在别的地方实现这个的修改？

    if(TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED) {
        if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV) {
            // 这种情况下相当于客户端发送了fin到并被确认，并收到服务器的fin报并返回ack后，进入了time_wait的阶段
            _tcpstate = TCPState::State::TIME_WAIT;
        }

    }
}


bool TCPConnection::active() const { 
    return _is_worked;
}

//超过容量的部分是直接丢弃还是怎么存下来后面再传
size_t TCPConnection::write(const string &data) {
    
    if(_is_worked == false) return 0;

    TCPSegment seg;
    size_t write_len = _sender.stream_in().write(data);

    //也就是如果_receiver已经收到ack了的话，要把段的ack置为1, 至于序列号之类的, tcpsender自己会完成的
    if(_receiver.ackno().has_value()) {
        seg.header().ack = true;

        update_ackno_wdsz_from_rcvr();
        seg.header().ackno = _ackno;
        seg.header().win = _window_sz;
    }

    //放入其中让sender之后从中读取
    _segments_out.push(seg);
    return write_len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {

    //把经过的时间传给sender
    _sender.tick(ms_since_last_tick);
    _time_since_last_segment_received += ms_since_last_tick;

    //相邻重传次数超过规定的次数的话就发rst段中断连接
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _need_send_rst = true;
        dispose_rst();
    }

    //在 TimeWait 的状态 如果距离收到上一个包的时间超过 10 rt_timeout 则断开连接
    if(_tcpstate == TCPState::State::TIME_WAIT && _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
        _is_worked = false;
    }
}

//将_sender_ended置为1
void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();// 中断发送器的输入流
    _sender_ended = true;
}


void TCPConnection::connect() {
    //发送一个 syn报
    _sender.fill_window();
    _tcpstate = TCPState::State::SYN_SENT;
    _is_worked = true;
}



TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n"; // tcp连接还没有断开

            // Your code here: need to send a RST segment to the peer
            _need_send_rst = true;
            dispose_rst();
           // _sender.send_empty_segment(1, 1);
            _tcpstate = TCPState::State::RESET;

        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

//从 receiver 里获取 ackno 和 window_sz
void TCPConnection::update_ackno_wdsz_from_rcvr() {
    if(_receiver.ackno().has_value()) {
        _ackno = _receiver.ackno().value();
        _window_sz = _receiver.window_size();
    }
}

//处理 rst 包的相关内容
void TCPConnection::dispose_rst() {
    // 如果需要的话，发送一个rst包，只有 rst 字段是1的空包
    if(_need_send_rst) {
        TCPSegment rst_seg;
        rst_seg.header().rst = true;
        _segments_out.push(rst_seg);
        _need_send_rst = false;
    }
    //将两个字节流都设成 error 状态
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    //连接不再进行
    _is_worked = false;
}
//发送 rst 报和接收到rst报的时候要立刻断开连接，不管数据有没有传输完

void TCPConnection::send_ack() {
    update_ackno_wdsz_from_rcvr();
    TCPSegment ack_seg;
    ack_seg.header().ack = true;
    ack_seg.header().ackno = _ackno;
    ack_seg.header().win = _window_sz;
    _segments_out.push(ack_seg);
}