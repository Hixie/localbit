#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

// #define VERBOSE

enum color { black, blue, red, purple, green, teal, yellow, white };

// SYSTEM

void printLog(char* message, ...) {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    char timestamp[27]; // enough for "-2147483648:00:00 00:00:00" + null
    strftime(timestamp, 27, "%Y:%m:%d %H:%M:%S", localtime(&currentTime.tv_sec));
    printf("%s.%03d cloudbit: ", timestamp, currentTime.tv_usec / 1000);
    va_list args;
    va_start(args, message);
    vprintf(message, args);
    puts("");
    va_end(args);
}

void printError(char* message) {
    printLog("%s: %s", message, strerror(errno));
}

void fillTimeSpec(struct timespec* ts, double seconds) {
    double integral, fraction;
    fraction = modf(seconds, &integral);
    ts->tv_sec = (int)trunc(integral);
    ts->tv_nsec = (int)trunc(fraction * 1000000000);
}

void delay(double seconds) {
    struct timespec ts;
    fillTimeSpec(&ts, seconds);
    nanosleep(&ts, NULL);
}

bool socketHasMessage(uint32_t socket, double seconds) {
    struct timespec ts;
    fillTimeSpec(&ts, seconds);
    fd_set descriptors;
    FD_ZERO(&descriptors);
    FD_SET(socket, &descriptors);
    int result = pselect(socket + 1, &descriptors, NULL, NULL, &ts, NULL);
    return FD_ISSET(socket, &descriptors);
}

bool readFile(char* filename, uint8_t** buffer, uint32_t* size, bool nullTerminate) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        printError("Failed to read file during fopen");
        *size = 0;
        *buffer = NULL;
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        printError("Failed to read file during fseek to end");
        fclose(file);
        *size = 0;
        *buffer = NULL;
        return false;
    }
    uint32_t length = ftell(file);
    if (fseek(file, 0, SEEK_SET) != 0) {
        printError("Failed to read file during fseek to start");
        fclose(file);
        *size = 0;
        *buffer = NULL;
        return false;
    }
    if (*size == 0) {
        *size = length;
    } else if (*size > length) {
        printLog("Failed to read file: insufficient data in file (file is %d bytes, need %d bytes)", length, *size);
        fclose(file);
        *size = 0;
        *buffer = NULL;
        return false;
    }
    uint32_t bufferSize = *size + (nullTerminate ? 1 : 0);
    *buffer = malloc(bufferSize);
    length = fread(*buffer, 1, *size, file);
    if (length != *size) {
        printError("Failed to read file during fread");
        free(*buffer);
        *buffer = NULL;
        *size = 0;
        fclose(file);
        return false;
    }
    if (fclose(file) != 0) {
        printError("Failed to read file during fclose");
        free(*buffer);
        *buffer = NULL;
        *size = 0;
        return false;
    }
    if (nullTerminate)
        *(*buffer + length) = 0;
    return true;
}

// The length is in bytes. The textBuffer must have length*2 characters.
// No attempt is made to check for a null terminator in textBuffer.
// If textBuffer contains any bytes that are not ASCII 0-9 a-f A-F, then
// the method returns false, and byteBuffer will be in a corrupted state.
// Otherwise, it returns true.
bool decodeHexBytes(char* textBuffer, uint8_t* byteBuffer, uint32_t length) {
    uint32_t index;
    for (index = 0; index < length; index += 1) {
        uint8_t byteValue = 0;
        bool highByte = false;
        do {
            char digit = textBuffer[index * 2 + (highByte ? 0 : 1)];
            uint8_t value;
            switch (digit) {
                case '0': value = 0; break;
                case '1': value = 1; break;
                case '2': value = 2; break;
                case '3': value = 3; break;
                case '4': value = 4; break;
                case '5': value = 5; break;
                case '6': value = 6; break;
                case '7': value = 7; break;
                case '8': value = 8; break;
                case '9': value = 9; break;
                case 'A': case 'a': value = 10; break;
                case 'B': case 'b': value = 11; break;
                case 'C': case 'c': value = 12; break;
                case 'D': case 'd': value = 13; break;
                case 'E': case 'e': value = 14; break;
                case 'F': case 'f': value = 15; break;
                default:
                  return false;
            }
            if (highByte)
                value <<= 4;
            byteValue += value;
            highByte = !highByte;
        } while (highByte);
        byteBuffer[index] = byteValue;
    }
}


// NETWORK

#define macAddressSize 6

struct network_t {
    struct sockaddr* serverAddress;
    uint32_t serverAddressLength;
    struct sockaddr* localAddress;
    uint32_t localAddressLength;
    uint32_t sendSocket;
    uint32_t receiveSocket;
    uint8_t localMacAddress[macAddressSize];
};

