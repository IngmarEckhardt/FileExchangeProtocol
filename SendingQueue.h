//
// Created by eckha on 26.11.2023.
//
#include <deque>
#include <cstdint>

#ifndef B15_SENDINGQUEUE_H
#define B15_SENDINGQUEUE_H
void putByteAsHalfBytesInSendingQueue(uint8_t &byteValue, std::deque<uint8_t> &deque);
#endif //B15_SENDINGQUEUE_H
