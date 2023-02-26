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
    //cout << data << "rsmstr\n";
    _fst_unread = _output.bytes_read();
    _fst_unrsm = _output.bytes_written();

    if(eof) {
        _eof = true;
        _lst_idx = index + data.size();
    }


    if(index < _fst_unrsm) {
        if(index + data.length() < getFstUrsm()) {
            return;
        }
        size_t writeLength = index + data.length() - getFstUrsm();
        push_substring(data.substr(getFstUrsm() - index, writeLength), getFstUrsm(), 0);
        return;
    }
    // 如果这个段至少有一部分超过了滑动窗口的范围，会将超过的部分直接丢弃，其余部分存进滑动窗口
    if(index + data.length() > getFstUaccp()) {
        if(index > getFstUaccp()) {
            return;
        }
        size_t writeLength = getFstUaccp() - index;
        push_substring(data.substr(0, writeLength), index, false);
        return;
    }

    if(index == getFstUrsm()) {
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
    _fst_unrsm = _output.bytes_written();//刚刚加的
    while(buffer.find(_fst_unrsm) != buffer.end()) {
        str += buffer[_fst_unrsm];
        buffer.erase(_fst_unrsm);
        _fst_unrsm++;
    }
    _output.write(str);


    if(_eof && _lst_idx == getFstUrsm()) {
        _output.end_input();
        buffer.clear();
    }

}

inline size_t StreamReassembler::getFstUread() {
    return _fst_unread = _output.bytes_read();
}
inline size_t StreamReassembler::getFstUread() const {
    return _fst_unread = _output.bytes_read();
}

size_t StreamReassembler::getFstUaccp() {
    return getFstUread() + _capacity;
}

size_t StreamReassembler::getFstUaccp() const{
    return getFstUread() + _capacity;
}
size_t StreamReassembler::unassembled_bytes() const { return buffer.size(); }

inline bool StreamReassembler::empty() const { return buffer.empty();}

size_t StreamReassembler::getFstUrsm() {
    return _fst_unrsm = _output.bytes_written();
}

size_t StreamReassembler::getFstUrsm() const {
   return _fst_unrsm = _output.bytes_written();
}

bool StreamReassembler::is_eof() {
    return _eof;
}