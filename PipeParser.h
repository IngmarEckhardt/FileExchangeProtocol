#ifndef B15_PIPEPARSER_H
#define B15_PIPEPARSER_H
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <Windows.h>

std::vector<std::vector<uint8_t>> readBinaryInBlocksFromPipe(int pipeDescriptor, std::size_t blockSize);
std::vector<std::vector<uint8_t>> readStringInBlocksFromConsole(int consoleDescriptor, std::size_t blockSize);
std::vector<std::vector<uint8_t>> readBinaryInBlocksFromPipe(HANDLE pipeHandle, std::size_t blockSize);
std::vector<std::vector<uint8_t>> readStringInBlocksFromConsole(HANDLE consoleHandle, std::size_t blockSize);

#endif //B15_PIPEPARSER_H
