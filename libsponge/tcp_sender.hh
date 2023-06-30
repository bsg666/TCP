#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <map>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    // 需要发送的段放到队列里，我们不关心如何去发
    std::queue<TCPSegment> _segments_out{}; 

    //! retransmission timer for the connection
    // 超时重传时间
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    // 下一个要发送的seqno
    uint64_t _next_seqno{0};

    // 收到的window_size, 初始化为1
    uint64_t _window_size{1};

    WrappingInt32 _ackno{0};

    // 收到的ackno
    uint64_t _abs_ackno{0};

    // 几种状态
    
    // syn已经发送
    bool _syn_sent{false};

    // 收到对方对syn的确认
    bool _syn_acked{false};
    
    // fin已经发送
    bool _fin_sent{false};

    // 收到对方对fin的确认
    bool _fin_acked{false};
    
    // 定时时间
    size_t _ticks{0};

    // 超时重传时间
    size_t _retransmission_timeout{0};

    std::vector<TCPSegment> _segment_vector{std::vector<TCPSegment>()};

    // 连续重传次数
    size_t _consecutive_retransmissions{0}; 

    // 零窗口探测
    bool _zero_window{false};



  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    int ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    // 告诉TCPSender时间正在流逝, 看看是否需要超时重传
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    // 发送出去但是没有收到确认的字节数量
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}

    std::string state();

    bool fin_acked() const;

    bool syn_sent() const;
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
