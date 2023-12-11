
#include <iostream>
#include <fstream>
#include <iterator>
#include "FileTransferHelper.h"
#include "ReceivingQueue.h"
#include "MessageComposer.h"
#include "ControlCharacter.h"

std::vector<std::vector<uint8_t>> incomingFileBlocks;

uint32_t blockAmountToReceive = 0;
uint32_t alreadyReceivedBlocks = 0;

uint32_t blockAmountToSend = 0;
int nextFileIndexToSend = -1;

int escapeCounter = 100.000;
int misses = 0;

bool incomingFileTransmissionAnnounced = false;
bool ownFileTransmissionAnnounced = false;
bool ownFileTransmissionAccepted = true;
bool ownFileTransmissionSuccessful = false;
bool clearPreSendingQueue = false;


//diese Funktion zählt nur bis 100.000 und checkt jedes Mal ob per 1 oder 8Mhz ein neues Zeichen ankam
void waitUntilReceivingQueueHasAnElement(const std::deque<uint8_t> &receivingQueue) {
    while (receivingQueue.empty() && misses < escapeCounter) {
        misses++;
        for (int i = 0; i < escapeCounter; i++) {
            if (!receivingQueue.empty()) {
                break;
            }
        }
    }
}

//zieht ein komplettes Byte von der receivingQueue mit ihren Halbbytes und packt in die receivedBytes-Liste mit vollen Bytes
void pullAByteWithCheckBefore(std::vector<uint8_t> &receivedBytes, std::deque<uint8_t> &receivingQueue,
                              uint8_t &currentByte) {
    if (!receivingQueue.empty()) {
        misses = 0;
        currentByte = getNextByte(receivingQueue);
        receivedBytes.push_back(currentByte);
    }
}


//SOH und 0b0 wurden bereits von der Queue gezogen und zu receivedBytes hinzugefügt
void searchForProtocolMessageOne(std::deque<uint8_t> &receivingQueue, std::vector<uint8_t> &receivedBytes) {
    uint8_t currentByte;
    //wir warten falls das zweite Zeichen noch nicht da ist
    do {
        waitUntilReceivingQueueHasAnElement(receivingQueue);
        pullAByteWithCheckBefore(receivedBytes, receivingQueue, currentByte);
    } while (currentByte != ETX);

    waitUntilReceivingQueueHasAnElement(receivingQueue);
    // Check if the next character is the correct BCC Character
    if (!receivingQueue.empty()) {
        misses = 0;
        uint8_t bcc = getNextByte(receivingQueue);
        uint8_t calculatedBcc = calculateCRCXOR(receivedBytes.data(), receivedBytes.size());


        if (receivedBytes.size() == 8 && receivedBytes[5] == STX && receivedBytes[6] == STX && bcc == calculatedBcc) {
            uint8_t mostSignificantByte = receivedBytes[1];
            uint8_t secondSignificantByte = receivedBytes[2];
            uint8_t leastSignificantByte = receivedBytes[3];

            blockAmountToReceive =
                    (static_cast<uint32_t>(mostSignificantByte) << 16) |
                    (static_cast<uint32_t>(secondSignificantByte) << 8) |
                    static_cast<uint32_t>(leastSignificantByte);


            //Der Speicher für die Blöcke wird erweitert auf die Anzahl der zu erwartenden Blöcke
            incomingFileBlocks.reserve(blockAmountToReceive);
            incomingFileTransmissionAnnounced = true;
        }
    }
    receivedBytes.clear();
}

