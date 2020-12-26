
#define ARDUINOTRACE_SERIAL Serial2
#define ARDUINOTRACE_ENABLE 0
#define RXD2 16
#define TXD2 17
#include <ArduinoTrace.h>

#include <Arduino.h>

#include "XSPI.h"
#include "XNAND.h"
#include "defs.h"

#define SERIAL_BUFFER_SIZE 0x8400

#define USBSERIAL Serial

uint32_t read_param_le(void);
void StartFlashRead(uint32_t, uint32_t);
void StartFlashWrite(uint32_t, uint32_t);
void FlashInit(void);
void FlashDeinit(void);
void FlashStatus(void);

uint16_t STATUS_D = 0;
const uint PAGE_SIZE = 0x210;
uint8_t PAGE_BUFFER[PAGE_SIZE] = {};

void setup()
{
    // put your setup code here, to run once:
    USBSERIAL.begin(115200);
    USBSERIAL.setRxBufferSize(SERIAL_BUFFER_SIZE);
#if ARDUINOTRACE_ENABLE == 1
    ARDUINOTRACE_SERIAL.begin(9600, SERIAL_8N1, RXD2, TXD2);
#endif
    TRACE();
    XSPI_Init();
}

void loop()
{
    if (USBSERIAL.available())
    {
        int16_t command = USBSERIAL.read(); // serial.read is 0-255 or -1 :(
        DUMP(command);
        if (command > 0)
        {
            uint32_t paramA = read_param_le();
            DUMP(paramA);
            uint32_t paramB = read_param_le();
            DUMP(paramB);
            switch (command)
            {
            case REQ_DATAREAD:
                StartFlashRead(paramA << 5, paramB);
                break;
            case REQ_DATAWRITE:
                StartFlashWrite(paramA << 5, paramB);
                break;
            case REQ_DATAINIT:
                FlashInit();
                break;
            case REQ_DATADEINIT:
                FlashDeinit();
                break;
            case REQ_DATASTATUS:
                FlashStatus();
                break;
            }
        }
    }
}

void StartFlashRead(uint32_t startPage, uint32_t len)
{
    uint8_t *buff_ptr = PAGE_BUFFER;
    STATUS_D = 0;
    uint32_t wordsLeft = 0;
    uint32_t nextPage = startPage;

    len /= 4;
    while (len)
    {
        uint8_t readNow;

        if (!wordsLeft)
        {
            STATUS_D = XNAND_StartRead(nextPage);
            nextPage++;
            wordsLeft = PAGE_SIZE / 4;
            buff_ptr = PAGE_BUFFER;
        }

        readNow = (len < wordsLeft) ? len : wordsLeft;
        XNAND_ReadFillBuffer(buff_ptr, readNow);

        buff_ptr += (readNow * 4);
        wordsLeft -= readNow;
        len -= readNow;

        if (!wordsLeft)
        {
            USBSERIAL.write(PAGE_BUFFER, PAGE_SIZE);
        }
    }
}

void StartFlashWrite(uint32_t startPage, uint32_t len)
{
    TRACE();
    STATUS_D = XNAND_Erase(startPage);
    DUMP(STATUS_D);
    XNAND_StartWrite();

    uint8_t *buff_ptr = PAGE_BUFFER;
    uint32_t wordsLeft = 0;
    uint32_t nextPage = startPage;

    len /= 4;
    DUMP(len);
    while (len)
    {
        uint8_t writeNow;

        if (!wordsLeft)
        {
            while (USBSERIAL.available() < PAGE_SIZE)
            {
            }
            USBSERIAL.readBytes((char *)PAGE_BUFFER, PAGE_SIZE);
            buff_ptr = PAGE_BUFFER;
            wordsLeft = PAGE_SIZE / 4;
        }

        writeNow = (len < wordsLeft) ? len : wordsLeft;
        XNAND_WriteProcess(buff_ptr, writeNow);
        buff_ptr += (writeNow * 4);
        wordsLeft -= writeNow;
        len -= writeNow;

        //execute write if buffer in NAND controller is filled
        if (!wordsLeft)
        {
            STATUS_D = XNAND_WriteExecute(nextPage++);
            XNAND_StartWrite();
        }
    }
}

void FlashInit(void)
{
    TRACE();
    XSPI_EnterFlashmode();

    uint8_t FlashConfig[4];
    XSPI_Read(0, FlashConfig); //2 times?
    XSPI_Read(0, FlashConfig);
    USBSERIAL.write(FlashConfig, 4);
}

void FlashDeinit()
{
    TRACE();
    XSPI_LeaveFlashmode();
}

void FlashStatus()
{
    TRACE();
    uint8_t FlashStatus[2] = {(uint8_t)(STATUS_D & 0xFF), (uint8_t)(STATUS_D >> 8)};
    USBSERIAL.write(FlashStatus, 2);
}

uint32_t read_param_le()
{
    uint32_t buffer = 0;
    int16_t read = -1;
    for (int i = 0; i < 4;)
    {
        if (USBSERIAL.available())
        {
            read = USBSERIAL.read();
            buffer = (buffer >> 8) | (read << 24);
            i++;
        }
    }
    return buffer;
}