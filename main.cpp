#include <iostream>
#include "b15libs/b15f.h"
#include "MessageComposer.h"
#include "SendingQueue.h"
#include "ReceivingQueue.h"
#include "PipeParser.h"
#include "FileTransferHelper.h"
#include <thread>

using namespace std;


// globale Variable die von beiden nebenthreads gelesen wird, aber nur von einem geschrieben
bool escPressed = false;
// legt fest welche ports im B15 gelesen und geschrieben werden
bool secondComputer = false;
// wechselt zwischen Text- und Datenkommunikation
bool fileTransmissionModus = false;
// da Daten und Texte per std::cin reinkommen per Pipe kann man auf ein ESC durch den User erst lauschen wenn die Daten gelesen wurden, das regelt der bool
bool parseCinInput = false;
// Wenn der User Enter drückt wird die Übertragung gestartet, sorgt dafür dass beide Computer laufende Programme haben bevor die Übertragung gestartet wird
bool startTransmission = false;
// wenn die Datei gespeichert werden soll und nicht nur per std::cout ausgegeben
bool safeFile = false;
// Blockgröße der Datenübertragung
const std::size_t blockSize = 4 * 1024;  // 4 KByte
// Deque die halb-bytes enthält und die an das B15 geschickt wird per einzelnen Thread
std::deque<uint8_t> sendingQueue;
//zwei Queues die bytes enthalten und mit denen die sendingQueue befüllt wird
std::queue<std::vector<uint8_t>> preSendingQueueData;
std::queue<std::vector<uint8_t>> preSendingQueueCommands;
// eien receiving queue die schon geparste bytes enthält und vom gleichen Thread der die sendingQueue bearbeitet befüllt wird durch lesen des Ports beim B15
std::deque<uint8_t> receivingQueue;
// ein Vektor mit den 4kByte Blöcken der Datei die per std::cin gelesen wurde
std::vector<std::vector<uint8_t>> fileBlocks;
// die Instanz des B15
B15F drv = B15F::getInstance();


void handleStartArgs(int argc, char *const *argv) {
    if (argc > 1) {
        // Überprüfe, ob das übergebene Argument dem erwarteten Parameter entspricht
        if (strcmp(argv[1], "-second") == 0) {
            // Setze einen Boolean-Wert entsprechend
            secondComputer = true;

            cout << "second computer uses upper bit of the port for sending" << endl;
        } else {
            cout << "first computer uses lower bits of the port for sending" << endl;
        }
    }
    if (argc > 2) {
        if (std::strcmp(argv[2], "-data") == 0) {
            // Setze einen Boolean-Wert entsprechend
            fileTransmissionModus = true;

            std::cout << "starting file transmission" << std::endl;
        }
    } else {
        std::cout << "Kein Parameter übergeben." << std::endl;
    }
    if (argc > 3) {
        if (std::strcmp(argv[3], "-file") == 0) {
            // Setze einen Boolean-Wert entsprechend
            safeFile = true;

            std::cout << "saving file" << std::endl;
        }
    } else {
        std::cout << "Kein Parameter übergeben." << std::endl;
    }
}


void sendToPort(uint8_t fourBitNumberToSend) {
    uint8_t eightBitNumberToSend = secondComputer ? (fourBitNumberToSend << 4) & 0xF0 : fourBitNumberToSend;
    drv.setRegister(&PORTA, eightBitNumberToSend);
}

uint8_t receiceFromPort() {
    uint8_t value = secondComputer ? drv.getRegister(&PINA) : drv.getRegister(&PINA) >> 4;
    return value;
}


void dataExchangeLoop() {
    uint8_t lastHalfByte = 0b0;
    int loopCounter = 0;
    while (!escPressed) {
        //gesendet wird nur bei jedem dritten Schleifendurchlauf um eine höhere Abtastrate als Senderate zu erreichen
        if (!sendingQueue.empty() && loopCounter == 2) {
            uint8_t nextSignToSend = sendingQueue.front();
            sendingQueue.pop_front();
            sendToPort(nextSignToSend);
        }
        uint8_t halfByteFromPort = receiceFromPort();
        if (halfByteFromPort != lastHalfByte) {
            receivingQueue.push_back(halfByteFromPort);
            lastHalfByte = halfByteFromPort;
        }
        loopCounter++;
        loopCounter %= 3;
    }
}

void preQueueAndWaitForEscAndEnterLoop() {
    // 4500 Byte * 20 Datenpakete * 2.5 wegen Umwandlung in Halbbytes + EscapeCharakters
    while (!escPressed) {
        // 4500 Byte * 20 Datenpakete * 2.5 wegen Umwandlung in Halbbytes + EscapeCharakters
        if (!preSendingQueueData.empty() && startTransmission && sendingQueue.size() < 225000) {
            vector<uint8_t> nextArray = preSendingQueueData.front();
            preSendingQueueData.pop();
            for (uint8_t &byte: nextArray) {
                putByteAsHalfBytesInSendingQueue(byte, sendingQueue);
            }
        }
        //falls ein Befehl vorliegt wird der immer zwischen zwei Datenpakete eingeschoben und frühzeitig gesendet
        if (!preSendingQueueCommands.empty()) {
            vector<uint8_t> nextArray = preSendingQueueCommands.front();
            preSendingQueueCommands.pop();
            for (uint8_t &byte: nextArray) {
                putByteAsHalfBytesInSendingQueue(byte, sendingQueue);
            }
        }
        if (parseCinInput) {
            char ch;
            std::cin.get(ch);  // Warte auf Tastatureingabe
            if (ch == 27) {    // ASCII-Code für ESC-Taste
                escPressed = true;
            } else if (ch == '\n') {  // Check for ENTER key
                startTransmission = true;
            }
        }

    }
}