// SOH und STX wurde vorher schon von der receiving Queue gezogen und receivedBytes hinzugefügt
uint32_t searchForRequestProtocollMsgTwo(std::deque<uint8_t> &receivingQueue, std::vector<uint8_t> receivedBytes) {
    int intervallStart = 0;
    uint8_t currentByte = 0;
    //wir warten bzw ziehen die nächsten vier zeichen
    for (int i = 0; i < 4; i++) {

        waitUntilReceivingQueueHasAnElement(receivingQueue);
        pullAByteWithCheckBefore(receivedBytes, receivingQueue, currentByte);
    }

    // das nächste Zeichen muss der BCC sein
    waitUntilReceivingQueueHasAnElement(receivingQueue);
    if (!receivingQueue.empty()) {
        misses = 0;
        uint8_t bcc = getNextByte(receivingQueue);
        uint8_t calculatedBcc = calculateCRCXOR(receivedBytes.data(), receivedBytes.size());

        //prüfen aller vorraussetzungen für Protokoll-message II
        if (receivedBytes[1] == STX && receivedBytes[5] == ETX && receivedBytes.size() == 7 && bcc == calculatedBcc) {
            uint8_t mostSignificantByte = receivedBytes[2];
            uint8_t secondSignificantByte = receivedBytes[3];
            uint8_t leastSignificantByte = receivedBytes[4];

            intervallStart =
                    (static_cast<uint32_t>(mostSignificantByte) << 16) |
                    (static_cast<uint32_t>(secondSignificantByte) << 8) |
                    static_cast<uint32_t>(leastSignificantByte);
        }
    }
    receivedBytes.clear();
    return intervallStart;
}

//SOH und STX wurden schon von der receiving Queue abgezogen und received Bytes hinzugefügt
void checkAndProcessMsg2(std::deque<uint8_t> &receivingQueue, std::vector<uint8_t> &receivedBytes) {
    uint32_t requestedBlock = searchForRequestProtocollMsgTwo(receivingQueue, receivedBytes);

    //wenn die Dateiübertragung abgeschlossen ist fragt der Empfänger noch einmal nach einem zusätzlichen Block, das wird als ACK gelesen
    if (requestedBlock > 1 && blockAmountToSend > 0 && requestedBlock == blockAmountToSend + 1) {

        ownFileTransmissionSuccessful = true;
        //file Transmission complete: send confirmation on screen or do nothing in pipe-mode

    } else if (requestedBlock > 1 && blockAmountToSend > 0 && requestedBlock < blockAmountToSend) {

        // falls der requested Block über 1 ist gab es einen übertragungsfehler und wir setzen den index zurück und setzen einen Bool damit die Pre-Sendequeue gelöscht wird
        nextFileIndexToSend = requestedBlock - 1;
        clearPreSendingQueue = true;

        //falls der Block 1 abgefragt wird, wird die Datenübertragung gestartet
    } else if (requestedBlock == 1 && blockAmountToSend > 0 && requestedBlock < blockAmountToSend) {


        ownFileTransmissionAccepted = true;
        nextFileIndexToSend = 0;
    }
    receivedBytes.clear();
}

std::vector<uint8_t> readDataBlock(std::deque<uint8_t> &receivingQueue, bool &lastBlock) {
    std::vector<uint8_t> datablock;
    uint8_t currentByte = 0b0;
    for (int i = 0; i < 4000; i++) {
        waitUntilReceivingQueueHasAnElement(receivingQueue);
        if (!receivingQueue.empty()) {
            misses = 0;
            currentByte = getNextByte(receivingQueue);
            datablock.push_back(currentByte);
        }
    }
    //lösche alle nullen von hinten im letzten Block bis ein Wert auftaucht
    if (lastBlock) {
        while (datablock.back() == 0b0) {
            datablock.pop_back();
        }
    }

    return datablock;
}

