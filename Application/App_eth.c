#include "App_eth.h"

#include "main.h"
#include "cmsis_os.h"
#include "stm32h7xx_hal.h"
#include <string.h>
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"

void reset_phy(void)
{
  HAL_GPIO_WritePin(ETH_RST_GPIO_Port, ETH_RST_Pin, GPIO_PIN_RESET);
  osDelay(55);
  HAL_GPIO_WritePin(ETH_RST_GPIO_Port, ETH_RST_Pin, GPIO_PIN_SET);
  osDelay(55);
}


