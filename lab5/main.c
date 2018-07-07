/****************************************************************************************
 * File name: main.c
 * Program Objective: To understand how to use the CC3100 SimpleLink Wi-Fi BoosterPack to
 * connect to a wifi access point and transfer data to a web server.
 * Objectives    : 1. CC3100 can connect to `Embedded_Lab_EXT` Wi-Fi network.
                   2. CC3100 can communicate with HTTP server with aid of GET method.
                   An appropriate GET URL is generated based on current POT value and it
                   is sent to the HTTP server.
                   3. HTTP response, i.e. JSON value, is parsed and the final error output
                   is shown on terminal panel in CCS software.
                   4. Timer and ADC peripherals are activated, and they are handled by using
                   interrupt services. The system can send POT value every 5 seconds
                   automatically.
                   5. The correctness of recent saved POT values are tested by using
                   `show` functionality.
 * Description : CC3100 SimpleLink Wi-Fi module is stacked on top of the Tiva board and pot is
 * connected to the pin PE3 of Tiva board. When the program starts executing, the CC3100 module
 * establishes connection with the wifi access point and it gets connected to the internet.
 * After every five seconds the pot value is send to the server using GET method. The pot value
 * is updated in the table given in the web site and it is accessed using the
 * URL - http://192.168.2.18/?func=show&ID=xxxxxxxx. The ADC0 and Timer0 modules are enabled to
 * convert the pot values to digital and send them to the web server respectively. The JSON tokens
 * and error value are printed on the terminal after the value is successfully received.
 * Externally modified files: user.h, ssock.h, sl_common.h
 * TI provided code http_client is used in this program and the copyright goes to,
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 * References : [1]Lab5-V1.1.pdf
 *              [2]Embedded System Design using TM4C LaunchPadTM Development Kit,SSQU015(Canvas file)
 *              [3]Tiva C Series TM4C123G LaunchPad Evaluation Board User's Guide
 *              [4]Tiva TM4C123GH6PM Microcontroller datasheet
 *              [5]TivaWare Peripheral Driver Library User guide
 *              [6]http://processors.wiki.ti.com/index.php/Tiva_TM4C123G_LaunchPad_Blink_the_RGB
 *              [7]Embedded Systems An Introduction using the Renesas RX63N Microcontroller BY JAMES M. CONRAD
 *              [8]http://processors.wiki.ti.com/index.php/CC31xx_HTTP_Client
 *              [9]http://processors.wiki.ti.com/index.php/CC3100_Getting_Started_with_WLAN_Station
 *              [10]http://processors.wiki.ti.com/index.php/CC3100_%26_CC3200
 *              [11]https://www.youtube.com/watch?v=lRlCoXHEMKw
 *              [12]https://e2e.ti.com/support/wireless_connectivity/simplelink_wifi_cc31xx_cc32xx/f/968/p/497053/1798332
 *              [13]http://shukra.cedt.iisc.ernet.in/edwiki/EmSys:TM4C123G_LaunchPad_-_Interrupts_and_the_Timers
 *              [14]CC3100 SimpleLink Wi-Fi® and IoT Solution BoosterPack Hardware User's Guide
 *              [15]http://www.ti.com/lit/ds/swas031d/swas031d.pdf
 *              [16]http://www.mouser.com/ds/2/405/cc3100-469564.pdf
 *              [17]http://software-dl.ti.com/dsps/dsps_public_sw/sdo_sb/targetcontent/tirtos/2_14_04_31/exports/
 *              tirtos_full_2_14_04_31/docs/networkservices/doxygen/html/group__ti__net__http___h_t_t_p_cli.html
 *********************************************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "simplelink.h"
#include "sl_common.h"
#include "stdint.h"
#include "httpcli.h"
#include "httpcli.c"
#include "jsmn.h"
#include "jsmn.c"
#include "ssock.h"
#include "ssock.c"
#include"inc/hw_memmap.h"
#include"driverlib/gpio.h"
#include"inc/hw_types.h"
#include"driverlib/debug.h"
#include"driverlib/sysctl.h"
#include"driverlib/adc.h"
#include"driverlib/uart.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include"driverlib/pin_map.h"
#include "inc/hw_ints.h"
#include"driverlib/uart.h"

static _i32 establishConnectionWithAP();
static _i32 configureSimpleLinkToDefaultState();
static _i32 initializeAppVariables();
static void  displayBanner();
static _i32 ConnectToHTTPServer(HTTPCli_Handle httpClient);
static _i32 HTTPGetMethod(HTTPCli_Handle httpClient);
static _i32 readResponse(HTTPCli_Handle httpClient);
static void FlushHTTPResponse(HTTPCli_Handle httpClient);
static _i32 ParseJSONData(_i8 *ptr);
void Timer0AIntHandler(void);
void TimerInitAndStart(void);
void ADC0InitAndTrigger(void);

#define APPLICATION_VERSION "1.2.0"
#define SL_STOP_TIMEOUT        0xFF
#define POST_REQUEST_URI       "/POST"
#define POST_DATA              "{\n\"name\":\"xyz\",\n\"address\":\n{\n\"plot#\":12,\n\"street\":\"abc\",\n\"city\":\"ijk\"\n},\n\"age\":30\n}"
#define DELETE_REQUEST_URI     "/delete"
#define PUT_REQUEST_URI        "/put"
#define PUT_DATA               "PUT request."
#define HOST_NAME              "192.168.2.18"
#define HOST_PORT              80
#define PROXY_IP               0xBA5FB660
#define PROXY_PORT             0xC0A80212
#define READ_SIZE       1450
#define MAX_BUFF_SIZE   1460
#define SPACE           32

char GET_REQUEST_URI[32] =       "/?func=save&ID=xxxxxxxxx&POT=";
uint32_t ui32FlagToCheckTimer = 0;
uint32_t ui32ADC0DigitalValue[1], ui32ADCValueStore;
_u32 g_Status;
_u32 g_DestinationIP;
_u32 g_BytesReceived; /* variable to store the file size */
_u8  g_buff[MAX_BUFF_SIZE+1];
_i32 g_SockID = 0;

typedef enum{
    DEVICE_NOT_IN_STATION_MODE = -0x7D0,
    INVALID_HEX_STRING = DEVICE_NOT_IN_STATION_MODE - 1,
    TCP_RECV_ERROR = INVALID_HEX_STRING - 1,
    TCP_SEND_ERROR = TCP_RECV_ERROR - 1,
    FILE_NOT_FOUND_ERROR = TCP_SEND_ERROR - 1,
    INVALID_SERVER_RESPONSE = FILE_NOT_FOUND_ERROR - 1,
    FORMAT_NOT_SUPPORTED = INVALID_SERVER_RESPONSE - 1,
    FILE_WRITE_ERROR = FORMAT_NOT_SUPPORTED - 1,
    INVALID_FILE = FILE_WRITE_ERROR - 1,

    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

/**********************************************************************************************
 * Function name: SimpleLinkWlanEventHandler
 * Inputs: SlWlanEvent_t *pWlanEvent
 * Description: This function handles WLAN events
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

void SimpleLinkWlanEventHandler(SlWlanEvent_t *pWlanEvent)
{
    if(pWlanEvent == NULL)
    {
        CLI_Write(" [WLAN EVENT] NULL Pointer Error \n\r");
        return;
    }

    switch(pWlanEvent->Event)
    {
        case SL_WLAN_CONNECT_EVENT:
        {
            SET_STATUS_BIT(g_Status, STATUS_BIT_CONNECTION);
        }
        break;

        case SL_WLAN_DISCONNECT_EVENT:
        {
            slWlanConnectAsyncResponse_t*  pEventData = NULL;

            CLR_STATUS_BIT(g_Status, STATUS_BIT_CONNECTION);
            CLR_STATUS_BIT(g_Status, STATUS_BIT_IP_ACQUIRED);

            pEventData = &pWlanEvent->EventData.STAandP2PModeDisconnected;
            if(SL_WLAN_DISCONNECT_USER_INITIATED_DISCONNECTION == pEventData->reason_code)
            {
                CLI_Write(" Device disconnected from the AP on application's request \n\r");
            }
            else
            {
                CLI_Write(" Device disconnected from the AP on an ERROR..!! \n\r");
            }
        }
        break;

        default:
        {
            CLI_Write(" [WLAN EVENT] Unexpected event \n\r");
        }
        break;
    }
}

/**********************************************************************************************
 * Function name: SimpleLinkNetAppEventHandler
 * Inputs: SlNetAppEvent_t *pNetAppEvent
 * Description: This function handles events for IP address acquisition via DHCP indication
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

void SimpleLinkNetAppEventHandler(SlNetAppEvent_t *pNetAppEvent)
{
    if(pNetAppEvent == NULL)
    {
        CLI_Write(" [NETAPP EVENT] NULL Pointer Error \n\r");
        return;
    }

    switch(pNetAppEvent->Event)
    {
        case SL_NETAPP_IPV4_IPACQUIRED_EVENT:
        {
            SET_STATUS_BIT(g_Status, STATUS_BIT_IP_ACQUIRED);
        }
        break;

        default:
        {
            CLI_Write(" [NETAPP EVENT] Unexpected event \n\r");
        }
        break;
    }
}

/**********************************************************************************************
 * Function name: SimpleLinkHttpServerCallback
 * Inputs: SlHttpServerEvent_t *pHttpEvent, SlHttpServerResponse_t *pHttpResponse
 * Description: This function handles callback for the HTTP server events
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

void SimpleLinkHttpServerCallback(SlHttpServerEvent_t *pHttpEvent,
                                  SlHttpServerResponse_t *pHttpResponse)
{
    CLI_Write(" [HTTP EVENT] Unexpected event \n\r");
}

/**********************************************************************************************
 * Function name: SimpleLinkGeneralEventHandler
 * Inputs: SlDeviceEvent_t *pDevEvent
 * Description: This function handles general error events indication
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

void SimpleLinkGeneralEventHandler(SlDeviceEvent_t *pDevEvent)
{
    CLI_Write(" [GENERAL EVENT] \n\r");
}

/**********************************************************************************************
 * Function name: SimpleLinkSockEventHandler
 * Inputs: SlSockEvent_t *pSock
 * Description: This function handles socket events indication
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

void SimpleLinkSockEventHandler(SlSockEvent_t *pSock)
{
    if(pSock == NULL)
    {
        CLI_Write(" [SOCK EVENT] NULL Pointer Error \n\r");
        return;
    }

    switch( pSock->Event )
    {
        case SL_SOCKET_TX_FAILED_EVENT:
        {
            switch( pSock->socketAsyncEvent.SockTxFailData.status )
            {
                case SL_ECLOSE:
                    CLI_Write(" [SOCK EVENT] Close socket operation failed to transmit all queued packets\n\r");
                break;


                default:
                    CLI_Write(" [SOCK EVENT] Unexpected event \n\r");
                break;
            }
        }
        break;

        default:
            CLI_Write(" [SOCK EVENT] Unexpected event \n\r");
        break;
    }
}

/**********************************************************************************************
 * Function name: main
 * Description: This function Stop WDT and initialize the system-clock of the MCU, configures
 * command line interface, displays banner, configures device in default state,
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

int main(int argc, char** argv)
{
   _i32            retVal = -1;
   HTTPCli_Struct     httpClient;
   int ui32DigitFromADCValue;
   int ui32DigitExtraction;
   stopWDT();
   initClk();
   CLI_Configure();
   displayBanner();
   retVal = initializeAppVariables();
   ASSERT_ON_ERROR(retVal);
   retVal = configureSimpleLinkToDefaultState();
   if(retVal < 0)
   {
       if (DEVICE_NOT_IN_STATION_MODE == retVal)
       {
           CLI_Write(" Failed to configure the device in its default state \n\r");
       }
       LOOP_FOREVER();
   }
   CLI_Write(" Device is configured in default state \n\r");

   TimerInitAndStart();

   while(1)
   {
   retVal = sl_Start(0, 0, 0);
   if ((retVal < 0) || (ROLE_STA != retVal) )
   {
       CLI_Write(" Failed to start the device \n\r");
       LOOP_FOREVER();
   }
   CLI_Write(" Device started as STATION \n\r");
   retVal = establishConnectionWithAP();
   if(retVal < 0)
   {
       CLI_Write(" Failed to establish connection w/ an AP \n\r");
       LOOP_FOREVER();
   }
   CLI_Write(" Connection established w/ AP and IP is acquired \n\r");
   retVal = ConnectToHTTPServer(&httpClient);
   if(retVal < 0)
   {
       LOOP_FOREVER();
   }
   ADC0InitAndTrigger();
   if(ui32FlagToCheckTimer)
   {
       CLI_Write("\n\r");
       CLI_Write(" HTTP Get Test Begin:\n\r");

       /*The pot value is dynamically appended with the GET_REQUEST_URI to update it in the server.
        * When the get request starts, the pot value is scaled to a value from 0 to 255 by dividing
        * it by 16. The MSB to LSB digits are extracted and converted to ASCII by adding 48 to those digits.
        * These ASCII numbers are appended at the end of the string "/?func=save&ID=xxxxxxxxx&POT=" to
        * save the values to the server.*/
       ui32DigitExtraction = (int)ui32ADCValueStore/16;
       ui32DigitFromADCValue = ui32DigitExtraction/100;
       ui32DigitExtraction = ui32DigitExtraction-ui32DigitFromADCValue*100;
       GET_REQUEST_URI[29]= ui32DigitFromADCValue+48;
       ui32DigitFromADCValue = ui32DigitExtraction/10;
       GET_REQUEST_URI[30] = ui32DigitFromADCValue+48;
       ui32DigitExtraction = ui32DigitExtraction-ui32DigitFromADCValue*10;
       GET_REQUEST_URI[31] = ui32DigitExtraction+48;
       retVal = HTTPGetMethod(&httpClient);
       ui32FlagToCheckTimer = 0;
       if(retVal < 0)
       {
           CLI_Write(" HTTP Get Test failed.\n\r");
       }
       CLI_Write(" HTTP Get Test Completed Successfully\n\r");
       CLI_Write("\n\r");
   }
   }
   retVal = sl_Stop(SL_STOP_TIMEOUT);
   if(retVal < 0)
   {
       LOOP_FOREVER();
   }
   return SUCCESS;
}

/**********************************************************************************************
 * Function name: HTTPGetMethod
 * Inputs: HTTPCli_Handle httpClient
 * Outputs: retVal
 * Description: This function demonstrate the HTTP GET method
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

static _i32 HTTPGetMethod(HTTPCli_Handle httpClient)
{
    _i32             retVal = 0;
    bool             moreFlags;
    const HTTPCli_Field    fields[4] = {
                                    {HTTPCli_FIELD_NAME_HOST, HOST_NAME},
                                    {HTTPCli_FIELD_NAME_ACCEPT, "*/*"},
                                    {HTTPCli_FIELD_NAME_CONTENT_LENGTH, "0"},
                                    {NULL, NULL}
                                };

    HTTPCli_setRequestFields(httpClient, fields);

    moreFlags = 0;
    CLI_Write(GET_REQUEST_URI);
    CLI_Write("\n");
    retVal = HTTPCli_sendRequest(httpClient, HTTPCli_METHOD_GET, GET_REQUEST_URI, moreFlags);
    if(retVal < 0)
    {
        CLI_Write(" Failed to send HTTP GET request.\n\r");
        return retVal;
    }
    retVal = readResponse(httpClient);
    return retVal;
}

/**********************************************************************************************
 * Function name: readResponse
 * Inputs: HTTPCli_Handle httpClient
 * Outputs: retVal
 * Description: This function read response from server and dump on console
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

static _i32 readResponse(HTTPCli_Handle httpClient)
{
    _i32            retVal = 0;
    _i32            bytesRead = 0;
    _i32            id = 0;
    _u32            len = 0;
    _i32            json = 0;
    _i8             *dataBuffer=NULL;
    bool            moreFlags = 1;
    const _i8       *ids[4] = {
                                HTTPCli_FIELD_NAME_CONTENT_LENGTH,
                                HTTPCli_FIELD_NAME_CONNECTION,
                                HTTPCli_FIELD_NAME_CONTENT_TYPE,
                                NULL
                              };

    retVal = HTTPCli_getResponseStatus(httpClient);
    if(retVal > 0)
    {
        switch(retVal)
        {
        case 200:
        {
            CLI_Write(" HTTP Status 200\n\r");
            HTTPCli_setResponseFields(httpClient, (const char **)ids);
            while((id = HTTPCli_getResponseField(httpClient, (char *)g_buff, sizeof(g_buff), &moreFlags))
                    != HTTPCli_FIELD_ID_END)
            {

                switch(id)
                {
                case 0:
                {
                    len = strtoul((char *)g_buff, NULL, 0);
                }
                break;
                case 1:
                {
                }
                break;
                case 2:
                {
                    if(!strncmp((const char *)g_buff, "application/json",
                            sizeof("application/json")))
                    {
                        json = 1;
                    }
                    else
                    {
                        json = 0;
                    }
                    CLI_Write(" ");
                    CLI_Write(HTTPCli_FIELD_NAME_CONTENT_TYPE);
                    CLI_Write(" : ");
                    CLI_Write("application/json\n\r");
                }
                break;
                default:
                {
                    CLI_Write(" Wrong filter id\n\r");
                    retVal = -1;
                    goto end;
                }
                }
            }
            bytesRead = 0;
            if(len > sizeof(g_buff))
            {
                dataBuffer = (_i8 *) malloc(len);
                if(dataBuffer)
                {
                    CLI_Write(" Failed to allocate memory\n\r");
                    retVal = -1;
                    goto end;
                }
            }
            else
            {
                dataBuffer = (_i8 *)g_buff;
            }
            bytesRead = HTTPCli_readResponseBody(httpClient, (char *)dataBuffer, len, &moreFlags);
            if(bytesRead < 0)
            {
                CLI_Write(" Failed to received response body\n\r");
                retVal = bytesRead;
                goto end;
            }
            else if( bytesRead < len || moreFlags)
            {
                CLI_Write(" Mismatch in content length and received data length\n\r");
                goto end;
            }
            dataBuffer[bytesRead] = '\0';
            CLI_Write(dataBuffer);
            if(json)
            {
                retVal = ParseJSONData(dataBuffer);
                if(retVal < 0)
                {
                    goto end;
                }
            }
            else
            {
            }
        }
        break;
        case 404:
            CLI_Write(" File not found. \r\n");
        default:
            FlushHTTPResponse(httpClient);
            break;
        }
    }
    else
    {
        CLI_Write(" Failed to receive data from server.\r\n");
        goto end;
    }

    retVal = 0;

end:
    if(len > sizeof(g_buff) && (dataBuffer != NULL))
    {
        free(dataBuffer);
    }
    return retVal;
}

/**********************************************************************************************
 * Function name: ConnectToHTTPServer
 * Inputs: HTTPCli_Handle httpClient
 * Outputs: retVal
 * Description: This function establish a HTTP connection
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

static _i32 ConnectToHTTPServer(HTTPCli_Handle httpClient)
{
    _i32                retVal = -1;
    struct sockaddr_in     addr;

#ifdef USE_PROXY
    struct sockaddr_in     paddr;

    paddr.sin_family = AF_INET;
    paddr.sin_port = htons(PROXY_PORT);
    paddr.sin_addr.s_addr = sl_Htonl(PROXY_IP);
    HTTPCli_setProxy((struct sockaddr *)&paddr);
#endif

    retVal = sl_NetAppDnsGetHostByName(HOST_NAME, pal_Strlen(HOST_NAME),
                                       &g_DestinationIP, SL_AF_INET);
    if(retVal < 0)
    {
        CLI_Write(" Device couldn't get the IP for the host-name\r\n");
        ASSERT_ON_ERROR(retVal);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(HOST_PORT);
    addr.sin_addr.s_addr = sl_Htonl(g_DestinationIP);

    HTTPCli_construct(httpClient);
    retVal = HTTPCli_connect(httpClient, (struct sockaddr *)&addr, 0, NULL);
    if (retVal < 0)
    {
        CLI_Write("Connection to server failed\n\r");
        ASSERT_ON_ERROR(retVal);
    }

    CLI_Write(" Successfully connected to the server \r\n");
    return SUCCESS;
}

/**********************************************************************************************
 * Function name: FlushHTTPResponse
 * Inputs: HTTPCli_Handle httpClient
 * Description: This function flush HTTP response
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

static void FlushHTTPResponse(HTTPCli_Handle httpClient)
{
    const _i8       *ids[2] = {
                               HTTPCli_FIELD_NAME_CONNECTION,
                               NULL
                              };
    _i8             buf[128];
    _i32            id;
    _i32            len = 1;
    bool            moreFlag = 0;
    _i8             **prevRespFilelds = NULL;

    prevRespFilelds = (_i8 **)HTTPCli_setResponseFields(httpClient, (const char **)ids);

    while ((id = HTTPCli_getResponseField(httpClient, (char *)buf, sizeof(buf), &moreFlag))
            != HTTPCli_FIELD_ID_END)
    {

        if(id == 0)
        {
            if(!strncmp((const char *)buf, "close", sizeof("close")))
            {
                CLI_Write(" Connection terminated by server\n\r");
            }
        }
    }

    HTTPCli_setResponseFields(httpClient, (const char **)prevRespFilelds);

    while(1)
    {
        HTTPCli_readResponseBody(httpClient, (char *)buf, sizeof(buf) - 1, &moreFlag);
        CLI_Write((_u8 *)buf);
        CLI_Write("\r\n");

        if ((len - 2) >= 0 && buf[len - 2] == '\r' && buf [len - 1] == '\n')
        {
        }

        if(!moreFlag)
        {
            break;
        }
    }
}

/**********************************************************************************************
 * Function name: ParseJSONData
 * Inputs: _i8 *ptr
 * Outputs: retVal
 * Description: This function parse JSON data
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

static _i32 ParseJSONData(_i8 *ptr)
{
    _i32            retVal = 0;
    _i32            noOfToken;
    jsmn_parser     parser;
    jsmntok_t       tokenList[3];
    _i8             printBuffer[4];

    jsmn_init(&parser);
    noOfToken = jsmn_parse(&parser, (const char *)ptr, strlen((const char *)ptr), NULL, 10);
    if(noOfToken <= 0)
    {
        CLI_Write(" Failed to initialize JSON parser\n\r");
        return -1;

    }
    if(tokenList == NULL)
    {
        CLI_Write(" Failed to allocate memory\n\r");
        return -1;
    }
    jsmn_init(&parser);
    noOfToken = jsmn_parse(&parser, (const char *)ptr, strlen((const char *)ptr), tokenList, noOfToken);
    if(noOfToken < 0)
    {
        CLI_Write(" Failed to parse JSON tokens\n\r");
        retVal = noOfToken;
    }
    else
    {
        CLI_Write(" Successfully parsed ");
        sprintf((char *)printBuffer, "%ld", noOfToken);
        CLI_Write((_u8 *)printBuffer);
        CLI_Write(" JSON tokens\n\r");
    }

    free(tokenList);

    return retVal;
}

/**********************************************************************************************
 * Function name: configureSimpleLinkToDefaultState
 * Outputs: retVal
 * Description: This function configure the SimpleLink device in its default state. It Sets the
 * mode to STATION, Configures connection policy to Auto and AutoSmartConfig, Deletes all the
 * stored profiles, Enables DHCP, Disables Scan policy, Sets Tx power to maximum, Sets power
 * policy to normal, Unregisters mDNS services, Remove all filters
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

static _i32 configureSimpleLinkToDefaultState()
{
    SlVersionFull   ver = {0};
    _WlanRxFilterOperationCommandBuff_t  RxFilterIdMask = {0};

    _u8           val = 1;
    _u8           configOpt = 0;
    _u8           configLen = 0;
    _u8           power = 0;

    _i32          retVal = -1;
    _i32          mode = -1;

    mode = sl_Start(0, 0, 0);
    ASSERT_ON_ERROR(mode);

    if (ROLE_STA != mode)
    {
        if (ROLE_AP == mode)
        {
            while(!IS_IP_ACQUIRED(g_Status)) { _SlNonOsMainLoopTask(); }
        }
        retVal = sl_WlanSetMode(ROLE_STA);
        ASSERT_ON_ERROR(retVal);

        retVal = sl_Stop(SL_STOP_TIMEOUT);
        ASSERT_ON_ERROR(retVal);

        retVal = sl_Start(0, 0, 0);
        ASSERT_ON_ERROR(retVal);

        if (ROLE_STA != retVal)
        {
            ASSERT_ON_ERROR(DEVICE_NOT_IN_STATION_MODE);
        }
    }

    configOpt = SL_DEVICE_GENERAL_VERSION;
    configLen = sizeof(ver);
    retVal = sl_DevGet(SL_DEVICE_GENERAL_CONFIGURATION, &configOpt, &configLen, (_u8 *)(&ver));
    ASSERT_ON_ERROR(retVal);
    retVal = sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(1, 0, 0, 0, 1), NULL, 0);
    ASSERT_ON_ERROR(retVal);

    retVal = sl_WlanProfileDel(0xFF);
    ASSERT_ON_ERROR(retVal);

    retVal = sl_WlanDisconnect();
    if(0 == retVal)
    {
        while(IS_CONNECTED(g_Status)) { _SlNonOsMainLoopTask(); }
    }

    retVal = sl_NetCfgSet(SL_IPV4_STA_P2P_CL_DHCP_ENABLE,1,1,&val);
    ASSERT_ON_ERROR(retVal);

    configOpt = SL_SCAN_POLICY(0);
    retVal = sl_WlanPolicySet(SL_POLICY_SCAN , configOpt, NULL, 0);
    ASSERT_ON_ERROR(retVal);

    power = 0;
    retVal = sl_WlanSet(SL_WLAN_CFG_GENERAL_PARAM_ID, WLAN_GENERAL_PARAM_OPT_STA_TX_POWER, 1, (_u8 *)&power);
    ASSERT_ON_ERROR(retVal);

    retVal = sl_WlanPolicySet(SL_POLICY_PM , SL_NORMAL_POLICY, NULL, 0);
    ASSERT_ON_ERROR(retVal);

    retVal = sl_NetAppMDNSUnRegisterService(0, 0);
    ASSERT_ON_ERROR(retVal);

    pal_Memset(RxFilterIdMask.FilterIdMask, 0xFF, 8);
    retVal = sl_WlanRxFilterSet(SL_REMOVE_RX_FILTER, (_u8 *)&RxFilterIdMask,
                       sizeof(_WlanRxFilterOperationCommandBuff_t));
    ASSERT_ON_ERROR(retVal);

    retVal = sl_Stop(SL_STOP_TIMEOUT);
    ASSERT_ON_ERROR(retVal);

    retVal = initializeAppVariables();
    ASSERT_ON_ERROR(retVal);

    return retVal;
}

/**********************************************************************************************
 * Function name: establishConnectionWithAP
 * Outputs: SUCCESS
 * Description: This function connects to the required AP (SSID_NAME).
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

static _i32 establishConnectionWithAP()
{
    SlSecParams_t secParams = {0};
    _i32 retVal = 0;

    secParams.Key = PASSKEY;
    secParams.KeyLen = pal_Strlen(PASSKEY);
    secParams.Type = SEC_TYPE;

    retVal = sl_WlanConnect(SSID_NAME, pal_Strlen(SSID_NAME), 0, &secParams, 0);
    ASSERT_ON_ERROR(retVal);

    /* Wait */
    while((!IS_CONNECTED(g_Status)) || (!IS_IP_ACQUIRED(g_Status))) { _SlNonOsMainLoopTask(); }

    return SUCCESS;
}

/**********************************************************************************************
 * Function name: initializeAppVariables
 * Outputs: SUCCESS
 * Description: This function initializes the application variables
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

static _i32 initializeAppVariables()
{
    g_Status = 0;
    g_SockID = 0;
    g_DestinationIP = 0;
    g_BytesReceived = 0;
    pal_Memset(g_buff, 0, sizeof(g_buff));

    return SUCCESS;
}

/**********************************************************************************************
 * Function name: displayBanner
 * Description: This function displays the application's banner
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

static void displayBanner()
{
    CLI_Write("\n\r\n\r");
    CLI_Write(" HTTP Client - Version ");
    CLI_Write(APPLICATION_VERSION);
    CLI_Write("\n\r*******************************************************************************\n\r");
}


/**********************************************************************************************
 * Function name: ADC0InitAndTrigger
 * Description: This function configures the ADC0 module by enabling it and providing clock to
 * the module. It configures Pin PE3 as ADC analog input which is channel 0. Sample sequencer 1
 * is configured and digital value is stored to the variable ui32ADC0DigitalValue.
 * Equation to convert the analog value to digital value[8] :
 * digital value =          [Vin - Vref(-)]*[2^N - 1]
 *                      { ---------------------------- + 1/2 }int
 *                            Vref(+) - Vref(-)
 * Vref(-) = 0V
 * Vref(+) = 3.3V
 * N = 12 bits
 * Vin = input analog voltage in Volts
 * Reference for APIs:TivaWare Peripheral Driver Library User guide
 **********************************************************************************************/

void ADC0InitAndTrigger()
{
    /*Enables ADC0 and GPIO port E*/
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);

    /*Configures Pin PE3 as ADC analog input, ADC is triggered from processor trigger, channel 0 is
     * configured as ADC input, ADC sample sequencer 1 is configured and enabled, starts the
     * conversion and stores the value to ui32ADC0DigitalValue*/
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);
    ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_PROCESSOR, 0);
    ADCSequenceStepConfigure(ADC0_BASE, 1, 0, ADC_CTL_CH0);
    ADCSequenceEnable(ADC0_BASE, 1);
    ADCProcessorTrigger(ADC0_BASE, 1);
    ADCSequenceDataGet(ADC0_BASE, 1, ui32ADC0DigitalValue);
    ui32ADCValueStore = ui32ADC0DigitalValue[1];
}

/**********************************************************************************************
 * Function name: Timer0AIntHandler
 * Description: This is the interrupt handler for Timer 0. It clears the interrupt and sets
 * the value of the flag ui32FlagToCheckTimer to 1. This handler function is registered in
 * tm4c123gh6pm_startup_ccs.c file of this project.
 * Reference for APIs:TivaWare Peripheral Driver Library User guide
 **********************************************************************************************/

void Timer0AIntHandler(void)
{
    TimerIntClear( TIMER0_BASE, TIMER_TIMA_TIMEOUT );
    ui32FlagToCheckTimer = 1;
}

/**********************************************************************************************
 * Function name: TimerInitAndStart
 * Description: Timer 0 is configured as a periodic timer and the timer load value required for
 * a delay of five seconds is calculated using the below equation. The clock given to the timer
 * is 16 MHz.
 * delay = (1/16 MHz)*n
 * where n is the load value
 * delay is five seconds here. Therefore the value of n obtained is 80000000.
 * Reference for APIs:TivaWare Peripheral Driver Library User guide
 **********************************************************************************************/


void TimerInitAndStart(void)
{
    uint32_t ui32TimerPeriod;

    /*Enable Timer0 module */
    SysCtlPeripheralEnable( SYSCTL_PERIPH_TIMER0 );
    ui32FlagToCheckTimer = 0;

    /*Configure Timer 0 as a periodic Timer, Load the load value 80000000*/
    TimerConfigure( TIMER0_BASE, TIMER_CFG_A_PERIODIC );
    ui32TimerPeriod = 80000000;
    TimerLoadSet( TIMER0_BASE, TIMER_A, ui32TimerPeriod - 1 );

    /*Enable the timer interrupt and timer module*/
    IntEnable( INT_TIMER0A );
    TimerIntEnable( TIMER0_BASE, TIMER_TIMA_TIMEOUT );
    IntMasterEnable();
    TimerEnable( TIMER0_BASE, TIMER_A );
}
