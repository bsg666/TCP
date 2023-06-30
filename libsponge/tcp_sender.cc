#include "tcp_sender.hh"

#include "tcp_config.hh"

#include "tcp_state.hh"

#include <random>
#include <cassert>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {
        _retransmission_timeout = retx_timeout;
    }

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _abs_ackno; }

void TCPSender::fill_window() {
    if (_fin_sent) { // 如果已经发送了fin，直接返回
        return;
    }
    while (bytes_in_flight() < _window_size) {
        TCPSegment seg = TCPSegment();
        // 判断是否要发送 syn 
        if (!_syn_sent) {
            seg.header().syn = true;
            seg.header().seqno = _isn;
            _syn_sent = true;
        }
        // 从字节流中读 len 个字节塞到TCP报文中
        size_t len = min(_window_size - bytes_in_flight() - seg.length_in_sequence_space(), TCPConfig::MAX_PAYLOAD_SIZE);
        seg.header().seqno = wrap(_next_seqno, _isn);
        seg.payload() = _stream.read(len);

        // 判断当前是否可以发送 fin 
        if (!_fin_sent && _stream.eof()) { // 字节流终止了，可以发送 fin 了
            // 但是要保证接收方接收到的字节(已经发了但没收到确认的 + 现在要发的)不能超过它的窗口大小
            if (bytes_in_flight() + seg.length_in_sequence_space() < _window_size) { // 加了 FIN 之后最多相等，不能超过
                seg.header().fin = true;
                _fin_sent = true;
            }
        }

        // 发送 + "缓存"
        if (seg.length_in_sequence_space() > 0) { // 只有传递一些数据的网段才被追踪（包括SYN和FIN），缓存起来（实际上是采用智能指针、引用计数的只读字符串），一个空的ACK不需要被记住，也不用重传
            _segment_vector.push_back(seg);
            _segments_out.push(seg);
            _next_seqno += seg.length_in_sequence_space();
        }
        else {
            break;
        }

    }


}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
// 从 tcp_receiver 中获取
int TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (!_syn_sent) {
        return -1;
    }
    // When filling window, treat a '0' window size as equal to '1' but don't back off RTO
    // 零窗口探测，当接收方的接收窗口为0时，每隔一段时间，发送方会主动发送探测包(1字节)，通过迫使对端响应来得知其接收窗口有无打开。
    if (window_size == 0) {
        _window_size = 1;
        _zero_window = true;
    } else {
        _window_size = window_size;
        _zero_window = false;
    }
    uint64_t abs_ackno = unwrap(ackno, _isn, _abs_ackno);
    if (abs_ackno > _abs_ackno && abs_ackno <= _next_seqno) { //只有收到最新的ackno才更新
        _ackno = ackno;
        _abs_ackno = abs_ackno;
    } else {
        return 0;
    }
    // 每次收到ack就重置重传超时时间(初始的)、重传次数、定时时间
    _retransmission_timeout = _initial_retransmission_timeout;
    _consecutive_retransmissions = 0;
    _ticks = 0;

    // 更新状态
    if (!_syn_acked && _syn_sent && _next_seqno > bytes_in_flight() && !_stream.eof()) {
        _syn_acked = true;
    }
    if (!_fin_acked && _fin_sent && _stream.eof() && _abs_ackno == _stream.bytes_read() + 2 && bytes_in_flight() == 0) {
        _fin_acked = true;
        _segment_vector.clear();
    }

    // delete, 收到确认的不用追踪了
    vector<TCPSegment> segs = vector<TCPSegment>();
    for (const auto &seg : _segment_vector) {
        uint64_t seqno = unwrap(seg.header().seqno, _isn, _abs_ackno);
        if (seqno + seg.length_in_sequence_space() > _abs_ackno) {
            segs.push_back(seg);
        }
    }
    _segment_vector = segs;
    return 0;


}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { // 参数是自上次调用该方法以来已经过了多少毫秒
    if (bytes_in_flight() == 0) { // 全都收到确认了，不用重传了，直接返回
        if (!_segment_vector.empty()) {
            printf("!_segment_vector.empty()\n");
            assert(0);
        }
        _ticks = 0;
        return;
    }
    if (_segment_vector.empty()) { // 追踪表为空了，报错
        printf("_segment_vector.empty()\n");
        assert(0);
    }
    _ticks += ms_since_last_tick;
    if (_ticks >= _retransmission_timeout) {
        _ticks = 0;
        _segments_out.push(_segment_vector[0]);
        if (!_zero_window) { // 零窗口不用“指数回退”
            // When filling window, treat a '0' window size as equal to '1' but don't back off RTO
            _retransmission_timeout *= 2; // 超时重传时间 x 2, 拥塞控制
        }
        _consecutive_retransmissions++;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}

std::string TCPSender::state() {
    if (_stream.error()) {
        return TCPSenderStateSummary::ERROR;
    }
    if (!_syn_sent) {
        return TCPSenderStateSummary::CLOSED;
    }
    if (!_syn_acked) {
        return TCPSenderStateSummary::SYN_SENT;
    }
    if (!_fin_sent) {
        return TCPSenderStateSummary::SYN_ACKED;
    }
    if (!_fin_acked) {
        return TCPSenderStateSummary::FIN_SENT;
    }
    return TCPSenderStateSummary::FIN_ACKED;
}

bool TCPSender::fin_acked() const { return _fin_acked; }
bool TCPSender::syn_sent() const { return _syn_sent; }
