#include "tcp_receiver.hh"
#include "tcp_state.hh"

#include <cassert>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

/**
 *  \brief 当前 TCPReceiver 大体上有三种状态， 分别是
 *      1. LISTEN，此时 SYN 包尚未抵达。可以通过 _isn是否有值来判断是否在当前状态
 *      2. SYN_RECV, 此时 SYN 抵达。只能判断当前不在 1、3状态时才能确定在当前状态
 *      3. FIN_RECV, 此时 FIN 抵达。可以通过 ByteStream end_input 来判断是否在当前状态
 */

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (_isn.has_value() && seg.header().syn && _isn.value() != seg.header().seqno) { // 进攻式编程
        printf("重复的syn\n");
        assert(0);
    }
    if (!_isn.has_value()) { // _isn 没有值，说明还在 LISTEN 状态
        if (seg.header().syn) { // TCP 报文头部的 SYN 标志位为true, 收到 SYN 包
            _isn = seg.header().seqno;
            // SYN 包中的 payload 不能被丢弃, 不过 SYN 包一般不会放数据，后面再看
            _reassembler.push_substring(seg.payload().copy(), 0, seg.header().fin);
            update_ack_no();
        }
        return; // SYN包之前的数据包必须全部丢弃
    }
    // SYN_RECV 状态, 如果发来的是 FIN 包, 则关闭 _reassembler的_output字节流
    uint64_t  abs_seqno = unwrap(seg.header().seqno, _isn.value(), _reassembler.first_unassembled());
    _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, seg.header().fin);
    update_ack_no();
}

optional<WrappingInt32> TCPReceiver::ackno() const { return _ackno;}

size_t TCPReceiver::window_size() const { return _reassembler.first_unacceptable() -  _reassembler.first_unassembled(); }

void TCPReceiver::update_ack_no() {
    // 如果当前处于 FIN_RECV 状态，则还需要加上 FIN 标志长度
    if (_reassembler.stream_out().input_ended()) {
        _ackno = wrap(_reassembler.first_unassembled() + 2, _isn.value());
    } else {
    // 如果不在 LISTEN 状态，则 ackno 还需要加上一个 SYN 标志的长度, 相当于告诉对面收到了SYN请求
    // SYN、FIN都只是TCP头部的标志位，真正往字节流写的是payload(载荷、数据)，所以得 +1, +2
    // 而SYN包（前两次握手）一般都不携带数据，要告诉对面我们收到这个SYN包（建立连接的请求，SYN标志为true的TCP报文）,向对面发送一个ACK包，ackno = syn(_isn) + 1
        _ackno = wrap(_reassembler.first_unassembled() + 1, _isn.value());
    }

}

// state,可以用于调试
string TCPReceiver::state() {
    if (_reassembler.stream_out().error()) {
        return TCPReceiverStateSummary::ERROR;
    }
    if (_isn.has_value()) {
        return TCPReceiverStateSummary::SYN_RECV;
    }
    if (_reassembler.stream_out().input_ended()) {
        return TCPReceiverStateSummary::FIN_RECV;
    }
    return TCPReceiverStateSummary::LISTEN;
}