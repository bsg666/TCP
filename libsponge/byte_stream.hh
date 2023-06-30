#ifndef SPONGE_LIBSPONGE_BYTE_STREAM_HH
#define SPONGE_LIBSPONGE_BYTE_STREAM_HH

#include <cstring>
#include <string>
#include <queue>
//! \brief An in-order byte stream.

//! Bytes are written on the "input" side and read from the "output"
//! side.  The byte stream is finite: the writer can end the input,
//! and then no more bytes can be written.
class ByteStream {
  private:

    std::deque<char> _queue;  // char类型双端队列，即字节流（缓冲区）
    size_t _capacity_size;    // 缓冲区容量
    size_t _written_size;     // 已经写的数据的大小
    size_t _read_size;        // 已经读的数据的大小
    bool _end_input;          // 终止写入
    bool _error{};            // 字节流发生错误


  public:
    //! Construct a stream with room for `capacity` bytes.
    ByteStream(const size_t capacity);

    //! \name "Input" interface for the writer
    //!@{

    // 往字节流中写数据，返回写入的大小（不一定等于数据大小）
    size_t write(const std::string &data);

    // 字节流的剩余空间，目前还可以写多少个字节
    size_t remaining_capacity() const;

    // 字节流关闭，不能写入
    void end_input();

    // 字节流遇到错误
    void set_error() { _error = true; }

    //! \name "Output" interface for the reader
    //!@{

    // 查看接下来的 len 个字节，即队头前 len 个
    std::string peek_output(const size_t len) const;

    // 从字节流pop len 个字节
    void pop_output(const size_t len);

    //! Read (i.e., copy and then pop) the next "len" bytes of the stream
    //! \returns a string
    std::string read(const size_t len);

    // 字节流是否关闭
    bool input_ended() const;

    // 字节流是否出错
    bool error() const { return _error; }

    // 当前缓冲区的字节数，即队列的大小
    size_t buffer_size() const;

    // 缓冲区是否为空
    bool buffer_empty() const;

    // 缓冲区是否结束操作，为空并且终止写入
    bool eof() const;

    // 累计写入的字节数
    size_t bytes_written() const;

    // 累计读出的字节数
    size_t bytes_read() const;
};

#endif  // SPONGE_LIBSPONGE_BYTE_STREAM_HH