int searchForProtocollMsgThree(std::deque<uint8_t> &receivingQueue, std::vector<uint8_t> &receivedBytes) {
    uint8_t currentByte;
    std::vector<uint8_t> datablock;
    int32_t index = -1;

    //SOH und das Byte mit 0b0 wurden schon von der Queue gezogen und liegen in receivedVBytes,
    // wir erwarten als nächstes drei Bytes mit dem Index des Pakets nd ein STX Charakter
    for (int i = 0; i < 4; i++) {
        waitUntilReceivingQueueHasAnElement(receivingQueue);
        if (!receivingQueue.empty()) {
            misses = 0;
            currentByte = getNextByte(receivingQueue);
            receivedBytes.push_back(currentByte);
        } else { return -1; }
    }

    if (receivedBytes.size() == 6 && receivedBytes[5] == STX) {
        uint8_t mostSignificantByte = receivedBytes[2];
        uint8_t secondSignificantByte = receivedBytes[3];
        uint8_t leastSignificantByte = receivedBytes[4];

        index =
                (static_cast<uint32_t>(mostSignificantByte) << 16) |
                (static_cast<uint32_t>(secondSignificantByte) << 8) |
                static_cast<uint32_t>(leastSignificantByte);

        // der index des nächsten gebrauchten Pakets ist immer +1 von den schon empfangenen Paketen
        if (index == alreadyReceivedBlocks + 1) {

            waitUntilReceivingQueueHasAnElement(receivingQueue);
            if (!receivingQueue.empty()) {
                misses = 0;


                bool lastBlock = false;
                if (index == blockAmountToReceive) {
                    lastBlock = true;
                }
                //wir lesen 4kByte am Stück und löschen beim letzten Block die hinten liegenden 0b0 bytes
                datablock = readDataBlock(receivingQueue, lastBlock);
            }
            //wir fügen den Block als ganzes den receivedBytes hinzu
            for (uint8_t byte: datablock) {
                receivedBytes.push_back(byte);
            }


            waitUntilReceivingQueueHasAnElement(receivingQueue);
            if (!receivingQueue.empty()) {
                misses = 0;
                currentByte = getNextByte(receivingQueue);

                // Der nächste Charakter muss EOB oder ETX sein
                if (((currentByte != ETX) && (currentByte != EOB))
                    || ((index < blockAmountToReceive) && currentByte != ETX)
                    || ((index == blockAmountToReceive) && currentByte != EOB)
                        ) {
                    receivedBytes.clear();
                    return -1;
                }
                receivedBytes.push_back(currentByte);
            }


            waitUntilReceivingQueueHasAnElement(receivingQueue);
            // der nächste Charakter ist der BCC
            if (!receivingQueue.empty()) {
                misses = 0;
                uint8_t bcc = getNextByte(receivingQueue);
                uint8_t calculatedBcc = calculateCRCXOR(receivedBytes.data(), receivedBytes.size());
                // Wenn alle Test durchgehen wird der Block mit seinem Index (der Array-Index + 1 ist) an seinem Platz im Array gespeichert.
                if (bcc == calculatedBcc) {
                    //the index in the message blocks start with 1
                    incomingFileBlocks[index - 1] = datablock;
                    //if successful raise the block-counter
                    alreadyReceivedBlocks++;
                }
            }
        }
    }
    receivedBytes.clear();
    return index;
}


void processIndex(std::queue<std::vector<uint8_t>> &preSendingQueueCommands, int index) {
    if (index > 0 && index > alreadyReceivedBlocks && index <= blockAmountToReceive) {

        //Wenn der Index des datenpakets zu weit ist fehlen uns pakete und wir fordern eine Wiederholung ab dem letzten erfolgreichen Paket an,
        // damit es möglichst sofort gesendet wird an die spitze der Pre-SendingQueue
        preSendingQueueCommands.push(createBlockRepeatRequest(alreadyReceivedBlocks));
    }
    if (index > 0 && index == blockAmountToReceive && incomingFileBlocks.size() == blockAmountToReceive) {

        // wenn der index der Anzahl der Blöcke entspricht sollte auch unsere incomingFileBlocks die größe haben und wir senden
        // einen Request mit einem index + 1 um das erfolgreiche ankommen zu signalisieren
        preSendingQueueCommands.push(createBlockRepeatRequest(index + 1));
        ownFileTransmissionSuccessful = true;
    }
}

