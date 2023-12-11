//
// Created by eckha on 26.11.2023.
//

#ifndef B15_RECEIVINGQUEUE_H
#define B15_RECEIVINGQUEUE_H

uint8_t calculateCRCXOR(const uint8_t *data, size_t length);
uint8_t getNextByte(std::deque<uint8_t> &receivingQueue);
void readTextInput(std::deque<uint8_t> &receivingQueue, bool &messageReceived);

#endif //B15_RECEIVINGQUEUE_H
