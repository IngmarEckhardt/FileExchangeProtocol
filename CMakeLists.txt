cmake_minimum_required(VERSION 3.20)
project(B15)

set(CMAKE_CXX_STANDARD 17)

add_executable(ConnectionProtocoll main.cpp MessageComposer.cpp MessageComposer.h MessageToConsolePrinter.cpp MessageToConsolePrinter.h ControlCharacter.h ControlCharacter.cpp SendingQueue.cpp SendingQueue.h ReceivingQueue.cpp ReceivingQueue.h PipeParser.cpp PipeParser.h FileTransferHelper.cpp FileTransferHelper.h)
