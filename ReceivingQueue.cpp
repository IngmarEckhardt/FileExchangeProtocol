#include <queue>
#include <thread>
#include <chrono>
#include "MessageToConsolePrinter.h"
#include "ControlCharacter.h"
#include <iostream>
#include "ReceivingQueue.h"

uint8_t calculateCRCXOR(const uint8_t *data, size_t length) {
    uint8_t crc = 0;

    // Iterate over the data array
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
    }

    return crc;
}

bool waitForQueueElements(std::deque<uint8_t> &receivingQueue, int numElements, int maxAttempts) {
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        if (receivingQueue.size() >= numElements) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1 << attempt));
    }
    return false;
}


uint8_t getNextByte(std::deque<uint8_t> &receivingQueue) {

    if (!waitForQueueElements(receivingQueue, 2, 4)) {
        // Still not enough elements in the queue to form a complete byte
        if (!receivingQueue.empty()) {
            // Delete the last remaining element
            receivingQueue.pop_back();
        }
        return 0;
    }

    uint8_t upperHalfByte = receivingQueue.front();
    receivingQueue.pop_front();

    uint8_t lowerHalfByte = receivingQueue.front();
    receivingQueue.pop_front();

    /** ESC-Character als ganzes Byte löschen indem das nächste genommen wird */
    if ((upperHalfByte == ESC1high && lowerHalfByte == ESC1low) ||
        (upperHalfByte == ESC2high && lowerHalfByte == ESC2low)) {

        if (!waitForQueueElements(receivingQueue, 2, 4)) {
            // Still not enough elements in the queue to form a complete byte
            if (!receivingQueue.empty()) {
                // Delete the last remaining element
                receivingQueue.pop_back();
            }
            return 0;
        }


        upperHalfByte = receivingQueue.front();
        receivingQueue.pop_front();

        lowerHalfByte = receivingQueue.front();
        receivingQueue.pop_front();

    } else if (lowerHalfByte == ESC1high || lowerHalfByte == ESC2high) {
        uint8_t nextLowerHalfByte;
        /** auch ESC-Character die zwischen zwei identische Halbbytes eingeschoben wurden werden gelöscht. Wir betrachten
         * das nächst*/
        if (waitForQueueElements(receivingQueue, 1, 3)) {
            nextLowerHalfByte = receivingQueue.front();
        }
        if ((lowerHalfByte == ESC1high && nextLowerHalfByte == ESC1low) ||
            (lowerHalfByte == ESC2high && nextLowerHalfByte == ESC2low)) {

            if (!waitForQueueElements(receivingQueue, 2, 4)) {
                // Still not enough elements in the queue to form a complete byte
                if (!receivingQueue.empty()) {
                    // Delete the last remaining element
                    receivingQueue.pop_back();
                }
                return 0;
            }


            receivingQueue.pop_front();
            lowerHalfByte = receivingQueue.front();
            receivingQueue.pop_front();
        }
    }

    return (upperHalfByte << 4) | lowerHalfByte;
}







void readTextInput(std::deque<uint8_t> &receivingQueue, bool &messageReceived) {
    std::vector<uint8_t> receivedBytes;
    uint8_t nextCharacter = 0;
    // Wait for the Start of Text (STX) character
    if (!receivingQueue.empty()) {
        nextCharacter = getNextByte(receivingQueue);
    }

    if (nextCharacter == SOH) {
        // Check if the next character is the Start of Header (SOH) character
        if (!receivingQueue.empty()) {
            uint8_t secondCharacter = getNextByte(receivingQueue);
            if (secondCharacter == STX) {
                receivedBytes.clear();
                receivedBytes.push_back(SOH);
                receivedBytes.push_back(STX);

                // Read all bytes until the End of Text (ETX) character is encountered
                uint8_t currentByte;

                do {
                    if (!receivingQueue.empty()) {
                        currentByte = getNextByte(receivingQueue);
                        receivedBytes.push_back(currentByte);
                    }
                } while (currentByte != ETX);
                uint8_t bcc = getNextByte(receivingQueue);
                uint8_t calculatedBcc = calculateCRCXOR(receivedBytes.data(), receivedBytes.size());


                if (bcc != calculatedBcc) {
                    std::cout << "Invalid Block Check Character (BCC)" << std::endl;
                    std::cout << "Expected " << bcc << " but was " << calculatedBcc << std::endl;
                }

                // Process the received packet using printTextFromMessageBlockToConsole
                printTextFromMessageBlockToConsole(receivedBytes.data(), receivedBytes.size(), messageReceived);
            } else {
                std::cout << "Invalid Start of Text character." << std::endl;
            }
        } else {
            std::cout << "Only SOH in Queue." << std::endl;
        }
    } else {
        std::cout << "Invalid Start of Header (SOH) character." << std::endl;
    }
}