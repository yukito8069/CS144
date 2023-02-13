#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstddef>
#include <optional>
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {

    //如果是syn
    if(seg.header().syn) {
        _isn = seg.header().seqno; // 初始化第一个接收到的序列号
        _reassembler.push_substring(seg.payload().copy(), 0, seg.header().fin);//注意，对第一个包要特判，因为它下标开头的那个是syn，而不是捎带的数据的第一个开头
        updateACK(); //更新_ackno
        return;
    }

    // 如果在收到syn之前收到别的报文段就丢掉
    if(_ackno.has_value() == false) {
        return;
    }

    //将senqo转化成absolute seqno
    uint64_t isn = unwrap(seg.header().seqno, _isn.value(), _reassembler.getFstUrsm()) - 1;
    _reassembler.push_substring(seg.payload().copy(), isn, seg.header().fin);
    updateACK();
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    std::optional<WrappingInt32> op(_ackno);
    return op;
}

size_t TCPReceiver::window_size() const { return getFstUaccp() - _reassembler.getFstUrsm();}

void TCPReceiver::updateACK() {
    _ackno = wrap(_reassembler.getFstUrsm() + _reassembler.stream_out().input_ended() + 1, _isn.value());
}