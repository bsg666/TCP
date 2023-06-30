#include "stream_reassembler.hh"

#include <string>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {
    // printf("capacity: %zu\n", _capacity);
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof = true;
        eof_idx = index + data.length();
    }
    string s = data;
    try_write(index, s); // 尝试写入字节流，不一定成功
    if (_eof && first_unassembled() >= eof_idx) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _buf.size(); }

bool StreamReassembler::empty() const { return _buf.empty(); }

int StreamReassembler::try_write(size_t index, string &data) {
    size_t old_first_unassembled = _first_unassembled;

    // 先对data处理，能写入写入，能缓存缓存，重复的跳过，当前不能接受的丢弃
    for (size_t i = 0; i < data.length(); ++i) {
        if (index + i >= first_unacceptable()) { // 超出可缓存的范围,丢弃
            break;
        }
        if (index + i < _first_unassembled) {   // 之前已经写到字节流的
            continue;
        } else if (index + i == _first_unassembled) {   // 可以写入字节流，先暂存, 等会一次性写
            assembled_s += data[i];
            _first_unassembled++;
        } else {    // 等会可以写入字节流, 先存到缓冲区
            _buf.insert(pair<size_t, char>(index + i, data[i]));
        }
    }

    for (size_t i = _first_unassembled; i < first_unacceptable(); ++i) {
        if (_buf.find(i) != _buf.end()) {
            assembled_s += _buf.at(i);
            _first_unassembled++;
        } else {    // 升序的，找不到则break
            break;
        }
    }
    size_t n = _output.write(assembled_s);
    assembled_s = assembled_s.substr(n, assembled_s.length()); // 可能没写完

    for (size_t i = old_first_unassembled; i < _first_unassembled; ++i) { // 去除已经写到字节流的
        if (_buf.find(i) != _buf.end()) {
            _buf.erase(i);
        }
    }
    return 0;
}
size_t StreamReassembler::first_unacceptable() const { return first_unread() + _capacity; }
size_t StreamReassembler::first_unread() const { return _output.bytes_read(); }
size_t StreamReassembler::capacity() const{ return _capacity; }
bool StreamReassembler::eof() const{
    bool result = _eof && first_unassembled() >= eof_idx;
    if (result) {
        // printf("receiver end!~~!!\n\n");
    }
    return result;
}
size_t StreamReassembler::first_unassembled() const { return _first_unassembled; }