void initNetwork(char* serverName, uint16_t sendPort, uint16_t receivePort, uint8_t localMacAddress[macAddressSize], struct network_t* network) {
    // Resolve the server name.
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo* serverAddresses;
    uint32_t result = getaddrinfo(serverName, NULL, &hints, &serverAddresses);
    if (result != 0) {
        printLog("Failed to resolve server address \"%s\": %s", serverName, gai_strerror(result));
        exit(1);
    }

    // Configure the sending socket.
    assert(serverAddresses != NULL);
    network->serverAddressLength = serverAddresses->ai_addrlen;
    network->serverAddress = malloc(network->serverAddressLength);
    memcpy(network->serverAddress, serverAddresses->ai_addr, network->serverAddressLength);
    freeaddrinfo(serverAddresses);
    switch (network->serverAddress->sa_family) {
      case AF_INET:
        ((struct sockaddr_in*)(network->serverAddress))->sin_port = htons(sendPort);
        uint8_t* ip = (uint8_t*)&(((struct sockaddr_in*)(network->serverAddress))->sin_addr.s_addr);
        printLog("Using IPv4; server %s is at: %hhd.%hhd.%hhd.%hhd", serverName, *ip, *(ip + 1), *(ip + 2), *(ip + 3));
        break;
      case AF_INET6:
        ((struct sockaddr_in6*)(network->serverAddress))->sin6_port = htons(sendPort);
        printLog("Using IPv6.");
        break;
      default:
        assert(0);
    }
    network->sendSocket = socket(network->serverAddress->sa_family, SOCK_DGRAM, 0);
    if (network->sendSocket < 0) {
        printError("Failed to open UDP socket for sending");
        exit(1);
    }

    // Configure the receiving socket.
    network->receiveSocket = socket(AF_INET6, SOCK_DGRAM, 0);
    if (network->receiveSocket < 0) {
        printError("Failed to open UDP socket for receiving");
        exit(1);
    }
    socklen_t one = 1;
    if (setsockopt(network->receiveSocket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))) {
        printError("Failed to open UDP socket for receiving");
        exit(1);
    }
    network->localAddressLength = sizeof(struct sockaddr_in6);
    network->localAddress = malloc(network->localAddressLength);
    memset(network->localAddress, 0, network->localAddressLength);
    ((struct sockaddr_in6*)(network->localAddress))->sin6_family = AF_INET6;
    ((struct sockaddr_in6*)(network->localAddress))->sin6_port = htons(receivePort);
    ((struct sockaddr_in6*)(network->localAddress))->sin6_addr = in6addr_any;
    if (bind(network->receiveSocket, network->localAddress, network->localAddressLength) < 0) {
        printError("Failed to bind UDP socket for receiving");
        exit(1);
    }

    // Configure MAC address.
    memcpy(network->localMacAddress, localMacAddress, macAddressSize);
}

void doneNetwork(struct network_t* network) {
    free(network->serverAddress);
    network->serverAddress = NULL;
    network->serverAddressLength = 0;
    close(network->sendSocket);
    network->sendSocket = 0;
    free(network->localAddress);
    network->localAddress = NULL;
    network->localAddressLength = 0;
    close(network->receiveSocket);
    network->receiveSocket = 0;
}

uint32_t packetSize = macAddressSize + 1 + 1 + 2;
uint32_t macAddressOffset = 0;
uint32_t padding1Offset = macAddressSize + 0;
uint32_t flagsOffset = macAddressSize + 1;
uint32_t valueOffset = macAddressSize + 2;

void sendToNetwork(struct network_t* network, uint16_t value, bool button) {
    uint8_t buffer[packetSize];
    memcpy(&buffer[macAddressOffset], network->localMacAddress, macAddressSize);
    buffer[padding1Offset] = 0x00;
    buffer[flagsOffset] = 0x00;
    if (button)
      buffer[flagsOffset] |= 0x01;
    buffer[valueOffset + 0] = (uint8_t)((value >> 8) & 0xFF);
    buffer[valueOffset + 1] = (uint8_t)(value & 0xFF);
    uint32_t sentBytes = sendto(network->sendSocket, &buffer, sizeof(buffer), 0, network->serverAddress, network->serverAddressLength);
    if (sentBytes < 0) {
        printLog("Failed to send to UDP socket: %m");
        return;
    }
}

struct packet_t {
    bool setLed;
    enum color color;
    bool setOutput;
    uint16_t value;
};

#define bufferSize 10

