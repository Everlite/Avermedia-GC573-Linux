/*===================================================================
 * Copyright(c) AVerMedia TECHNOLOGIES, Inc. 2017
 * All rights reserved
 * =================================================================
 * board_goip.c
 *
 *  Created on: Apr 15, 2017
 *      Author: 
 *      Version:
 * =================================================================
 */
 
#include <linux/kernel.h>
#include <linux/errno.h>
#include "cxt_mgr.h"
#include "gpio_model.h"
#include "board_gpio.h"
#include "pci_model.h"
#include "i2c_model.h"
#include "aver_xilinx.h"

enum
{
    GPIO_IC_AVER_XILINX,
    GPIO_IC_NUM,
};



typedef struct
{
    const char *const *pin_name;
    U8_T	pin_no;
}pin_desc_t;

typedef struct
{
	const char *ic_name;
	pin_desc_t *pin_desc;
}gpio_config_t;

static const char *const pin_name[BOARD_GPIO_NUM]=
{
    [BOARD_GPIO_RESET_CL511]="PIN_RESETN_CL511",
    //[BOARD_GPIO_EDID_WP]="PIN_EDID_WP",
    //[BOARD_GPIO_EDID_W1]="PIN_EDID_W1",
    //[BOARD_GPIO_EDID_W2]="PIN_EDID_W2",
    //[BOARD_GPIO_HPD_RX_CTL]="PIN_HPD_RX_CTL",
};



static pin_desc_t aver_xilinx_pin_desc[]=
{
    {
	.pin_name=&pin_name[BOARD_GPIO_RESET_CL511],
	.pin_no=AVER_XILINX_GPIO_0, //Rick 20170420
    },
#if 0
    {
	.pin_name=&pin_name[BOARD_GPIO_EDID_WP],
	.pin_no=AVER_XILINX_GPIO_0, //4
    },
    {
	.pin_name=&pin_name[BOARD_GPIO_EDID_W1],
	.pin_no=AVER_XILINX_GPIO_0, //5
    },
    {
	.pin_name=&pin_name[BOARD_GPIO_EDID_W2],
	.pin_no=AVER_XILINX_GPIO_0, //6
    },
    {
	.pin_name=&pin_name[BOARD_GPIO_HPD_RX_CTL],
	.pin_no=AVER_XILINX_GPIO_0, //7
    },
#endif
    {
        NULL,
    },
};

static gpio_config_t gpio_config[GPIO_IC_NUM]=
{
	{
            .ic_name=AVER_XILINX_GPIODRV_NAME,
            .pin_desc=aver_xilinx_pin_desc,
	},
};




int board_gpio_init(cxt_mgr_handle_t cxt_mgr)
{
	gpio_model_handle_t gpio_mgr;
	int i;

        gpio_mgr=cxt_manager_get_context(cxt_mgr,GPIO_CXT_ID,0);
	if(gpio_mgr)
	{
            for(i=0;i<GPIO_IC_NUM;i++)
            {
		pin_desc_t *pin_desc;

	
		pin_desc=gpio_config[i].pin_desc;
		while(pin_desc->pin_name)
		{
                    gpio_model_register_pin(gpio_mgr,gpio_config[i].ic_name,*pin_desc->pin_name,pin_desc->pin_no);
                    pin_desc++;
		}
            }
	}

	return 0;
}

void board_set_gpio(cxt_mgr_handle_t cxt_mgr,board_gpio_e no,gpio_model_pin_value_t value)
{
    gpio_model_handle_t gpio_mgr;
    
    do
    {
        gpio_model_pin_handle_t pin_handle;
        if(!cxt_mgr)
            break;
        gpio_mgr=cxt_manager_get_context(cxt_mgr,GPIO_CXT_ID,0);
        if(!gpio_mgr)
            break;
        /* FIX: use >= for upper bound and add NULL check for pin_name[no] */
        if(no < 0 || no >= BOARD_GPIO_NUM || !pin_name[no])
            break;
        pin_handle=gpio_model_request_pin_handle(gpio_mgr,pin_name[no]);
        if(!pin_handle)
            break;
        gpio_model_pin_set(pin_handle,value);
        gpio_model_pin_release(pin_handle);
          
    }while(0);
    
}

gpio_model_pin_value_t board_get_gpio(cxt_mgr_handle_t cxt_mgr,board_gpio_e no)
{
    gpio_model_handle_t gpio_mgr;
    gpio_model_pin_value_t value=GPIOIC_VALUE_UNKNOWN;
    
    do
    {
        gpio_model_pin_handle_t pin_handle;
        if(!cxt_mgr)
            break;
        gpio_mgr=cxt_manager_get_context(cxt_mgr,GPIO_CXT_ID,0);
        if(!gpio_mgr)
            break;
        /* FIX: use >= for upper bound and add NULL check for pin_name[no] */
        if(no < 0 || no >= BOARD_GPIO_NUM || !pin_name[no])
            break;
        pin_handle=gpio_model_request_pin_handle(gpio_mgr,pin_name[no]);
        if(!pin_handle)
            break;
        value=gpio_model_pin_get(pin_handle);
        gpio_model_pin_release(pin_handle);
    }while(0);
    
    return value;

}

