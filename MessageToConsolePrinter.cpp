#include <string>
#include "ControlCharacter.h"
#include <iostream>
#include "MessageToConsolePrinter.h"

void printTextFromMessageBlockToConsole(uint8_t *receivedArray, size_t arrayLength, bool &messageReceived) {
    // Check if the array has at least SOH, STX, ETX and BCC characters
    if (arrayLength < 4) {
        std::cout << "Invalid array length. Unable to extract text." << std::endl;
        return;
    }

    std::size_t currentIndex = 0;

    // Check for Start of Header (SOH)
    if (receivedArray[currentIndex++] != SOH) {
        std::cout << "Invalid Start of Header (SOH) character." << std::endl;
        return;
    }

    // Check for Start of Text (STX)
    if (receivedArray[currentIndex++] != STX) {
        std::cout << "Invalid Start of Text (STX) character." << std::endl;
        return;
    }

    // Extract the text between SOH, STX, and ETX
    std::string extractedText;
    while (currentIndex < arrayLength && receivedArray[currentIndex] != ETX) {
        extractedText += static_cast<char>(receivedArray[currentIndex++]);
    }

    // Check for End of Text (ETX)
    if (currentIndex < arrayLength && receivedArray[currentIndex] == ETX) {
        std::cout << "Extracted Text: " << extractedText << std::endl;
        messageReceived = true;
    } else {
        std::cout << "Invalid End of Text (ETX) character." << std::endl;
    }
}