bool receiveFromNetwork(struct network_t* network, struct packet_t* result) {
    uint8_t buffer[bufferSize];
    uint32_t bytes = recv(network->receiveSocket, buffer, bufferSize, MSG_DONTWAIT | MSG_TRUNC);
    if (bytes < 0) {
        printLog("Failed to receive from UDP socket: %m");
        return false;
    }
    if (bytes < bufferSize) {
        printLog("Received malformed packet on UDP socket: %m");
        return false;
    }
    if (buffer[0] != network->localMacAddress[0] ||
        buffer[1] != network->localMacAddress[1] ||
        buffer[2] != network->localMacAddress[2] ||
        buffer[3] != network->localMacAddress[3] ||
        buffer[4] != network->localMacAddress[4] ||
        buffer[5] != network->localMacAddress[5]) {
        printLog("Received packet on UDP socket intended for another cloudbit");
        return false;
    }
    result->setLed = (buffer[6] & 0x80) > 0;
    result->color = result->setLed ? buffer[6] & 0x07 : 0x00;
    result->setOutput = (buffer[7] & 0x80) > 0;
    result->value = result->setOutput ? (buffer[8] << 8) + buffer[9] : 0xFFFF;
    return true;
}


// LOW-LEVEL HARDWARE ACCESS

void poke(uint32_t* page, uint32_t offset, uint32_t value) {
#ifdef VERBOSE
    printLog("poke 0x%08x + 0x%04x : %8x", page, offset, value);
#endif
    *(volatile uint32_t*)((uint32_t)page + offset) = value;
}

uint32_t peek(uint32_t* page, uint32_t offset) {
    uint32_t result = *(volatile uint32_t*)((uint32_t)page + offset);
#ifdef VERBOSE
    printLog("peek 0x%08x + 0x%04x : %8x", page, offset, result);
#endif
    return result;
}



// HIGH-LEVEL HARDWARE ACCESS

// - ADC

uint16_t readInput(uint32_t* page) {
    const uint32_t atTriggerBefore = 0x0004;
    const uint32_t atTriggerAfter = 0x0018;
    const uint32_t atValue = 0x0050;
    uint32_t newValue;
    bool wasHighBitSet = (newValue = peek(page, atValue)) >= 0x80000000;
    poke(page, atTriggerBefore, 0x00000001);
    while (wasHighBitSet == ((newValue = peek(page, atValue)) >= 0x80000000)) { }
    poke(page, atTriggerAfter, 0x00000001);
    newValue = newValue &~ 0x80000000;
    if (newValue < 200)
      newValue = 200;
    if (newValue > 1700)
      newValue = 1700;
    newValue = ((newValue - 200) * 0xFFFF) / 1500;
    assert((newValue & 0xFFFF) == newValue);
    return (uint16_t)newValue;
}


// - BUTTON

bool readButton(uint32_t* page) {
    const uint32_t buttonOffset = 0x0610;
    uint32_t value = peek(page, buttonOffset);
    return (value & 0x00000080) == 0;
}


// - DAC

uint32_t lastDacReadyFlag;

void writeOutput(uint32_t* page, uint32_t value) {
    uint32_t counter = 0;
    do {
        uint32_t readyFlag;
        while (((readyFlag = peek(page, 0x0040) & 0x00000002) == lastDacReadyFlag) && counter < 100) {
            delay(0.0000000001);
            counter += 1;
        }
        // if we wait more than 100ns, just stop waiting and write the value regardless
        poke(page, 0x00F0, (value + 0x8000) % 0x10000);
        lastDacReadyFlag = readyFlag;
    } while (counter < 20); // try to write the value up to 20 times
}


// - LED

void setColor(uint32_t* page, enum color value) {
  // green channel
  if ((value & 0b100) > 0) {
    poke(page, 0x0508, 0x40000000);
  } else {
    poke(page, 0x0504, 0x40000000);
  }
  // red channel
  if ((value & 0b010) > 0) {
    poke(page, 0x0508, 0x80000000);
  } else {
    poke(page, 0x0504, 0x80000000);
  }
  // blue channel
  if ((value & 0b001) > 0) {
    poke(page, 0x0518, 0x10000000);
  } else {
    poke(page, 0x0514, 0x10000000);
  }
}



// MAIN