void fileExchangeProtokoll() {
    startFileTransfer(preSendingQueueData, preSendingQueueCommands, receivingQueue, fileBlocks, escPressed, safeFile);
}

void setUpperFourBitsAsOutputRegister() {
    // Setze die Richtung der ersten vier Bits als Ausgabe (1 für Ausgabe, 0 für Eingabe)
    drv.setRegister(&DDRA, 0b11110000);
}


// Funktion, die die letzten vier Bits eines Registers auf 1 setzt
void setLowerFourBitsAsOutputRegister() {
    // Setze die Richtung der letzten vier Bits als Ausgabe (1 für Ausgabe, 0 für Eingabe)
    drv.setRegister(&DDRA, 0b00001111);
}


int main(int argc, char *argv[]) {
    handleStartArgs(argc, argv);
    if (secondComputer) {
        setUpperFourBitsAsOutputRegister();
    } else {
        setLowerFourBitsAsOutputRegister();
    }

    std::thread dataExchangeThread(dataExchangeLoop);
    std::thread preQueueThread(preQueueAndWaitForEscAndEnterLoop);

//windows
    HANDLE pipeHandle = GetStdHandle(STD_INPUT_HANDLE);
    //linux
//    int pipeDescriptor = 0; // Standard-Eingabe






    /** Dateiübertragungsprotokoll */
    if (fileTransmissionModus) {


        //startet einen Thread der Messages vom Protokoll handhabt und die Datei raussendet wenn sie fertig gelesen ist;
        std::thread fileTransmissionThread(fileExchangeProtokoll);


        /** Liest Dateien in 4kByte Blöcken aus der Pipe */
        //linux
//    std::vector<std::vector<uint8_t>> binaryDataBlocks = readBinaryInBlocksFromPipe(pipeDescriptor, blockSize);
        std::vector<std::vector<uint8_t>> binaryDataBlocks = readBinaryInBlocksFromPipe(pipeHandle, blockSize);

        uint32_t blockamount = binaryDataBlocks.size();
        std::vector<std::vector<uint8_t>> allFileBlocks;
        for (int i = 0; i < blockamount; i++) {
            std::vector<uint8_t> byteArray = createFileBlock(binaryDataBlocks[i], blockSize, i + 1, blockamount);
            allFileBlocks[i] = byteArray;
        }

        //erst wenn alle Blöcke gelesen wurden wird das in die globale Variable übertragen
        fileBlocks = allFileBlocks;

        //erst danach wird begonnen auf Enter oder ESC zu lauschen
        parseCinInput = true;


        dataExchangeThread.join();
        preQueueThread.join();
        fileTransmissionThread.join();




        /** Textübertragungsprotokoll */
    } else {
        std::vector<std::vector<uint8_t>> consoleBlocks = readStringInBlocksFromConsole(pipeHandle, blockSize);
        std::cout << "from console: ";

        for (const auto &block: consoleBlocks) {
            for (uint8_t byte: block) {
                std::cout << static_cast<char>(byte);
            }
        }
        std::cout << std::endl;

        std::string text(consoleBlocks[0].begin(), consoleBlocks[0].end());
        uint8_t *byteArray = createTextBlock(text);
        size_t arrayLength = text.length() + 4;

        for (size_t i = 0; i < arrayLength; ++i) {
            putByteAsHalfBytesInSendingQueue(byteArray[i], sendingQueue);
        }


        //erst wenn die Textnachricht von der Pipe gelesen wurde, wird begonnen auf Enter oder ESC zu lauschen
        parseCinInput = true;
        //escPressed wird ebenfalls auf true gesetzt wenn ein geprüfter Text-Input erkannt und auf die cout ausgegeben wurde
        while (!escPressed) {
            readTextInput(receivingQueue, escPressed);
            for (size_t i = 0; i < arrayLength; ++i) {
                putByteAsHalfBytesInSendingQueue(byteArray[i], sendingQueue);
            }
        }
        delete[] byteArray;

        dataExchangeThread.join();
        preQueueThread.join();
    }
    return 0;
}






/** helper function for debugging */
//
//void assertOutput() {
//    std::cout << "\nActual in Sendingqueue: ";
//    for (const auto &value: sendingQueue) {
//        std::cout << std::bitset<8>(value) << " ";
//    }
//    std::cout << std::endl;
//    std::cout << "Actual in receivingqueue: ";
//    for (const auto &value: receivingQueue) {
//        std::cout << std::bitset<8>(value) << " ";
//    }
//    std::cout << std::endl;
//    std::cout << "------------------------" << std::endl;
//}