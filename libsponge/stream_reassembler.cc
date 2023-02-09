#include "stream_reassembler.hh"
#include <cstddef>
#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    stream_out();
    _fst_unread = _output.bytes_read();
    _fst_unrsm = _output.bytes_written();

    if(eof) {
        _eof = true;
        _lst_idx = index + data.size();
    }


    if(index < _fst_unrsm) {
        if(index + data.length() < _fst_unrsm) {
            return;
        }
        size_t writeLength = index + data.length() - _fst_unrsm;
        push_substring(data.substr(_fst_unrsm - index, writeLength), _fst_unrsm, 0);
        return;
    }
    if(index + data.length() > _fst_unread + _capacity) {
        if(index > _fst_unread + _capacity) {
            return;
        }
        size_t writeLength = _fst_unread + _capacity - index;
        push_substring(data.substr(0, writeLength), index, false);
        return;
    }

    if(index == _fst_unrsm) {
        string str = "";
        for (size_t i = 0; i < data.length(); i++) {
            if(buffer.find(i + index) != buffer.end()) {
                buffer.erase(i + index);
            }
            str += data[i];
        }
        _fst_unrsm += data.length();
        _output.write(str);
    }
    else {
        for (size_t i = 0; i < data.length(); i++) {
            buffer[i + index] = data[i];
        }
    }
    string str = "";
    while(buffer.find(_fst_unrsm) != buffer.end()) {
        str += buffer[_fst_unrsm];
        buffer.erase(_fst_unrsm);
        _fst_unrsm++;
    }
    _output.write(str);

    stream_out();
    //std::cout << _fst_unrsm << "^^^\n";
    _fst_unread = _output.bytes_read();
    _fst_unrsm = _output.bytes_written();

   // std::cout << _fst_unrsm << "***\n";
    if(_eof && _lst_idx == _fst_unrsm) {
        _output.end_input();
        buffer.clear();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return buffer.size(); }

bool StreamReassembler::empty() const { return buffer.empty();}
