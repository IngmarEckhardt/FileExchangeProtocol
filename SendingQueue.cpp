#include "ControlCharacter.h"
#include "SendingQueue.h"

uint8_t lastNumberSend;

void splitNumber(uint8_t number, uint8_t(&upperNumber), uint8_t(&lowerNumber)) {
    // Zerlege die 8-Bit-Zahl in 4-Bit-Gruppen
    upperNumber = (number >> 4) & 0b1111;
    lowerNumber = (number) & 0b1111;
}

void addToSendingQueue(uint8_t(&upperNumber), uint8_t(&lowerNumber), std::deque<uint8_t> &sendingQueue) {
    if (upperNumber == lastNumberSend && lastNumberSend != 0
    || (lastNumberSend == ESC2high && upperNumber == ESC2low)
    || (lastNumberSend = ESC1high && upperNumber == ESC1low)
    ) {
        // If upperNumber is identical to the last number in the queue
        if (upperNumber != 0b0101) {
            sendingQueue.push_back(ESC1high);
            sendingQueue.push_back(ESC1low);
        } else {
            sendingQueue.push_back(ESC2high);
            sendingQueue.push_back(ESC2low);
        }
    }


    if (upperNumber == lowerNumber) {

        if (upperNumber != 0b0101) {
            sendingQueue.push_back(upperNumber);
            sendingQueue.push_back(ESC1high);
            sendingQueue.push_back(ESC1low);
            sendingQueue.push_back(upperNumber);
        } else {
            sendingQueue.push_back(upperNumber);
            sendingQueue.push_back(ESC2high);
            sendingQueue.push_back(ESC2low);
            sendingQueue.push_back(upperNumber);
        }
    } else{
        sendingQueue.push_back(upperNumber);
        sendingQueue.push_back(lowerNumber);
    }
    lastNumberSend = lowerNumber;
}

void putByteAsHalfBytesInSendingQueue(uint8_t &byteValue, std::deque<uint8_t> &deque) {
    uint8_t upperHalfByte;
    uint8_t lowerHalfByte;
    splitNumber(byteValue, upperHalfByte, lowerHalfByte);

    //falls es ein ESC-Charakter ist wird er zweimal hineingesteckt
    if (byteValue == ESC1 || byteValue == ESC2) {
        addToSendingQueue(upperHalfByte, lowerHalfByte, deque);

    }
    addToSendingQueue(upperHalfByte, lowerHalfByte, deque);
}