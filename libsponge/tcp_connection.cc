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

    //如果收到了一个rst段，就终止连接 (tcp不会给ack段发ack)
    if(seg.header().rst) {
        _need_send_rst = false;
        dispose_rst();
        return;
    }

    _time_since_last_segment_received = 0;

    //把收到的段传给reciver, 对fin段的处理也会直接交给receiver
    _receiver.segment_received(seg);

    //如果收到的段的ack字段是1，那么它的确认号有效，说明sender发出的段可能有新的被确认的部分
    if(seg.header().ack) { 
        _sender.ack_received(seg.header().ackno, seg.header().win);
        dispose_segment_out();
        // //如果正在发送 keep-alive 用的ack包，现在收到了新的包，应该停止这个动作
        // if(_need_send_ack == true) {
        //     _need_send_ack = false;
        // }
    }


    //如果作为服务端收到了一个syn包，就要返回给客户端一个 syn+ack 的包
    if(TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED &&
     TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV) {
        connect();
    }

    //如果这个段占用了序列号，也就是有字节，也就是不是ack空包，那么我们需要给对等体回复一个ack段
    if(seg.length_in_sequence_space()) {
        send_ack();
    }


    // keep-alive 段的 seqno 会是不合法的，且它会是一个不占用序列号的空段,下面是lab文档里直接搬下来的对 keep-alive 的处理
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) && seg.header().seqno == _receiver.ackno().value() - 1) {
        send_ack();
    }


    //服务端发出fin，收到ack后断开连接 (对于服务端的挥手断开处理)
    if(TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
     TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
      _linger_after_streams_finish == false) {
        _is_worked = false;
        return;
    }

    //如果出向字节流还没eof，入向字节流就关闭了连接，把_linger设为0
    if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV && TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED) {
        _linger_after_streams_finish = false;
    }
}


bool TCPConnection::active() const { 
    return _is_worked;
}

//超过容量的部分是直接丢弃还是怎么存下来后面再传
size_t TCPConnection::write(const string &data) {
    //TCPSegment seg;
    size_t write_len = _sender.stream_in().write(data);
    _sender.fill_window();
    dispose_segment_out();
    return write_len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {

    //把经过的时间传给sender
    _sender.tick(ms_since_last_tick);
    dispose_segment_out();
    _time_since_last_segment_received += ms_since_last_tick;

    //相邻重传次数超过规定的次数的话就发rst段中断连接
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _need_send_rst = true;
        dispose_rst();
        return;
    }

    // 在 TimeWait 的状态 如果距离收到上一个包的时间超过 10 rt_timeout 则断开连接
    if(TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
     TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
      _time_since_last_segment_received >= 10 * _cfg.rt_timeout && _linger_after_streams_finish == true) {
        _is_worked = false;
        _linger_after_streams_finish = false;
        return;
    }
    //
    if(TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV && _linger_after_streams_finish == false) {
        _is_worked = false;
        return;
    }
}

void TCPConnection::connect() {
    //第一次调用 fill_wd 发送一个 syn报
    _sender.fill_window();
    dispose_segment_out();
    _is_worked = true;
}



TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n"; // tcp连接还没有断开

            // Your code here: need to send a RST segment to the peer
            _need_send_rst = true;
            dispose_rst();

        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

// //从 receiver 里获取 ackno 和 window_sz
// void TCPConnection::update_ackno_wdsz_from_rcvr() {
//     if(_receiver.ackno().has_value()) {
//         _ackno = _receiver.ackno().value();
//         _window_sz = _receiver.window_size();
//     }
// }

//处理 rst 包的相关内容
void TCPConnection::dispose_rst() {
    // 如果需要的话，发送一个rst包，只有 rst 字段是1的空包
    if(_need_send_rst) {
        TCPSegment rst_seg;
        rst_seg.header().rst = true;
        _sender.segments_out().push(rst_seg);
        dispose_segment_out();
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
    TCPSegment ack_seg;
    ack_seg.header().ack = true;
    _sender.segments_out().push(ack_seg);
    dispose_segment_out();
}

//对segment_out中的段进行处理，加上ackno和window_sz, 每次往_sender的队列里放入数据后都要调用这个函数
void TCPConnection::dispose_segment_out() {
    while(!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        //如果接收端的ackno有值
        if(_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }

        _segments_out.push(seg);
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    dispose_segment_out();
}

//不管我们发送的是有数据的包还是空的ack包，我们都要给我们发送的报文段一个合法的ackno

//客户端给服务端发fin，服务端收到后返回ack，服务端给客户端发fin，客户端收到后发ack并timewait，服务端收到ack后断开，没收到则重发fin(sender:finack,receiver:finrcv)
//