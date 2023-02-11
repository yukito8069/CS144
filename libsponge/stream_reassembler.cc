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
       // cout << index  << " " << data.size() << "$$$\n";
        _lst_idx = index + data.size();
    }


    if(index < _fst_unrsm) {
        if(index + data.length() < _fst_unrsm) {
            return;
        }
        size_t writeLength = index + data.length() - _fst_unrsm;
        //cout << data.substr(_fst_unrsm - index, writeLength) << " str1\n";
        push_substring(data.substr(_fst_unrsm - index, writeLength), _fst_unrsm, 0);
        return;
    }
    if(index + data.length() > _fst_unread + _capacity) {
        if(index > _fst_unread + _capacity) {
            //cout << "!!\n";
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
       // cout << data << "rsmstr2\n";
        for (size_t i = 0; i < data.length(); i++) {
            buffer[i + index] = data[i];
          //  cout << i + index << " " << data[i] << "%%%\n";
        }
    }
    string str = "";
    _fst_unrsm = _output.bytes_written();//刚刚加的
    while(buffer.find(_fst_unrsm) != buffer.end()) {
        str += buffer[_fst_unrsm];
        buffer.erase(_fst_unrsm);
        _fst_unrsm++;
    }
    //cout << str << " strr\n";
    _output.write(str);

    //std::cout << _fst_unrsm << "^^^\n";
    _fst_unread = _output.bytes_read();
    _fst_unrsm = _output.bytes_written();

    //std::cout << _fst_unrsm << " " << _lst_idx << "***\n";
    if(_eof && _lst_idx == _fst_unrsm) {
        _output.end_input();
        buffer.clear();
    }

}
size_t StreamReassembler::getFstUaccp() {
    return _fst_unread + _capacity;
}

size_t StreamReassembler::getFstUaccp() const{
    return _fst_unread + _capacity;
}
size_t StreamReassembler::unassembled_bytes() const { return buffer.size(); }

bool StreamReassembler::empty() const { return buffer.empty();}

size_t StreamReassembler::getFstUrsm() {
    _fst_unrsm = _output.bytes_written();
    return _fst_unrsm;
}

size_t StreamReassembler::getFstUrsm() const {
    return _output.bytes_written();
}