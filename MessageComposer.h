//
// Created by eckha on 26.11.2023.
//

#ifndef B15_MESSAGECOMPOSER_H
#define B15_MESSAGECOMPOSER_H

#include <vector>
//Message I
std::vector<uint8_t> createFileTransferHandshake(int blockAmount);
//Message II
std::vector<uint8_t> createBlockRepeatRequest(uint32_t indexStart);
//Message III
std::vector<uint8_t> createFileBlock(std::vector<unsigned char> &rawFileData, size_t blockSize, int index, int blockAmount);
//Text-Protocol
uint8_t* createTextBlock(std::string &text);
#endif //B15_MESSAGECOMPOSER_H
