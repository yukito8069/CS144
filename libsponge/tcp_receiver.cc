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
    if(seg.header().syn) {
        _isn = seg.header().seqno;
        _reassembler.push_substring(seg.payload().copy(), 0, seg.header().fin);//注意，对第一个包要特判，因为它下标开头的那个是syn，而不是捎带的数据的第一个开头
        updateACK();
        // if(seg.header().fin) {
        //     _reassembler.stream_out().end_input();
        // }
        return;
    }
   // std::cout << _ackno->raw_value() << "???\n";
    if(_ackno.has_value() == false) {
        return;
    }
    uint64_t isn = unwrap(seg.header().seqno, _isn.value(), _reassembler.getFstUrsm()) - 1;
    //std::cout << isn << "^^^\n";
    _reassembler.push_substring(seg.payload().copy(), isn, seg.header().fin);
    // if(seg.header().fin) {
    //     _reassembler.stream_out().end_input();
    // }
   // cout << seg.payload().copy().length() << "segplen\n";
    updateACK();
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    std::optional<WrappingInt32> op(_ackno);
    return op;
}

//size_t TCPReceiver::window_size() const { return getFstUaccp() - _reassembler.getFstUrsm();}
size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size();}
//size_t TCPReceiver::getFstUrsm() {return _reassembler.getFstUrsm();}

void TCPReceiver::updateACK() {
    _ackno = wrap(_reassembler.getFstUrsm() + _reassembler.stream_out().input_ended() + 1, _isn.value());
}