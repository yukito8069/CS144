#include "byte_stream.hh"
#include <cstddef>
#include <iostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {}

//返回成功存入的字节数
size_t ByteStream::write(const string &data) {
    if(_eof) {
        return 0;
    }
    size_t writeLength = min(data.length(), _capacity - stream.size());
    for (std::size_t i = 0; i < writeLength; i++) {
        stream.push_back(data[i]);
    }
    _writeNum += writeLength;
    return writeLength;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    std::string str = "";
    // for (size_t i = 0; i < len; i++) {
    //     str += stream.at(i);
    // }
    size_t readLength = min(len, stream.size());
    for (size_t i = 1; i <= readLength; i++) {
        str += stream.at(i - 1);
    }
    return str;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t popLength = min(len, stream.size());
    _readNum += popLength;
    for (size_t i = 1; i <= popLength; i++) {
        stream.pop_front();
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string str = "";
    size_t readLength = min(len, stream.size());
    str = peek_output(readLength);
    //_readNum += readLength;
    pop_output(readLength);
    return str;
}

void ByteStream::end_input() {_eof = true;}

bool ByteStream::input_ended() const { return _eof;}

size_t ByteStream::buffer_size() const { return stream.size();}

bool ByteStream::buffer_empty() const { return stream.empty();}

bool ByteStream::eof() const { return _eof && stream.empty(); }

size_t ByteStream::bytes_written() const { return _writeNum;}

size_t ByteStream::bytes_read() const { return _readNum;}

size_t ByteStream::remaining_capacity() const { return _capacity - stream.size();}