void parseIncomingMessages(std::deque<uint8_t> &receivingQueue, std::queue<std::vector<uint8_t>> &preSendingQueueData,
                           std::queue<std::vector<uint8_t>> &preSendingQueueCommands) {
    uint8_t currentByte = 0;
    std::vector<uint8_t> receivedBytes;
    // Wait for the Start of Header SOH character
    if (!receivingQueue.empty()) {
        currentByte = getNextByte(receivingQueue);
    }

    if (currentByte == SOH) {
        receivedBytes.push_back(SOH);


        //Niemand hat eine Datenübertragung angekündigt, wir warten auf die Ankündigung einer Datenübertragung, Msg1
        if (!incomingFileTransmissionAnnounced && !ownFileTransmissionAnnounced) {
            waitUntilReceivingQueueHasAnElement(receivingQueue);
            pullAByteWithCheckBefore(receivedBytes, receivingQueue, currentByte);
            if (currentByte == 0b0) {
                searchForProtocolMessageOne(receivingQueue, receivedBytes);
            } else {
                receivedBytes.clear();
            }

            // Wir warten auf reinkommende Daten, Msg3 und wenn der Index nicht stimmt und wir eine Lücke in den Daten haben senden wir einen Request, Msg2, um den Index des Senders zurückzusetzen
        } else if (incomingFileTransmissionAnnounced && !ownFileTransmissionAnnounced) {
            int index = -1;
            waitUntilReceivingQueueHasAnElement(receivingQueue);
            pullAByteWithCheckBefore(receivedBytes, receivingQueue, currentByte);

            if (currentByte == 0b0) {
                index = searchForProtocollMsgThree(receivingQueue, receivedBytes);
            } else {
                receivedBytes.clear();
            }

            processIndex(preSendingQueueCommands, index);


            //Entweder wir bekommen einen Request weil wir die Datenübertragung einen Fehler hat,Msg2 oder wir bekommen eine Ankündigung einer Datenübertragung anderen,Msg1
        } else if (!incomingFileTransmissionAnnounced && ownFileTransmissionAnnounced) {

            waitUntilReceivingQueueHasAnElement(receivingQueue);
            pullAByteWithCheckBefore(receivedBytes, receivingQueue, currentByte);

            // wir wollen eine Datei senden, wir warten auf Request mit index 1 um die Blockübertragung ab Blockindex 0 zu starten
            // falls wir die Dateiübertragung gestartet haben und ein anderer Index kommt löschen wir die Pre-Sending-Queue, bis auf die erste Nachricht
            //und befüllen sie wieder mit Blöcken ab dem nachgefragten Index
            if (currentByte == STX) {
                checkAndProcessMsg2(receivingQueue, receivedBytes);

                // Wenn es keine Message 2 ist, wird uns eine Datenübertragung angekündigt
            } else if (currentByte == 0b0) {
                searchForProtocolMessageOne(receivingQueue, receivedBytes);
            } else {
                receivedBytes.clear();
            }


            //Beide wollen senden, es können Daten, Msg3, und Requests bei Fehlern, Msg2 reinkommen
        } else if (incomingFileTransmissionAnnounced && ownFileTransmissionAnnounced) {
            int index = -1;
            waitUntilReceivingQueueHasAnElement(receivingQueue);
            pullAByteWithCheckBefore(receivedBytes, receivingQueue, currentByte);

            if (currentByte == STX) {
                checkAndProcessMsg2(receivingQueue, receivedBytes);

                // Wenn es keine Message 2 ist, kommen Daten rein
            } else if (currentByte == 0b0) {
                index = searchForProtocollMsgThree(receivingQueue, receivedBytes);
            } else {
                receivedBytes.clear();
            }
            processIndex(preSendingQueueCommands, index);
        }

    }

}

void sendAnnouncementMsgOne(std::queue<std::vector<uint8_t>> &preSendingQueueCommands, int blockAmount) {
    preSendingQueueCommands.push(createFileTransferHandshake(blockAmount));
}

void putNextDataMsgThreeInPreSendingQueue(std::vector<std::vector<uint8_t>> fileBlocks, int blockArrayIndex,
                                          std::queue<std::vector<uint8_t>> &preSendingQueueData) {
    std::vector<uint8_t> nextBlock = fileBlocks[blockArrayIndex];
    preSendingQueueData.push(nextBlock);

}


