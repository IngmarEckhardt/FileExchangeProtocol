
#include <iostream>
#include <fstream>
#include <iterator>
#include <chrono>
#include "FileTransferHelper.h"
#include "ReceivingQueue.h"
#include "MessageComposer.h"
#include "ControlCharacter.h"

constexpr int WAIT_INTERVAL_MS = 200;

std::vector<std::vector<uint8_t>> incomingFileBlocks;
std::chrono::steady_clock::time_point lastExecutionTimeRequestMsg2 =
        std::chrono::steady_clock::now()-std::chrono::milliseconds(WAIT_INTERVAL_MS);;

uint32_t blockAmountToReceive = 0;
uint32_t alreadyReceivedBlocks = 0;
uint32_t blockAmountToSend = 0;
int currentFileBlockIndex = -1;
int misses = 0;






bool incomingFileTransmissionAnnounced = false;
bool ownFileTransmission = false;
bool ownFileTransmissionAccepted = true;
bool ownFileTransmissionSuccessful = false;
bool otherFileTransmissionSuccessful = false;
bool clearPreSendingQueue = false;


//diese Funktion zählt nur bis 100.000 und checkt jedes Mal ob per 1 oder 8Mhz ein neues Zeichen ankam
void waitUntilReceivingQueueHasAnElement(const std::deque<uint8_t> &receivingQueue) {
    constexpr int MAX_ESCAPE_COUNT = 100.000;
    for (misses = 0; receivingQueue.empty() && misses < MAX_ESCAPE_COUNT; misses++) {}
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
void processTransferAnnouncementProtocolMessageOne(std::deque<uint8_t> &receivingQueue, std::vector<uint8_t> &receivedBytes) {
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
uint32_t searchForRequestProtocolMsgTwo(std::deque<uint8_t> &receivingQueue, std::vector<uint8_t> receivedBytes) {
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
void processRequestMessageProtocolMessageTwo(std::deque<uint8_t> &receivingQueue, std::vector<uint8_t> &receivedBytes) {
    uint32_t requestedBlock = searchForRequestProtocolMsgTwo(receivingQueue, receivedBytes);

    //wenn die Dateiübertragung abgeschlossen ist fragt der Empfänger noch einmal nach einem zusätzlichen Block, das wird als ACK gelesen
    if (requestedBlock > 1 && blockAmountToSend > 0 && requestedBlock == blockAmountToSend + 1) {

        ownFileTransmissionSuccessful = true;
        //file Transmission complete: send confirmation on screen or do nothing in pipe-mode

    } else if (requestedBlock > 1 && blockAmountToSend > 0 && requestedBlock < blockAmountToSend) {

        // falls der requested Block über 1 ist gab es einen übertragungsfehler und wir setzen den index zurück und setzen einen Bool damit die Pre-Sendequeue gelöscht wird
        currentFileBlockIndex = requestedBlock - 1;
        clearPreSendingQueue = true;

        //falls der Block 1 abgefragt wird, wird die Datenübertragung gestartet
    } else if (requestedBlock == 1 && blockAmountToSend > 0 && requestedBlock < blockAmountToSend) {


        ownFileTransmissionAccepted = true;
        currentFileBlockIndex = 0;
    }
    receivedBytes.clear();
}

std::vector<uint8_t> readDataBlock(std::deque<uint8_t> &receivingQueue, bool &lastBlock) {
    const std::size_t BLOCKSIZE = 4 * 1024;
    std::vector<uint8_t> datablock;
    uint8_t currentByte = 0b0;

    for (int i = 0; i < BLOCKSIZE; i++) {
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

int processDataMessageProtocolMessageThree(std::deque<uint8_t> &receivingQueue, std::vector<uint8_t> &receivedBytes) {
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
    //Wir sollten jetzt 6 bytes haben und als letztes ein STX
    if (receivedBytes.size() == 6 && receivedBytes[5] == STX) {
        uint8_t mostSignificantByte = receivedBytes[2];
        uint8_t secondSignificantByte = receivedBytes[3];
        uint8_t leastSignificantByte = receivedBytes[4];

        index = (static_cast<uint32_t>(mostSignificantByte) << 16) |
                (static_cast<uint32_t>(secondSignificantByte) << 8) |
                static_cast<uint32_t>(leastSignificantByte);


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
            // Wenn der BCC stimmt und der index der nächste erwartete index ist, wird der Block einsortiert
            if (bcc == calculatedBcc && index == alreadyReceivedBlocks + 1) {
                //the index in the message blocks start with 1
                incomingFileBlocks[index - 1] = datablock;
                //if successful raise the block-counter
                alreadyReceivedBlocks++;
            }

        }
    }
    receivedBytes.clear();
    return index;
}


void processIndex(std::queue<std::vector<uint8_t>> &preSendingQueueCommands, int index, std::chrono::steady_clock::time_point currentTime) {
    std::chrono::milliseconds elapsedTime = duration_cast<std::chrono::milliseconds>(currentTime - lastExecutionTimeRequestMsg2);
    uint32_t twoPercentBlockAmount = blockAmountToReceive / 50;

    /** Wenn wir ein Paket verpasst haben schicken wir einen erneuten Request, ebenfalls falls wir das komplette hintere Ende der Datei verpasst haben und
     * die Blocknummern schon wieder kleiner sind als die bisher schon erhaltenen Blocknummern. Allerdings nur wenn der aktuelle index weiter vorne liegt als 2%
     * der Gesamtpaketmenge, ansonsten ignorieren wir die 1,99% doppelten Pakete einfach. Wir schicken den Request nur noch 200ms erneut */
    if (elapsedTime.count() >= WAIT_INTERVAL_MS && index > 0 && ((index > alreadyReceivedBlocks) || (index < (alreadyReceivedBlocks - twoPercentBlockAmount))) && index < blockAmountToReceive) {

        //Wenn der Index des datenpakets zu weit ist fehlen uns pakete und wir fordern eine Wiederholung ab dem letzten erfolgreichen Paket an,
        // damit es möglichst sofort gesendet wird an die spitze der Pre-SendingQueue
        preSendingQueueCommands.push(createBlockRepeatRequest(alreadyReceivedBlocks+1));
        lastExecutionTimeRequestMsg2 = std::chrono::steady_clock::now();
    }

    /** erhalten wir den letzten Block schicken wir einen erneuten request mit einem Nummer hinter der Blockanzahl den der Empfänger als ACK interpretiert */
    if (index > 0 && index == blockAmountToReceive && incomingFileBlocks.size() == blockAmountToReceive) {

        // wenn der index der Anzahl der Blöcke entspricht sollte auch unsere incomingFileBlocks die größe haben und wir senden
        // einen Request mit einem index + 1 um das erfolgreiche ankommen zu signalisieren
        preSendingQueueCommands.push(createBlockRepeatRequest(index + 1));
        otherFileTransmissionSuccessful = true;
    }
}

void parseIncomingMessages(std::deque<uint8_t> &receivingQueue, std::queue<std::vector<uint8_t>> &preSendingQueueData,
                           std::queue<std::vector<uint8_t>> &preSendingQueueCommands,
                           std::chrono::steady_clock::time_point currentTime) {
    uint8_t currentByte = 0;
    std::vector<uint8_t> receivedBytes;
    // Wait for the Start of Header SOH character
    if (!receivingQueue.empty()) {
        currentByte = getNextByte(receivingQueue);
    }

    if (currentByte == SOH) {
        receivedBytes.push_back(SOH);


        /** Fall 1 Niemand hat eine Datenübertragung angekündigt, wir warten auf die Ankündigung einer Datenübertragung, Msg1 */
        if (!incomingFileTransmissionAnnounced && !ownFileTransmission) {
            waitUntilReceivingQueueHasAnElement(receivingQueue);
            pullAByteWithCheckBefore(receivedBytes, receivingQueue, currentByte);
            if (currentByte == 0b0) {
                processTransferAnnouncementProtocolMessageOne(receivingQueue, receivedBytes);
            } else {
                receivedBytes.clear();
            }

            /** Fall 2, Wir warten auf reinkommende Daten, Msg3 und wenn der Index nicht stimmt und wir eine Lücke in den Daten haben
             * senden wir einen Request, Msg2, um den Index des Senders zurückzusetzen */
        } else if (incomingFileTransmissionAnnounced && !ownFileTransmission) {
            int index = -1;
            waitUntilReceivingQueueHasAnElement(receivingQueue);
            pullAByteWithCheckBefore(receivedBytes, receivingQueue, currentByte);

            if (currentByte == 0b0) {
                index = processDataMessageProtocolMessageThree(receivingQueue, receivedBytes);
            } else {
                receivedBytes.clear();
            }

            processIndex(preSendingQueueCommands, index, currentTime);


            /** Fall 3 Entweder wir bekommen einen Request weil wir die Datenübertragung einen Fehler hat,Msg2
             * oder wir bekommen eine Ankündigung einer Datenübertragung anderen,Msg1 */
        } else if (!incomingFileTransmissionAnnounced && ownFileTransmission) {

            waitUntilReceivingQueueHasAnElement(receivingQueue);
            pullAByteWithCheckBefore(receivedBytes, receivingQueue, currentByte);

            // wir wollen eine Datei senden, wir warten auf Request mit index 1 um die Blockübertragung ab Blockindex 0 zu starten
            // falls wir die Dateiübertragung gestartet haben und ein anderer Index kommt löschen wir die Pre-Sending-Queue, bis auf die erste Nachricht
            //und befüllen sie wieder mit Blöcken ab dem nachgefragten Index
            if (currentByte == STX) {
                processRequestMessageProtocolMessageTwo(receivingQueue, receivedBytes);

                // Wenn es keine Message 2 ist, wird uns eine Datenübertragung angekündigt
            } else if (currentByte == 0b0) {
                processTransferAnnouncementProtocolMessageOne(receivingQueue, receivedBytes);
            } else {
                receivedBytes.clear();
            }


            /** Fall 4 Beide wollen senden, es können Daten, Msg3 oder Requests bei Fehlern, Msg2 reinkommen */
        } else if (incomingFileTransmissionAnnounced && ownFileTransmission) {
            int index = -1;
            waitUntilReceivingQueueHasAnElement(receivingQueue);
            pullAByteWithCheckBefore(receivedBytes, receivingQueue, currentByte);

            if (currentByte == STX) {
                processRequestMessageProtocolMessageTwo(receivingQueue, receivedBytes);

                // Wenn es keine Message 2 ist, kommen Daten rein
            } else if (currentByte == 0b0) {
                index = processDataMessageProtocolMessageThree(receivingQueue, receivedBytes);
            } else {
                receivedBytes.clear();
            }
            processIndex(preSendingQueueCommands, index, currentTime);
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

void fileTransfer(std::queue<std::vector<uint8_t>> &preSendingQueueData,
                  std::queue<std::vector<uint8_t>> &preSendingQueueCommands, std::deque<uint8_t> &receivingQueue,
                  std::vector<std::vector<uint8_t>> &fileBlocks, bool &escapePressed, bool &safeFile) {


    constexpr int MAX_PRE_SENDING_QUEUE_SIZE = 200;
    std::chrono::steady_clock::time_point currentTime;
    std::chrono::milliseconds elapsedTime;
    //wir legen die erste executionTime in die Vergangenheit
    std::chrono::steady_clock::time_point lastExecutionTime = std::chrono::steady_clock::now()-std::chrono::milliseconds(WAIT_INTERVAL_MS);


    while (!escapePressed) {
        currentTime = std::chrono::steady_clock::now();
        elapsedTime = duration_cast<std::chrono::milliseconds>(currentTime - lastExecutionTime);

        parseIncomingMessages(receivingQueue, preSendingQueueData, preSendingQueueCommands, currentTime);



        //falls wir bisher keine Datei senden schauen wir nach solange bis das file-Array gefüllt ist, dann ändern wir den bool
        if (!ownFileTransmission && !fileBlocks.empty()) {
            ownFileTransmission = true;
        }


        //falls wir eine Datei senden wollen aber es wurde noch nicht akzeptiert senden wir unsere Ankündigung alle 200ms
        if ((!ownFileTransmissionAccepted && ownFileTransmission) && elapsedTime.count() >= WAIT_INTERVAL_MS) {
            sendAnnouncementMsgOne(preSendingQueueCommands, fileBlocks.size());
            lastExecutionTime = std::chrono::steady_clock::now();
        }


        //wenn die Übertragung akzeptiert wurde, und wir eine Nachricht erhalten dass es eine Lücke in der Übertragung gibt
        //wenn der boole clearPreSendingQueue gesetzt wurde, wurde der index zurückgesetzt, wir befüllen die pre-SendingQueue erneut
        if (ownFileTransmissionAccepted && !ownFileTransmissionSuccessful && currentFileBlockIndex >= 0 &&
            currentFileBlockIndex < blockAmountToSend - 1 && clearPreSendingQueue) {

            //die komplette Queue wird neu angelegt und damit gelöscht
            preSendingQueueData = std::queue<std::vector<uint8_t>>();
            clearPreSendingQueue = false;

            putNextDataMsgThreeInPreSendingQueue(fileBlocks, currentFileBlockIndex++, preSendingQueueData);
        }


        // wenn die Pre-Sende-Queue durch den Thread der die Daten in die eigenetliche Sendequeue schaufelt kleiner als 200 elemente ist, stecken wir das nächste rein
        if (ownFileTransmissionAccepted && !ownFileTransmissionSuccessful && currentFileBlockIndex >= 0 &&
            currentFileBlockIndex < blockAmountToSend - 1 && preSendingQueueData.size() < MAX_PRE_SENDING_QUEUE_SIZE && !clearPreSendingQueue) {

            //wir sorgen dafür dass die Datei mehrfach gesendet wird, indem der index auf 0 gesetzt wird, sobald wir am Ende angelangt sind
            currentFileBlockIndex %= blockAmountToSend - 1;
            putNextDataMsgThreeInPreSendingQueue(fileBlocks, currentFileBlockIndex++, preSendingQueueData);
        }


        //wir sind fertig, speichern die Datei und geben sie über cout aus
        if (otherFileTransmissionSuccessful) {
            reconstructAndSaveFile(fileBlocks, "output.bin", safeFile);
        }
    }
}