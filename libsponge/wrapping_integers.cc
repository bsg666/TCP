#include "wrapping_integers.hh"
#include <assert.h>
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
// 64位转换成32位，截断即可，即保留低位的32位
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {


return WrappingInt32{isn + uint32_t(n)}; }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    WrappingInt32 wrapCheckpoint = wrap(checkpoint, isn);
    int32_t diff = n - wrapCheckpoint;
    // expectation : 0xffffffff -> 0x00000000ffffffff
    // truth : uint64_t(ffffffff) -> ffffffffffffffff 因为是有符号整形，所以代表的是-1（补码），类型转换要保证值不变，高位全部补1

    if (diff < 0 && checkpoint + diff > checkpoint) {
        return checkpoint + uint32_t(diff); // 如果先转换成无符号，就没有问题了
    }
    if (diff > 0 && checkpoint + diff < checkpoint) { // 进攻式编程，出现这种情况直接报错，到时候再处理
        printf("unwrap: ????\n");
        assert(0);
    }
    return checkpoint + diff;
}