void reconstructAndSaveFile(const std::vector<std::vector<uint8_t>> &fileBlocks, const std::string &outputFileName,
                       bool safeFile) {
    if (safeFile) {
        // Schritt 1: Datei wieder zusammensetzen
        std::vector<uint8_t> reconstructedFile;
        for (const auto &block: fileBlocks) {
            reconstructedFile.insert(reconstructedFile.end(), block.begin(), block.end());
        }

        // Schritt 2: Datei abspeichern
        std::ofstream outputFile(outputFileName, std::ios::binary);
        if (outputFile.is_open()) {
            outputFile.write(reinterpret_cast<const char *>(reconstructedFile.data()), reconstructedFile.size());
            outputFile.close();
        }
    }

    //über cout ausgeben
    for (const auto &block: fileBlocks) {
        std::copy(block.begin(), block.end(), std::ostream_iterator<uint8_t>(std::cout));
    }

}

void startFileTransfer(std::queue<std::vector<uint8_t>> &preSendingQueueData,
                       std::queue<std::vector<uint8_t>> &preSendingQueueCommands, std::deque<uint8_t> &receivingQueue,
                       std::vector<std::vector<uint8_t>> &fileBlocks, bool &escapePressed, bool &safeFile) {

    int counter = 0;

    while (!escapePressed) {
        //als erstes die Eingangsschlange lesen
        parseIncomingMessages(receivingQueue, preSendingQueueData, preSendingQueueCommands);

        //falls wir bisher keine Datei senden schauen wir nach solange bis das file-Array gefüllt ist, dann ändern wir den bool
        if (!ownFileTransmissionAnnounced) {
            if (!fileBlocks.empty()) {
                ownFileTransmissionAnnounced = true;
            }
        }

        //falls wir eine Datei senden wollen aber es wurde noch nicht akzeptiert senden wir unsere Ankündigung
        //nur alle 100 durchläufe wird die Ankündigung, Msg1 geschickt falls sie bisher nicht akzeptiert wurde und keine Msg 2 mit Index 1 erfasst wurde
        if ((!ownFileTransmissionAccepted && ownFileTransmissionAnnounced) && (counter++ % 100 != 0)) {
            counter = 0;
            sendAnnouncementMsgOne(preSendingQueueCommands, fileBlocks.size());
        }

        //wenn die Übertragung akzeptiert wurde, und wir eine Nachricht erhalten dass es eine Lücke in der Übertragung gibt
        //wenn der boole clearPreSendingQueue gesetzt wurde, wurde der index zurückgesetzt, wir befüllen die pre-SendingQueue erneut
        if (ownFileTransmissionAccepted && !ownFileTransmissionSuccessful && nextFileIndexToSend >= 0 &&
            nextFileIndexToSend < blockAmountToSend - 1 && clearPreSendingQueue) {

            //die komplette Queue wird neu angelegt und damit gelöscht
            preSendingQueueData = std::queue<std::vector<uint8_t>>();
            clearPreSendingQueue = false;

            putNextDataMsgThreeInPreSendingQueue(fileBlocks, nextFileIndexToSend++, preSendingQueueData);
        }

        // wenn die Pre-Sende-Queue durch den Thread der die Daten in die eigenetliche Sendequeue schaufelt kleiner als 200 elemente ist, stecken wir das nächste rein
        if (ownFileTransmissionAccepted && !ownFileTransmissionSuccessful && nextFileIndexToSend >= 0 &&
            nextFileIndexToSend < blockAmountToSend - 1 && preSendingQueueData.size() < 200) {

            //wir sorgen dafür dass die Datei mehrfach gesendet wird, indem der index auf 0 gesetzt wird, sobald wir am Ende angelangt sind
            nextFileIndexToSend %= blockAmountToSend - 1;
            putNextDataMsgThreeInPreSendingQueue(fileBlocks, nextFileIndexToSend++, preSendingQueueData);
        }


        //wir sind fertig, speichern die Datei und geben sie über cout aus
        if (ownFileTransmissionAccepted && ownFileTransmissionSuccessful) {
            reconstructAndSaveFile(fileBlocks, "output.bin", safeFile);
        }
    }
}