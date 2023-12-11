#include "PipeParser.h"


/** Linux*/
std::vector<std::vector<uint8_t>> readBinaryInBlocksFromPipe(int pipeDescriptor, std::size_t blockSize) {
    std::vector<std::vector<uint8_t>> blocks;
    std::vector<uint8_t> currentBlock(blockSize, 0);

    ssize_t bytesRead;
    while ((bytesRead = read(pipeDescriptor, currentBlock.data(), blockSize)) == blockSize) {
        blocks.push_back(currentBlock);
    }

    if (bytesRead == 0) {
        // No more data to read
    } else if (bytesRead < blockSize) {
        currentBlock.resize(bytesRead);
        blocks.push_back(currentBlock);
    } else {
        std::cerr << "Error reading from the pipe. Errno: " << errno << std::endl;
    }

    return blocks;
}

std::vector<std::vector<uint8_t>> readStringInBlocksFromConsole(int consoleDescriptor, std::size_t blockSize) {
    std::vector<std::vector<uint8_t>> blocks;
    std::string input;
    char ch;
    ssize_t bytesRead;

    while ((bytesRead = read(consoleDescriptor, &ch, 1)) > 0) {
        if (ch == '\n' || ch == '\r') {
            if (!input.empty()) {
                std::vector<uint8_t> block(input.begin(), input.end());
                blocks.push_back(block);
                input.clear();
            }
        } else {
            input.push_back(ch);
            if (input.size() == blockSize) {
                std::vector<uint8_t> block(input.begin(), input.end());
                blocks.push_back(block);
                input.clear();
            }
        }
    }

    if (!input.empty()) {
        std::vector<uint8_t> block(input.begin(), input.end());
        blocks.push_back(block);
    }

    if (bytesRead == -1) {
        std::cerr << "Error reading from the console. Errno: " << errno << std::endl;
    }

    return blocks;
}

/** Windows */
std::vector<std::vector<uint8_t>> readBinaryInBlocksFromPipe(HANDLE pipeHandle, std::size_t blockSize) {
    std::vector<std::vector<uint8_t>> blocks;
    std::vector<uint8_t> currentBlock(blockSize, 0);

    DWORD bytesRead;
    while (ReadFile(pipeHandle, currentBlock.data(), static_cast<DWORD>(blockSize), &bytesRead, nullptr) && bytesRead == blockSize) {
        blocks.push_back(currentBlock);
    }

    if (bytesRead == 0) {
        // No more data to read
    } else if (bytesRead < blockSize) {
        currentBlock.resize(bytesRead);
        blocks.push_back(currentBlock);
    } else {
        std::cerr << "Error reading from the pipe. GetLastError: " << GetLastError() << std::endl;
    }

    return blocks;
}

std::vector<std::vector<uint8_t>> readStringInBlocksFromConsole(HANDLE consoleHandle, std::size_t blockSize) {
    std::vector<std::vector<uint8_t>> blocks;
    std::string input;
    char ch;

    while (ReadFile(consoleHandle, &ch, 1, nullptr, nullptr)) {
        if (ch == '\n' || ch == '\r') {
            if (!input.empty()) {
                std::vector<uint8_t> block(input.begin(), input.end());
                blocks.push_back(block);
                input.clear();
            }
        } else {
            input.push_back(ch);
            if (input.size() == blockSize) {
                std::vector<uint8_t> block(input.begin(), input.end());
                blocks.push_back(block);
                input.clear();
            }
        }
    }

    if (!input.empty()) {
        std::vector<uint8_t> block(input.begin(), input.end());
        blocks.push_back(block);
    }
    return blocks;
}