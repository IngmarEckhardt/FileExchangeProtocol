#include <string>
#include "MessageComposer.h"
#include "ControlCharacter.h"



// Protocol-message I

std::vector<uint8_t> createFileTransferHandshake(int blockAmount) {
    std::vector<uint8_t> arrayToSend(8);
    std::size_t currentIndex = 0;
    uint8_t crcXOR = 0;

    // Add Start of Header (SOH) character
    arrayToSend[currentIndex++] = SOH;
    crcXOR ^= SOH;
    arrayToSend[currentIndex++] = 0b0;

    uint8_t mostSignificantByte = static_cast<uint8_t>(blockAmount >> 16);
    arrayToSend[currentIndex++] = mostSignificantByte;
    crcXOR ^= mostSignificantByte;
    uint8_t secondSignificantByte = static_cast<uint8_t>(blockAmount >> 8);
    arrayToSend[currentIndex++] = secondSignificantByte;
    crcXOR ^= secondSignificantByte;
    uint8_t leastSignificantByte = static_cast<uint8_t>(blockAmount);
    arrayToSend[currentIndex++] = leastSignificantByte;

    crcXOR ^= leastSignificantByte;
    // Add Start of Text (STX) character
    arrayToSend[currentIndex++] = STX;
    crcXOR ^= STX;

    arrayToSend[currentIndex++] = ETX;
    crcXOR ^= ETX;

    arrayToSend[currentIndex] = crcXOR;

    return arrayToSend;
}

//Protocol-message II
std::vector<uint8_t> createBlockRepeatRequest(uint32_t indexStart) {
    std::vector<uint8_t> arrayToSend(7);
    std::size_t currentIndex = 0;
    uint8_t crcXOR = 0;

    // Add Start of Header (SOH) character
    arrayToSend[currentIndex++] = SOH;
    crcXOR ^= SOH;
    arrayToSend[currentIndex++] = STX;
    crcXOR ^= STX;

    uint8_t mostSignificantByte = static_cast<uint8_t>(indexStart >> 16);
    arrayToSend[currentIndex++] = mostSignificantByte;
    crcXOR ^= mostSignificantByte;
    uint8_t secondSignificantByte = static_cast<uint8_t>(indexStart >> 8);
    arrayToSend[currentIndex++] = secondSignificantByte;
    crcXOR ^= secondSignificantByte;
    uint8_t leastSignificantByte = static_cast<uint8_t>(indexStart);
    arrayToSend[currentIndex++] = leastSignificantByte;
    crcXOR ^= leastSignificantByte;

    arrayToSend[currentIndex++] = ETX;
    crcXOR ^= ETX;

    arrayToSend[currentIndex] = crcXOR;

    return arrayToSend;
}


//Protocol-message III
std::vector<uint8_t> createFileBlock(std::vector<unsigned char> &rawFileData, size_t blockSize, int index, int blockAmount) {
    std::vector<uint8_t> arrayToSend(blockSize + 7);
    std::size_t currentIndex = 0;
    uint8_t crcXOR = 0;

    // Add Start of Header (SOH) character
    arrayToSend[currentIndex++] = SOH;
    crcXOR ^= SOH;

    //einfügen von einem 0 byte um die Unterscheidung von STX zu vereinfachen
    arrayToSend[currentIndex++] = 0b0;

    uint8_t mostSignificantByte = static_cast<uint8_t>(index >> 16);
    arrayToSend[currentIndex++] = mostSignificantByte;
    crcXOR ^= mostSignificantByte;
    uint8_t secondSignificantByte = static_cast<uint8_t>(index >> 8);
    arrayToSend[currentIndex++] = secondSignificantByte;
    crcXOR ^= secondSignificantByte;
    uint8_t leastSignificantByte = static_cast<uint8_t>(index);
    arrayToSend[currentIndex++] = leastSignificantByte;
    crcXOR ^= leastSignificantByte;


    // Add Start of Text (STX) character
    arrayToSend[currentIndex++] = STX;
    crcXOR ^= STX;

    for (uint8_t byte: rawFileData) {
        arrayToSend[currentIndex++] = byte;
        crcXOR ^= byte;
    }

    //Anfüllen des Blocks bis auf 4kByte beim letzten Block
    int emptySpace = blockSize - rawFileData.size();

    while (emptySpace) {
        arrayToSend[currentIndex++] = 0b0;
        emptySpace--;
    }

    //Es wird immer EOB gesendet, bis auf den letzten Block der ein ETX bekommt
    if (index < blockAmount) {
        arrayToSend[currentIndex++] = EOB;
        crcXOR ^= EOB;
    } else {
        arrayToSend[currentIndex++] = ETX;
        crcXOR ^= ETX;
    }
    arrayToSend[currentIndex] = crcXOR;

    return arrayToSend;
}





uint8_t *createTextBlock(std::string &text) {
    uint8_t *arrayToSend = new uint8_t[text.length() + 4];
    std::size_t currentIndex = 0;
    uint8_t crcXOR = 0;

    // Add Start of Header (SOH) character
    arrayToSend[currentIndex++] = SOH;
    crcXOR ^= SOH;

    // Add Start of Text (STX) character
    arrayToSend[currentIndex++] = STX;
    crcXOR ^= STX;

    for (char c: text) {
        // Convert each character to its ASCII value
        uint8_t asciiValue = static_cast<uint8_t>(c);
        arrayToSend[currentIndex++] = asciiValue;
        crcXOR ^= asciiValue;
    }
    arrayToSend[currentIndex++] = ETX;
    crcXOR ^= ETX;

    arrayToSend[currentIndex] = crcXOR;

    return arrayToSend;
}