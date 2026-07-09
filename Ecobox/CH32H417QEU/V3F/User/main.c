#include "CH584_Comm.h"
#include "debug.h"
#include "feeder.h"
#include "fan.h"
#include "ledeat.h"
#include "ledwest.h"
#include "pump.h"
#include "status_uart.h"
#include "usart.h"

static uint8_t g_uvb_state = 0;

static uint8_t IsScreenGroup(uint8_t value)
{
    return (value == SCREEN_GROUP_ACTUATOR) ||
           (value == SCREEN_GROUP_FEEDER) ||
           (value == SCREEN_GROUP_LIGHT);
}

static uint8_t IsControlGroup(uint8_t value)
{
    return IsScreenGroup(value);
}

static void Uvb_On(void)
{
    g_uvb_state = 1;
}

static void Uvb_Off(void)
{
    g_uvb_state = 0;
}

static void PrintPacketLine(const char *prefix, uint8_t group, uint8_t cmd, uint8_t value, uint8_t has_value)
{
    if(has_value)
    {
        StatusUart_Printf("%s%02X %02X %02X 5A\r\n",
                          prefix,
                          (unsigned int)group,
                          (unsigned int)cmd,
                          (unsigned int)value);
    }
    else
    {
        StatusUart_Printf("%s%02X %02X 5A\r\n",
                          prefix,
                          (unsigned int)group,
                          (unsigned int)cmd);
    }
}

static void ApplyLightScene(uint8_t scene_cmd)
{
    switch(scene_cmd)
    {
        case SCREEN_CMD_SCENE_DAWN:
            LedEast_SetBrightness(85);
            LedWest_SetBrightness(15);
            StatusUart_Printf("A1D\r\n");
            break;

        case SCREEN_CMD_SCENE_MORNING:
            LedEast_SetBrightness(100);
            LedWest_SetBrightness(35);
            StatusUart_Printf("A2D\r\n");
            break;

        case SCREEN_CMD_SCENE_NOON:
            LedEast_SetBrightness(100);
            LedWest_SetBrightness(100);
            StatusUart_Printf("A3D\r\n");
            break;

        case SCREEN_CMD_SCENE_DUSK:
            LedEast_SetBrightness(35);
            LedWest_SetBrightness(100);
            StatusUart_Printf("A4D\r\n");
            break;

        case SCREEN_CMD_SCENE_TWILIGHT:
            LedEast_SetBrightness(10);
            LedWest_SetBrightness(65);
            StatusUart_Printf("A5D\r\n");
            break;

        case SCREEN_CMD_SCENE_NIGHT:
            LedEast_SetBrightness(0);
            LedWest_SetBrightness(0);
            StatusUart_Printf("A6D\r\n");
            break;

        default:
            StatusUart_Printf("ERRD\r\n");
            break;
    }
}

