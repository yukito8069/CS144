#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <cstddef>
#include <cstdint>
#include <set>
#include <queue>
#include <map>

//! \brief The "sender" part of a TCP implementation.
//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    bool _syn_sent = false;
    bool _fin_sent = false;
    bool _window_sz_eq0 = false;
    bool _timer_run = true;
    uint64_t _ackno = 0; //最早的未确认的发送了的段
    size_t _bytes_unack = 0;

    //bool _syn_ack_get = false;
    //std::map<WrappingInt32, TCPSegment> un_ack_mp;//发送了但没收到ack的map
    std::queue<TCPSegment> _un_ack{};//按时间存发送了但未ack的段
    //uint64_t _lf, _rt;
    uint16_t _window_sz = 1; //发送窗口的大小
    size_t _time = 0;//最早的发送了但未确认的报文段距离发送已经过去了多少时间

    size_t _consecutive_retran_cnt = 0;//累计相邻超时次数

    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn; //发送方的初始序列号
    //uint64_t recv_absq = 0;//最后一个收到确认的报文段的序列号
    //WrappingInt32 _unack = _isn; //第一个发送了但未收到确认的报文段的序列号

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{}; // 放进这个队列，把报文段交给网络层

    //! retransmission timer for the connection
    unsigned int _initial_retransmission_timeout; //初始的超时时间
    unsigned int _RTO = _initial_retransmission_timeout;//这个是我自己加的动态的RTO

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream; //我们要发的数据

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0}; //下一个要发送的数据的abseqno

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
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    // 
    void send_empty_segment(bool ack, bool rst);

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
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
    
    bool is_fin_sented() {
        return _fin_sent;
    }
    bool is_fin_sented() const {
        return _fin_sent;
    }

    // void set_ackno_wdsz(WrappingInt32 ackno, uint16_t wdsz);
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH