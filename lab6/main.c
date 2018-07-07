/****************************************************************************************
 * File name: main.c
 * Program Objective: To understand how to use the CC3100 SimpleLink Wi-Fi BoosterPack as
 * a server and communicate with the client (send data from the client to server and vice versa).
 * Objectives    : 1. CC3100 can connect to intended Wi-Fi network.
                   2. The acquired address is formatted in IP `X.Y.W.Z`.
                   3. CC3100 can successfully start TCP server.
                   4. CC3100 can receive data successfully. Based on received data both
                   LEDs are handled properly.
                   5. CC3100 can also send back data, i.e. POT value, to provided code.  ADC
                   is handled by using its ISR. The correctness of recent transmitted POT value
                   is tested by using Python code.
 * Description : CC3100 SimpleLink Wi-Fi module is stacked on top of the Tiva board and
 * potentiometer is connected to the pin PE3 of Tiva board. When the program starts executing,
 * the CC3100 module establishes connection with the wifi access point. The CC3100 module
 * establishes connection with TCP server and starts the TCP server. The client is running on the
 * same machine and is implemented in client.py python file. After acquiring the IP address of the
 * CC3100 device, the client sends LED1 and LED2 values (commands to turn on and off) to the server.
 * The server activates the GPIO pins on the Tiva board to turn on and off the LEDs. The client waits
 * for the potentiometer value to be sent from the server. The ADC converts the pot value to digital
 * value and stores in a buffer. The server sends the value of pot in the buffer to client. The client
 * displays the pot value in the windows command prompt.
 * Externally modified files: user.h, ssock.h, sl_common.h
 * TI provided code tcp_socket is used in this program and the copyright goes to,
 * Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
 * References : [1]Embedded System Design using TM4C LaunchPadTM Development Kit,SSQU015(Canvas file)
 *              [2]Tiva C Series TM4C123G LaunchPad Evaluation Board User's Guide
 *              [3]Tiva TM4C123GH6PM Microcontroller datasheet
 *              [4]TivaWare Peripheral Driver Library User guide
 *              [5]Embedded Systems An Introduction using the Renesas RX63N Microcontroller BY JAMES M. CONRAD
 *              [6]http://processors.wiki.ti.com/index.php/CC3100_Getting_Started_with_WLAN_Station
 *              [7]http://processors.wiki.ti.com/index.php/CC3100_%26_CC3200
 *              [8]https://www.youtube.com/watch?v=lRlCoXHEMKw
 *              [9]https://e2e.ti.com/support/wireless_connectivity/simplelink_wifi_cc31xx_cc32xx/f/968/p/497053/1798332
 *              [10]CC3100 SimpleLink Wi-Fi® and IoT Solution BoosterPack Hardware User's Guide
 *              [11]http://www.ti.com/lit/ds/swas031d/swas031d.pdf
 *              [12]http://www.mouser.com/ds/2/405/cc3100-469564.pdf
 *********************************************************************************************************************/

#include <stdbool.h>
#include "simplelink.h"
#include "sl_common.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include"driverlib/adc.h"
#include"inc/hw_types.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"

#define APPLICATION_VERSION "1.2.0"
#define SL_STOP_TIMEOUT        0xFF

/* Here CC3100 device is used as server.Its IP address is defined below in hex format.
 * Ip address of the CC3100 device is,
 * In decimal format - 3232236077
 * In dotted decimal format - 192.168.2.45
 * In hex format - 0xC0A8022D */
#define IP_ADDR         0xC0A8022D

#define PORT_NUM        5001
#define BUF_SIZE        1400
#define NO_OF_PACKETS   1000

typedef enum{
    DEVICE_NOT_IN_STATION_MODE = -0x7D0,
    TCP_SEND_ERROR = DEVICE_NOT_IN_STATION_MODE - 1,
    TCP_RECV_ERROR = TCP_SEND_ERROR -1,

    STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes;

union
{
    _u8 BsdBuf[BUF_SIZE];
    _u32 demobuf[BUF_SIZE/4];

    /*This stores the potentiometer value converted by the ADC*/
    _u8 PotValueBuf[1];
} uBuf;

_u8 g_Status = 0;

/*This stores the converted ADC value of the potentiometer*/
uint32_t ui32ADC0DigitalValue[1], ui32ADCValueStore;

/*This stores the received value for the LEDs' activation from the client*/
unsigned char ReceiveBuffer[20];

static _i32 configureSimpleLinkToDefaultState();
static _i32 establishConnectionWithAP();
static _i32 initializeAppVariables();
static _i32 BsdTcpServer(_u16 Port);
static void displayBanner();
void ADC0InitAndTrigger(void);
void LEDinit(void);
void ADC0IntHandler();

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

            /*The below code prints the IP address of the CC3100 module in the CCS terminal.
             * The IP address gets stored in the variable gIPv4Address. There are 4 bytes
             * in the IP address. The LSB byte is stored to IPAddressBuf[0] and the subsequent
             * bytes are stored into the IPAddressBuf character array. sprintf formats the IP
             * address into the format X.Y.W.Z. Then the formatted IP address, IPAddressFormatted
             * is printed in the terminal*/
            unsigned long gIPv4Address = pNetAppEvent->EventData.ipAcquiredV4.ip;
            char IPAddressBuf[4];
            char IPAddressFormatted[20];
            IPAddressBuf[0] = gIPv4Address & 0xFF;
            IPAddressBuf[1] = (gIPv4Address >> 8) & 0xFF;
            IPAddressBuf[2] = (gIPv4Address >> 16) & 0xFF;
            IPAddressBuf[3] = (gIPv4Address >> 24) & 0xFF;
            sprintf(IPAddressFormatted, "%d .%d.%d.%d\n", IPAddressBuf[3], IPAddressBuf[2], IPAddressBuf[1], IPAddressBuf[0]);
            CLI_Write("IP address is: ");
            CLI_Write((_u8 *) IPAddressFormatted);
            CLI_Write("\n\r");
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

            switch( pSock->socketAsyncEvent.SockTxFailData.status )
            {
                case SL_ECLOSE:
                    CLI_Write(" [SOCK EVENT] Close socket operation, failed to transmit all queued packets\n\r");
                    break;
                default:
                    CLI_Write(" [SOCK EVENT] Unexpected event \n\r");
                    break;
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
 * command line interface, displays banner, configures device in default state, configures
 * device in station mode, establishes connection with TCP server, starts the TCP server
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

int main(int argc, char** argv)
{
    LEDinit();

    _i32 retVal = -1;
    retVal = initializeAppVariables();
    ASSERT_ON_ERROR(retVal);

    stopWDT();
    initClk();
    ADC0InitAndTrigger();
    CLI_Configure();
    displayBanner();
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

    retVal = sl_Start(0, 0, 0);
    if ((retVal < 0) ||
        (ROLE_STA != retVal) )
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

    CLI_Write(" Establishing connection with TCP server \n\r");

    CLI_Write(" Starting TCP server\r\n");

    retVal = BsdTcpServer(PORT_NUM);
    if(retVal < 0)
           CLI_Write(" Failed to start TCP server \n\r");
    else
           CLI_Write(" TCP client connected successfully \n\r");

    retVal = sl_Stop(SL_STOP_TIMEOUT);
    if(retVal < 0)
        LOOP_FOREVER();

    return 0;
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

    while((!IS_CONNECTED(g_Status)) || (!IS_IP_ACQUIRED(g_Status))) { _SlNonOsMainLoopTask(); }

    return SUCCESS;
}

/**********************************************************************************************
 * Function name: BsdTcpServer
 * Outputs: SUCCESS
 * Description: This function opens a TCP socket in Listen mode and waits for an incoming
 * TCP connection. If a socket connection is established then the function will try to read
 * 1000 TCP packets from the connected client.
 * Copyright: Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
 **********************************************************************************************/

static _i32 BsdTcpServer(_u16 Port)
{
    SlSockAddrIn_t  Addr;
    SlSockAddrIn_t  LocalAddr;

    _u16          idx = 0;
    _u16          AddrSize = 0;
    _i16          SockID = 0;
    _i32          Status = 0;
    _i16          newSockID = 0;
    _u16          LoopCount = 0;
    _i16          recvSize = 0;
    _u16          irx;

    for (idx=0 ; idx<BUF_SIZE ; idx++)
    {
        uBuf.BsdBuf[idx] = (_u8)(idx % 10);
    }

    LocalAddr.sin_family = SL_AF_INET;
    LocalAddr.sin_port = sl_Htons((_u16)Port);
    LocalAddr.sin_addr.s_addr = 0;

    SockID = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, 0);
    if( SockID < 0 )
    {
        CLI_Write(" [TCP Server] Create socket Error \n\r");
        ASSERT_ON_ERROR(SockID);
    }

    AddrSize = sizeof(SlSockAddrIn_t);
    Status = sl_Bind(SockID, (SlSockAddr_t *)&LocalAddr, AddrSize);
    if( Status < 0 )
    {
        sl_Close(SockID);
        CLI_Write(" [TCP Server] Socket address assignment Error \n\r");
        ASSERT_ON_ERROR(Status);
    }

    Status = sl_Listen(SockID, 0);
    if( Status < 0 )
    {
        sl_Close(SockID);
        CLI_Write(" [TCP Server] Listen Error \n\r");
        ASSERT_ON_ERROR(Status);
    }

    newSockID = sl_Accept(SockID, ( struct SlSockAddr_t *)&Addr,
                              (SlSocklen_t*)&AddrSize);
    if( newSockID < 0 )
    {
        sl_Close(SockID);
        CLI_Write(" [TCP Server] Accept connection Error \n\r");
        ASSERT_ON_ERROR(newSockID);
    }

    while (LoopCount < NO_OF_PACKETS)
    {
        recvSize = BUF_SIZE;
        do
        {
               Status = sl_Recv(newSockID, &(uBuf.BsdBuf[BUF_SIZE - recvSize]), recvSize, 0);

               /*Stores the received values of the LED1 and LED2 as characters in the ReceiveBuffer array*/
               for(irx = 0; irx < 20; irx++)
               {
                   ReceiveBuffer[irx] = uBuf.BsdBuf[irx];
               }

               /*This checks values sent from the client for LED1 and LED2. If it is 1, the LED is turned
                * on and turned off otherwise. On board LEDs connected to PF1 and PF2 (blue and red) are
                * used in this program. The string sent from the client is in the form of LED1 :0& LED2 :1.
                * So the 5th and 12th character in the string represent LED1 and LED2 value respectively.*/

               if(ReceiveBuffer[5] == '1' )
               {
                   GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, GPIO_PIN_1);
               }
               else
               {
                   GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x0);
               }
               if(ReceiveBuffer[12] == '1' )
               {
                   GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);
               }
               else
               {
                   GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x0);
               }

               /*Stores the ADC value to a buffer which is part of the union declared so that it can be sent to the client.
                * Before sending the pot values to the client The ADC value is scaled by 16, to ensure that the value sent
                * is in the range from 0 to 255. Afterwards it sends the value to the client and closes the connection*/
               uBuf.PotValueBuf[0] = ui32ADCValueStore/16;
               Status = sl_Send(newSockID, uBuf.PotValueBuf, 1, 0 );
               sl_Close(newSockID);

               if( Status <= 0 )
               {
                   sl_Close(newSockID);
                   sl_Close(SockID);
                   CLI_Write(" [TCP Server] Data recv Error \n\r");
                   ASSERT_ON_ERROR(TCP_RECV_ERROR);
               }

               /*Made the recvSize to 0 so that the do while loop terminates.*/
               recvSize = 0;
        }
        while(recvSize > 0);
        {
            /*Made the LoopCount to 1001 so that it is greater than NO_OF_PACKETS and the while loop terminates*/
            LoopCount = 1001;
        }
    }

    Status = sl_Close(newSockID);
    ASSERT_ON_ERROR(Status);

    Status = sl_Close(SockID);
    ASSERT_ON_ERROR(Status);

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
    pal_Memset(uBuf.BsdBuf, 0, sizeof(uBuf));

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
    CLI_Write(" TCP socket application - Version ");
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
    ADCSequenceStepConfigure(ADC0_BASE, 1, 0, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
    ADCSequenceEnable(ADC0_BASE, 1);
    ADCProcessorTrigger(ADC0_BASE, 1);
    while(!ADCIntStatus(ADC0_BASE, 1, false))
    {
    }
}

/**********************************************************************************************
 * Function name: LEDinit
 * Description: This function initializes the the GPIO pins connected to the blue and red LEDs.
 * It turns off the LEDs when the program starts initially.
 * Reference for APIs:TivaWare Peripheral Driver Library User guide
 **********************************************************************************************/

void LEDinit()
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2);
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2, 0x00);
}

/**********************************************************************************************
 * Function name: ADC0IntHandler
 * Description: This is the interrupt handler for ADC0 module. This clears the interrupt and
 * gets the converted value to a global variable.
 * Reference for APIs:TivaWare Peripheral Driver Library User guide
 **********************************************************************************************/

void ADC0IntHandler()
{
    /*Clears the interrupt after conversion is done*/
    ADCIntClear(ADC0_BASE, 1);

    /*Gets the converted value to the variable ui32ADC0DigitalValue*/
    ADCSequenceDataGet(ADC0_BASE, 1, ui32ADC0DigitalValue);

    /*Stores the converted value to ui32ADCValueStore*/
    ui32ADCValueStore = ui32ADC0DigitalValue[1];
}