static void HandleScreenPacket(uint8_t group, uint8_t cmd, uint8_t value, uint8_t has_value)
{
    StatusUart_LogScreenPacket(group, cmd, value, has_value);
    StatusUart_Printf("AAABBB\r\n");

    switch(group)
    {
        case SCREEN_GROUP_ACTUATOR:
            switch(cmd)
            {
                case SCREEN_CMD_PUMP_ON:
                    Pump_On();
                    StatusUart_Printf("A1A\r\n");
                    break;

                case SCREEN_CMD_PUMP_OFF:
                    Pump_Off();
                    StatusUart_Printf("A0A\r\n");
                    break;

                case SCREEN_CMD_FAN_ON:
                    Fan_On();
                    StatusUart_Printf("A1B\r\n");
                    break;

                case SCREEN_CMD_FAN_OFF:
                    Fan_Off();
                    StatusUart_Printf("A0B\r\n");
                    break;

                case SCREEN_CMD_UVB_ON:
                    Uvb_On();
                    StatusUart_Printf("UVBON\r\n");
                    break;

                case SCREEN_CMD_UVB_OFF:
                    Uvb_Off();
                    StatusUart_Printf("UVBOFF\r\n");
                    break;

                default:
                    StatusUart_Printf("ERRAB\r\n");
                    break;
            }
            break;

        case SCREEN_GROUP_FEEDER:
            switch(cmd)
            {
                case SCREEN_CMD_FEEDER_OPEN:
                    Feeder_Set180Deg();
                    StatusUart_Printf("A401\r\n");
                    break;

                case SCREEN_CMD_FEEDER_RESET:
                    Feeder_Reset();
                    StatusUart_Printf("A402\r\n");
                    break;

                case SCREEN_CMD_FEEDER_OPEN_RESET:
                    Feeder_FeedOnce();
                    if(has_value)
                    {
                        StatusUart_Printf("A403%02u\r\n", (unsigned int)value);
                    }
                    else
                    {
                        StatusUart_Printf("A403\r\n");
                    }
                    break;

                default:
                    StatusUart_Printf("ERRC\r\n");
                    break;
            }
            break;

        case SCREEN_GROUP_LIGHT:
            switch(cmd)
            {
                case SCREEN_CMD_SCENE_DAWN:
                case SCREEN_CMD_SCENE_MORNING:
                case SCREEN_CMD_SCENE_NOON:
                case SCREEN_CMD_SCENE_DUSK:
                case SCREEN_CMD_SCENE_TWILIGHT:
                case SCREEN_CMD_SCENE_NIGHT:
                    ApplyLightScene(cmd);
                    break;

                case SCREEN_CMD_LEDEAT_SET:
                    if(has_value)
                    {
                        LedEast_SetBrightness(value);
                        StatusUart_Printf("AEA%03u\r\n", (unsigned int)LedEast_GetBrightness());
                    }
                    else
                    {
                        StatusUart_Printf("ERREA\r\n");
                    }
                    break;

                case SCREEN_CMD_LEDWEST_SET:
                    if(has_value)
                    {
                        LedWest_SetBrightness(value);
                        StatusUart_Printf("AWB%03u\r\n", (unsigned int)LedWest_GetBrightness());
                    }
                    else
                    {
                        StatusUart_Printf("ERRWB\r\n");
                    }
                    break;

                default:
                    StatusUart_Printf("ERRD\r\n");
                    break;
            }
            break;

        default:
            StatusUart_Printf("ERR\r\n");
            break;
    }
}

int main(void)
{
    SystemInit();
    SystemAndCoreClockUpdate();
    Delay_Init();
    StatusUart_Init(115200);
    CH584_Comm_Init(115200);

    Pump_Init();
    Fan_Init();
    Feeder_Init();
    LedEast_Init();
    LedWest_Init();

    StatusUart_Printf("screen/status uart: USART3 PB10(TX) PB11(RX)\r\n");
    StatusUart_Printf("ch584 uart: USART2 PD5(TX) PD6(RX)\r\n");
    StatusUart_Printf("waiting packet: [group cmd value? 5A]\r\n");

    while(1)
    {
        uint8_t group = 0;
        uint8_t cmd = 0;
        uint8_t value = 0;
        uint8_t has_value = 0;
        CH584_CommPacket ch584_packet = {0};

        while(StatusUart_ReadPacket(&group, &cmd, &value, &has_value))
        {
            if(!IsControlGroup(group))
            {
                PrintPacketLine("invalid screen rx: ", group, cmd, value, has_value);
                continue;
            }

            PrintPacketLine("rx: ", group, cmd, value, has_value);
            HandleScreenPacket(group, cmd, value, has_value);
        }

        while(CH584_Comm_ReadPacket(&ch584_packet))
        {
            if(!IsControlGroup(ch584_packet.group))
            {
                PrintPacketLine("invalid ch584 rx: ",
                                ch584_packet.group,
                                ch584_packet.cmd,
                                ch584_packet.value,
                                ch584_packet.has_value);
                continue;
            }

            PrintPacketLine("ch584 rx: ",
                            ch584_packet.group,
                            ch584_packet.cmd,
                            ch584_packet.value,
                            ch584_packet.has_value);
            HandleScreenPacket(ch584_packet.group,
                               ch584_packet.cmd,
                               ch584_packet.value,
                               ch584_packet.has_value);
        }

        Feeder_Service(20);
        Delay_Ms(20);
    }
}
