//
// Created by eckha on 29.11.2023.
//

#ifndef B15_FILETRANSFERHELPER_H
#define B15_FILETRANSFERHELPER_H

#include <deque>
#include <cstdint>
#include <queue>

void startFileTransfer(std::queue<std::vector<uint8_t>> &preSendingQueueData,
                       std::queue<std::vector<uint8_t>> &preSendingQueueCommands, std::deque<uint8_t> &receivingQueue,
                       std::vector<std::vector<uint8_t>> &fileBlocks, bool &escapePressed, bool &safeFile);
#endif //B15_FILETRANSFERHELPER_H