void main(int argc, char** argv) {
    printLog("initializing...");

    // HARDWARE

    int32_t fd = open("/dev/mem", O_RDWR);
    if (fd < 0) {
        printError("Failed to open /dev/mem");
        exit(1);
    }
    
    // Map the 8KB page around the GPIO hardware memory.
    uint32_t* gpioPage = mmap(NULL, 0x1fff, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x80018000);
    if (gpioPage == MAP_FAILED) {
        printError("Failed to mmap /dev/mem for LED hardware");
        exit(1);
    }
    // Initialize the LED hardware.
    // This sequence based on LEDcolor.d and button.d.
    poke(gpioPage, 0x0114, 0xF0000000); // LED
    poke(gpioPage, 0x0124, 0x0000C000); // button
    poke(gpioPage, 0x0134, 0x03000000); // LED
    poke(gpioPage, 0x0704, 0x40000000); // LED
    poke(gpioPage, 0x0714, 0x10000000); // LED
    poke(gpioPage, 0x0718, 0x00000080); // button

    // Map the 8KB page around the DAC hardware memory.
    uint32_t* dacPage = mmap(NULL, 0x1fff, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x80048000);
    if (dacPage == MAP_FAILED) {
        printError("Failed to mmap /dev/mem for DAC hardware");
        exit(1);
    }
    // The DAC hardware is initialized by DAC_init.
    lastDacReadyFlag = ~peek(dacPage, 0x0040) & 0x00000002; // assume it's ready

    // Map the 8KB page around the ADC hardware memory.
    uint32_t* adcPage = mmap(NULL, 0x1fff, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x80050000);
    if (adcPage == MAP_FAILED) {
        printError("Failed to mmap /dev/mem for ADC hardware");
        exit(1);
    }
    // Initialize the ADC hardware.
    // This sequence based on ADC.d.
    poke(adcPage, 0x0008, 0x40000000);
    poke(adcPage, 0x0004, 0x00000001);
    poke(adcPage, 0x0028, 0x01000000);
    poke(adcPage, 0x0014, 0x00010000);
    poke(adcPage, 0x0034, 0x00000001);
    poke(adcPage, 0x0024, 0x01000000);
    poke(adcPage, 0x0144, 0x00000000);

    if (close(fd) < 0) {
        printError("Failed to close /dev/mem");
        exit(1);
    }


    // NETWORK

    char* serverName;
    uint32_t serverNameLength = 0;
    if (!readFile("/root/localbit_server.cfg", (uint8_t**)&serverName, &serverNameLength, true)) {
        printLog("Failed to open server configuration file.");
        exit(1);
    }
    *(char*)(strchrnul(serverName, 0x0A)) = '\0';
    printLog("Configured localbit server hostname is \"%s\"", serverName);

    char* localMacAddressString;
    uint32_t localMacAddressStringLength = 12;
    if (!readFile("/root/mac_address.cfg", (uint8_t**)&localMacAddressString, &localMacAddressStringLength, false)) {
        printLog("Failed to open MAC address file.");
        exit(1);
    }
    uint8_t localMacAddress[macAddressSize];
    decodeHexBytes(localMacAddressString, localMacAddress, macAddressSize);
    printLog("Configured cloudbit device identity is \"%02x:%02x:%02x:%02x:%02x:%02x\"",
      *(localMacAddress+0),
      *(localMacAddress+1),
      *(localMacAddress+2),
      *(localMacAddress+3),
      *(localMacAddress+4),
      *(localMacAddress+5)
    );

    struct network_t network;
    initNetwork(serverName, 2020, 2021, localMacAddress, &network);

    free(serverName);
    free(localMacAddressString);

    
    // PROGRAM LOOP

    setColor(gpioPage, black);
    writeOutput(dacPage, 0x0000);
    printLog("ready");

    uint32_t lastValue = 0;
    double pause = 0.0;
    while (true) {
      if (socketHasMessage(network.receiveSocket, pause)) {
          struct packet_t packet;
          if (receiveFromNetwork(&network, &packet)) {
              printLog("received message: setLed=%02x color=%02x setOutput=%02x value=%04x", packet.setLed, packet.color, packet.setOutput, packet.value);
              if (packet.setLed)
                  setColor(gpioPage, packet.color);
              if (packet.setOutput)
                  writeOutput(dacPage, packet.value);
          }
      }

      bool button = readButton(gpioPage);
      uint16_t value = readInput(adcPage);
      printLog("input: %04x  button: %02x", value, button);
      sendToNetwork(&network, value, button);

      int delta = abs(lastValue - value);
      lastValue = value;
      if (delta > 0x500 || button) {
        pause = 0.00;
      } else if (delta > 0x200) {
        pause -= 0.01;
      } else {
        pause += 0.01;
      }
      if (pause < 0.0)
        pause = 0.0;
      if (pause > 2.0)
        pause = 2.0;
      delay(0.01);
    }

    doneNetwork(&network);
}
