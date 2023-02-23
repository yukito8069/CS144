#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { 
    //std::cout<<unwrap(_unack, _isn, 0) << "**\n";
    return _bytes_unack;
}//最后的checkpoint是乱填的

void TCPSender::fill_window() {
    if(_fin_sent) return;
    //while(_window_sz > bytes_in_flight()) {
    while(next_seqno_absolute() < _ackno + _window_sz){//下一个要发的在最早的一个未收到确认的到加上窗口大小的值的区间内
        if(_fin_sent) {
            return;
        }
        // if(_fin_sent || (_stream.buffer_empty() && _syn_sent)) {
        //     return;
        // }
        //std::cout << _window_sz << " " << _bytes_unack <<"*\n";
        TCPSegment seg;
        seg.header().seqno = next_seqno();
        if(!_syn_sent) {
            seg.header().syn = true;
            //_syn_sent = true;
        }
        if(next_seqno_absolute() + seg.length_in_sequence_space() > _ackno + _window_sz) break;
        //if(_window_sz < bytes_in_flight() + seg.length_in_sequence_space()) break;
        // size_t len = min(TCPConfig::MAX_PAYLOAD_SIZE, _window_sz + _ackno - 1 - seg.length_in_sequence_space());
        size_t len = min(TCPConfig::MAX_PAYLOAD_SIZE, _window_sz + _ackno - next_seqno_absolute() - seg.length_in_sequence_space());
        //size_t len = min(TCPConfig::MAX_PAYLOAD_SIZE, _window_sz - bytes_in_flight() - seg.length_in_sequence_space());
        seg.payload() = _stream.read(len);
        //cout << _stream.eof() << " " << seg.length_in_sequence_space() << " " << bytes_in_flight() << " " << len << " " << seg.payload().copy() << " " << seg.payload().size() << "&&\n";
        // if(!_fin_sent && _stream.eof() && seg.length_in_sequence_space() + 1 + bytes_in_flight() <= _window_sz) {
        //     seg.header().fin = true;
        //    // _fin_sent = true;
        // }
        if(!_fin_sent && _stream.eof() && _window_sz + _ackno >= seg.length_in_sequence_space() + next_seqno_absolute() + 1) {
            seg.header().fin = true;
        }
        if(seg.length_in_sequence_space() > 0) {
            _next_seqno += seg.length_in_sequence_space();
            _bytes_unack += seg.length_in_sequence_space();
            if(seg.header().syn) {
                _syn_sent = true;
            }
            if(seg.header().fin) {
                _fin_sent = true;
            }
            //std::cout << seg.header().seqno << " " << seg.header().syn << " " << seg.header().fin << " " << seg.payload().copy() << "^^^\n"; 
            _segments_out.push(seg);
            _un_ack.push(seg);
            _timer_run = true;
        }
        else {
            break;
        }
    }
    
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    if(unwrap(ackno, _isn, _next_seqno) > next_seqno_absolute()) {
        return;
    }
    if(_bytes_unack && _ackno < unwrap(ackno, _isn,  _next_seqno)) {
        if(window_size) {//0的时候不能把_RTO置为初始值
            _RTO = _initial_retransmission_timeout;
            _window_sz_eq0 = false;
        }
        else {
            _window_sz_eq0 = true;
        }
        _time = 0;
        _consecutive_retran_cnt = 0;
    }
    else if(_bytes_unack == 0) {
        _timer_run = false;
    }
    //if(_ackno >= unwrap(ackno, _isn, _next_seqno)) return;
    //_consecutive_retran_cnt = 0;
    //_time = 0;
    _ackno = max(_ackno, unwrap(ackno, _isn,  _next_seqno));//更新最新的ackno

    // if(_next_seqno < unwrap(ackno, _isn, _next_seqno)) {
    //     _next_seqno = unwrap(ackno, _isn, _next_seqno);
    // }

    //如果最前面的发送了但没有收到确认的段的末尾的seqno要比收到确认的这个Seqno要小
    //cout << unwrap(_un_ack.front().header().seqno, _isn, _next_seqno) + _un_ack.front().length_in_sequence_space() << " " << unwrap(ackno, _isn, _next_seqno)<< "^$#@\n";
    while(_un_ack.size() && unwrap(_un_ack.front().header().seqno, _isn, _next_seqno) + _un_ack.front().length_in_sequence_space() <= unwrap(ackno, _isn, _next_seqno)) {
        //把它从未确认的字节数里的贡献减掉
        //cout << _un_ack.front().length_in_sequence_space() << " seg_sz\n";
        _bytes_unack -= _un_ack.front().length_in_sequence_space();
        //把它从未确认的队列中移去
        _un_ack.pop();
        // _time = 0;
        // _consecutive_retran_cnt = 0;
    }
    
    // if(_un_ack.size()) {
    //     //如果队首的报文段的一部分被确认了，另一部分没被确认
    //     if(unwrap(ackno, _isn, _next_seqno) > unwrap(_un_ack.front().header().seqno, _isn, _next_seqno)) {
    //         _bytes_unack -= unwrap(ackno, _isn, _next_seqno) -  unwrap(_un_ack.front().header().seqno, _isn, _next_seqno);
    //         _un_ack.front().header().seqno = ackno;
    //         _time = 0;
    //         _consecutive_retran_cnt = 0;
    //     }
    // }
  //      _unack = _un_ack.front().header().seqno;

    
    if(!_stream.eof() || !_fin_sent) {
        //_time = 0;

        _window_sz = max(window_size, uint16_t(1));
        fill_window();

    }
    //cout << _bytes_unack << " " << _timer_run << "???\n";
    //_consecutive_retran_cnt = 0;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if(_timer_run == false) return;
    _time += ms_since_last_tick;
    //cout << _time << " " << _RTO << "是否需要超时重传\n";
    if(_time >= _RTO) {
        _time = 0;
        _segments_out.push(_un_ack.front()); //发送第一个发送了但未被确认的段
        if(_window_sz_eq0 == false){
            _consecutive_retran_cnt++;
            _RTO *= 2;
        }
    }

}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retran_cnt;}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().ack = 1;
    seg.header().seqno = next_seqno();

    _next_seqno += seg.length_in_sequence_space();

    _segments_out.push(seg);
    _un_ack.push(seg);
    // un_ack_mp[seg.header().seqno] = seg;
}