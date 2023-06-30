#include "tcp_connection.hh"

#include <cassert>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_now - _last_segment_received_timestamp; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_active) {
        return;
    }
    // rst
    if (seg.header().rst) {
        close();
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }
    // _receiver接受seg
    _receiver.segment_received(seg);
    _last_segment_received_timestamp = _time_now;

    // keep-alive
    if (_receiver.ackno().has_value() && seg.length_in_sequence_space() == 0 &&
        seg.header().seqno == _receiver.ackno().value() - 1) {
            _sender.send_empty_segment(); // 这里只是放到sender的segment_out队列里
            // 在发送segment之前，
            // TCPConnection会读取TCPReceiver中关于ackno和window_size相关的信息。
            // 如果当前TCPReceiver里有一个合法的ackno，
            // TCPConnection会更改TCPSegment里的ACK标志位，
            // 将其设置为真。
            // "数据捎带ACK",也叫延时确认(delay ACK),在接收到segment时并不立即发送ACK segment(0字节),而是在发送payload的segment（有数据）时捎带上ACK
            fill_window();
            return;
    }
    // 如果ACK标志位为真，通知TCPSender有segment被确认
    if (seg.header().ack) {
        if (_sender.ack_received(seg.header().ackno, seg.header().win) < 0)
            return;
    }

    // 确保至少给这个segment回复一个ACK
    _sender.fill_window(); // 放到sender的队列
    size_t cnt = fill_window(); // 捎带ack和win, 放到connection的队列,返回发送的报文个数
    if (cnt == 0 && seg.length_in_sequence_space() > 0) { // 没有捎带的话，则发送空的 ACK 报文
        _sender.send_empty_segment();
        fill_window();
    }

    // 出向字节流还没有到EOF的时候，入向stream就关闭了字节流, 就不需要linger了，“被动关闭”
    if (!_sender.stream_in().eof() && _receiver.stream_out().input_ended()) {
        _linger_after_streams_finish = false;
    }
    if (_sender.fin_acked() && _receiver.stream_out().input_ended()) {
        if (!_linger_after_streams_finish) {
            close();
        }
    }

}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) { // 暴露给应用层的接口, 在这里发送
    if (!_active) {
        return 0;
    }
    size_t len = _sender.stream_in().write(data);
    _sender.fill_window();
    fill_window();
    return len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active) {
        return;
    }
    _sender.tick(ms_since_last_tick); // 可能会发送数据(超时重传), 放到sender的队列
    fill_window(); // 从sender的队列pop到conn的队列
    _time_now += ms_since_last_tick;
    // 连续重传次数超过MAX_RETX_ATTEMPTS, 终止连接，发送rst
    if (_active && _sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_rst_segment();
        return;
    }
    if (_sender.fin_acked() && _receiver.stream_out().input_ended()) {
        if (_linger_after_streams_finish) {
            if (_time_now >= _last_segment_received_timestamp + 10 * _cfg.rt_timeout) {
                close();
            }
        }
    }

}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    // 在输入流结束后，必须立即发送 FIN
    _sender.fill_window();
    fill_window();
}

void TCPConnection::connect() {
    if (!_active) {
        return;
    }
    // 第一次调用 _sender.fill_window 将会发送一个 syn 数据包
    _sender.fill_window();
    fill_window();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            send_rst_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

// 发送数据，捎带 ACK 和 window_size
size_t TCPConnection::fill_window() { 
    size_t cnt = _sender.segments_out().size();
    while (!_sender.segments_out().empty()) { // 把sender的segments_out队列的放到connection的队列
        TCPSegment seg = _sender.segments_out().front();
        if (_receiver.ackno().has_value()) { // 捎带上 ACK 和 window_size
            seg.header().ackno = _receiver.ackno().value();
            seg.header().ack = true;
        }
        seg.header().win = _receiver.window_size() > UINT16_MAX ? UINT16_MAX : _receiver.window_size();
        _segments_out.push(seg);
        _sender.segments_out().pop();
    }
    return cnt;
}

// 发送rst
void TCPConnection::send_rst_segment() {
    while (!_sender.segments_out().empty()) {
        _sender.segments_out().pop();
    }
    while (!_segments_out.empty()) {
        _segments_out.pop();
    }
    _sender.send_empty_segment();
    TCPSegment seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    seg.header().rst = true;
    seg.header().ackno = _receiver.ackno().value();
    _segments_out.push(seg);
    _active = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    
}

void TCPConnection::close() {
    // 清空队列
    while(!_sender.segments_out().empty()) {
        _sender.segments_out().pop();
    }
    while (!_segments_out.empty()) {
        _segments_out.pop();
    }
    _active = false;
}