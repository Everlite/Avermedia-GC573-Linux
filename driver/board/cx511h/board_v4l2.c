/*===================================================================
 * Copyright(c) AVerMedia TECHNOLOGIES, Inc. 2017
 * All rights reserved
 * =================================================================
 * board_v4l2.c
 *
 *  Created on: Apr 15, 2017
 *      Author: 
 *      Version:
 * =================================================================
 */
 
//#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/videodev2.h>
#include <linux/string.h>
#include "board.h"
#include "cxt_mgr.h"
#include "framegrabber.h"
#include "mem_model.h"
#include "v4l2_model.h"
#include "board_v4l2.h"
#include "debug.h"
#include "pci_model.h"
#include "i2c_model.h"
#include "task_model.h"
#include "aver_xilinx.h"
#include "ite6805.h"
#include "board_alsa.h"
#include "board_i2c.h"

static int cnt_retry=0;

/* Module parameter: force HDMI input format override.
 * 0 = Auto-detect from ITE6805 (default)
 * 1 = Force YUV 4:2:2 BT.709 Limited
 * 2 = Force YUV 4:4:4 BT.709 Limited
 * 3 = Force RGB Full Range (0-255)
 * 4 = Force RGB Limited Range (16-235)
 * Usage: sudo insmod cx511h.ko force_input_mode=3
 * Runtime: echo 3 > /sys/module/cx511h/parameters/force_input_mode
 */
static int force_input_mode = 0;
module_param(force_input_mode, int, 0644);
MODULE_PARM_DESC(force_input_mode,
    "Force HDMI input format: 0=auto, 1=YUV422, 2=YUV444, 3=RGB-Full, 4=RGB-Limited");

/* Module parameter: debug FPGA pixel format override.
 * -1 = Use automatic mapping (default)
 * 0-11 = Force specific AVER_XILINX_FMT_* value (see aver_xilinx.h)
 * Usage: sudo insmod cx511h.ko debug_pixel_format=1
 */
static int debug_pixel_format = -1;
module_param(debug_pixel_format, int, 0644);
MODULE_PARM_DESC(debug_pixel_format,
    "Debug: Force FPGA pixel format: -1=auto, 0=YUYV, 1=UYVY, 2=YVYU, 3=VYUY, 4=RGBP, 5=RGBR, 6=RGBO, 7=RGBQ, 8=RGB3, 9=BGR3, 10=RGB4, 11=BGR4");

/*
 * Set LED color via FPGA GPIO pins.
 * r, g, b: 0=off, 1=on for each color channel.
 * Pin numbers come from module parameters in board_config.c.
 */
static void cx511h_set_led_color(handle_t aver_xilinx_handle, int r, int g, int b)
{
    int pin_r = board_get_led_pin_r();
    int pin_g = board_get_led_pin_g();
    int pin_b = board_get_led_pin_b();

    if (pin_r >= 0 && pin_r <= 8)
        aver_xilinx_set_gpio_output(aver_xilinx_handle, pin_r, r ? 1 : 0);
    if (pin_g >= 0 && pin_g <= 8)
        aver_xilinx_set_gpio_output(aver_xilinx_handle, pin_g, g ? 1 : 0);
    if (pin_b >= 0 && pin_b <= 8)
        aver_xilinx_set_gpio_output(aver_xilinx_handle, pin_b, b ? 1 : 0);
}

typedef struct
{
	BASIC_CXT_HANDLE_DECLARE;
        framegrabber_handle_t fg_handle;
        v4l2_model_handle_t v4l2_handle;
        i2c_model_handle_t i2c_model_handle;
        task_model_handle_t task_model_handle;
        handle_t aver_xilinx_handle;
        handle_t pci_handle;
        handle_t i2c_chip_handle[CL511H_I2C_CHIP_COUNT];
        ite6805_frameinfo_t cur_fe_frameinfo;
        enum ite6805_audio_sample cur_fe_audioinfo;
        int cur_bchs_value;
        int cur_bchs_selection;
        task_handle_t check_signal_task_handle;
        int board_id;
}board_v4l2_context_t;

static board_chip_desc_t cl511h_chip_desc[CL511H_I2C_CHIP_COUNT]=
{
        [CL511H_I2C_CHIP_ITE6805_0]=
        {
            .name=ITE6805_DRVNAME,
            .index=1,
        },
    
};

static framegrabber_setup_input_info_t cl511h_input_info[] =
{
    [CL511H_HDMI_INPUT] =
    {
        .name = "HDMI",
        .support_framesize_info =
        {
            [FRAMESIZE_640x480] = REFRESHRATE_60_BIT, 
            [FRAMESIZE_720x480] = REFRESHRATE_60_BIT, 
            [FRAMESIZE_720x576] = REFRESHRATE_50_BIT, 
            [FRAMESIZE_800x600] = REFRESHRATE_60_BIT, 
            [FRAMESIZE_1024x768] = REFRESHRATE_60_BIT,
            [FRAMESIZE_1280x720] = REFRESHRATE_50_BIT | REFRESHRATE_60_BIT,
            [FRAMESIZE_1280x768]  = REFRESHRATE_60_BIT,
            [FRAMESIZE_1280x800]  = REFRESHRATE_60_BIT,
            [FRAMESIZE_1280x1024] = REFRESHRATE_60_BIT,
            [FRAMESIZE_1360x768]  = REFRESHRATE_60_BIT,
            [FRAMESIZE_1440x900]  = REFRESHRATE_60_BIT,
            [FRAMESIZE_1680x1050] = REFRESHRATE_60_BIT,
            [FRAMESIZE_1920x1080] = REFRESHRATE_120_BIT | REFRESHRATE_60_BIT \
                                    | REFRESHRATE_50_BIT | REFRESHRATE_30_BIT | REFRESHRATE_25_BIT | REFRESHRATE_24_BIT,
            [FRAMESIZE_1920x1200] = REFRESHRATE_60_BIT  | REFRESHRATE_50_BIT | REFRESHRATE_30_BIT \
                                    | REFRESHRATE_25_BIT | REFRESHRATE_24_BIT,
            [FRAMESIZE_2048x1080] = REFRESHRATE_144_BIT | REFRESHRATE_120_BIT | REFRESHRATE_60_BIT \
                                    | REFRESHRATE_50_BIT | REFRESHRATE_30_BIT | REFRESHRATE_25_BIT \
                                    | REFRESHRATE_24_BIT,
            [FRAMESIZE_2560x1080] = REFRESHRATE_144_BIT | REFRESHRATE_120_BIT | REFRESHRATE_60_BIT, 
            [FRAMESIZE_2560x1440] = REFRESHRATE_144_BIT | REFRESHRATE_120_BIT | REFRESHRATE_60_BIT, 
            [FRAMESIZE_3840x2160] = REFRESHRATE_60_BIT  | REFRESHRATE_50_BIT | REFRESHRATE_30_BIT \
                                    | REFRESHRATE_25_BIT | REFRESHRATE_24_BIT, 
            [FRAMESIZE_4096x2160] = REFRESHRATE_60_BIT  | REFRESHRATE_50_BIT | REFRESHRATE_30_BIT \
                                    | REFRESHRATE_25_BIT | REFRESHRATE_24_BIT,                       
        },
    },
    [CL511H_INPUT_COUNT] =
    {
		.name = NULL,
	},
};

static framegrabber_property_t  cl511h_property={
		.name="CL511H",
		.input_setup_info=cl511h_input_info,
		.support_out_pixfmt_mask=FRAMEGRABBER_PIXFMT_BITMSK, // Re-enable all formats for debugging OBS hang
		//.max_supported_line_width=3840,
		//.max_supported_line_width=4096,
        .max_frame_size=4096 *2160,

};

static v4l2_model_device_setup_t device_info=
{
    .type = DEVICE_TYPE_GRABBER,
    .driver_name="CL511H",
    .card_name="AVerMedia CL511H",
    .capabilities= V4L2_MODEL_CAPS_CAPTURE_BIT | V4L2_MODEL_CAPS_READWRITE_BIT | V4L2_MODEL_CAPS_STREAMING_BIT,
    .buffer_type=V4L2_MODEL_BUF_TYPE_DMA_SG,
};

static void *board_v4l2_alloc(void);
static void board_v4l2_release(void *cxt);
static void cx511h_v4l2_stream_on(v4l2_model_callback_parameter_t *cb_info);
static void cx511h_v4l2_stream_off(v4l2_model_callback_parameter_t *cb_info);
static void cx511h_stream_on(framegrabber_handle_t handle);
static void cx511h_stream_off(framegrabber_handle_t handle);
//static void cx511h_bchs_read(framegrabber_handle_t handle);
static void cx511h_bchs_write(framegrabber_handle_t handle);
static void cx511h_brightness_read(framegrabber_handle_t handle,int *brightness);
static void cx511h_contrast_read(framegrabber_handle_t handle,int *contrast);
static void cx511h_hue_read(framegrabber_handle_t handle,int *hue);
static void cx511h_saturation_read(framegrabber_handle_t handle,int *saturation);
static void cx511h_hdcp_state_read(framegrabber_handle_t handle,int *hdcp_state);
static void cx511h_hdcp_state_set(framegrabber_handle_t handle,int hdcp_state);
static int cx511h_i2c_read(framegrabber_handle_t handle, unsigned char channel, unsigned int slave, unsigned int sub, unsigned char sublen, unsigned char *data, unsigned int datalen, unsigned int is_10bit);
static int cx511h_i2c_write(framegrabber_handle_t handle, unsigned char channel, unsigned int slave, unsigned int sub, unsigned char sublen, unsigned char *data, unsigned int datalen, unsigned int is_10bit);
static int cx511h_reg_read(framegrabber_handle_t handle, unsigned int offset, unsigned char n_bytes, unsigned int *data);
static int cx511h_reg_write(framegrabber_handle_t handle, unsigned int offset, unsigned char n_bytes, unsigned int data);

static int cx511h_flash_dump(framegrabber_handle_t handle,int start_block, int blocks, U8_T *flash_dump); //
static int cx511h_flash_update(framegrabber_handle_t handle,int start_block, int blocks, U8_T *flash_dump); //

static framegrabber_interface_t cx511h_ops={
    .stream_on      = cx511h_stream_on,
    .stream_off     = cx511h_stream_off,
    //.bchs_get     = cx511h_bchs_read,
    .brightness_get = cx511h_brightness_read,
    .contrast_get   = cx511h_contrast_read,
    .hue_get        = cx511h_hue_read,
    .saturation_get = cx511h_saturation_read,
    .bchs_set       = cx511h_bchs_write,
    .flash_read     = cx511h_flash_dump,
    .flash_update   = cx511h_flash_update,
    .hdcp_state_get = cx511h_hdcp_state_read,
    .hdcp_state_set  = cx511h_hdcp_state_set,
    .i2c_read = cx511h_i2c_read,
    .i2c_write = cx511h_i2c_write,
    .reg_read = cx511h_reg_read,
    .reg_write = cx511h_reg_write,
};

static void *board_v4l2_alloc()
{
	board_v4l2_context_t *cxt;
	cxt=mem_model_alloc_buffer(sizeof(*cxt));
	if (cxt)
		memset(cxt, 0, sizeof(*cxt));

	return cxt;
}

static void board_v4l2_release(void *cxt)
{
	board_v4l2_context_t *board_v4l2_cxt=cxt;

	if(board_v4l2_cxt)
	{
            if(board_v4l2_cxt->check_signal_task_handle)
            {
                task_model_release_task(board_v4l2_cxt->task_model_handle,board_v4l2_cxt->check_signal_task_handle);
                board_v4l2_cxt->check_signal_task_handle=NULL;
            }
            cxt_manager_unref_context(board_v4l2_cxt->task_model_handle);
            cxt_manager_unref_context(board_v4l2_cxt->aver_xilinx_handle);
            mem_model_free_buffer(board_v4l2_cxt);
	}
}

static void cx511h_v4l2_stream_on(v4l2_model_callback_parameter_t *cb_info)
{
    board_v4l2_context_t *board_v4l2_cxt=cb_info->asso_data;
    
    printk(KERN_ERR "[cx511h-dma] >>> VIDIOC_STREAMON callback fired! cxt=%p\n",
           board_v4l2_cxt);

    /* Trigger the actual hardware configuration logic (includes CSC force and FPGA setup) */
    if (board_v4l2_cxt && board_v4l2_cxt->fg_handle) {
        cx511h_stream_on(board_v4l2_cxt->fg_handle);
    }
}

static void cx511h_v4l2_stream_off(v4l2_model_callback_parameter_t *cb_info)
{
    board_v4l2_context_t *board_v4l2_cxt=cb_info->asso_data;
    debug_msg("%s %p\n",__func__,board_v4l2_cxt);
    
}
static void cx511h_stream_on(framegrabber_handle_t handle)
{
    board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
    aver_xilinx_video_process_cfg_t vip_cfg;
    int width,height;
    int out_width,out_height;
    int frameinterval=0;
    int audio_rate=0;
    int dual_pixel=0;
    //int hdcp_state = 0;
    framegrabber_framemode_e framemode;
    const framegrabber_pixfmt_t *pixfmt=framegrabber_g_out_pixelfmt(handle);
    
    handle_t ite6805_handle=board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0]; 

    if (!ite6805_handle) {
        printk(KERN_ERR "[cx511h-ttl] ERROR: ite6805_handle is NULL in stream_on!\n");
        return;
    }

    printk(KERN_ERR "[cx511h-ttl] === STREAM ON START ===\n");

    /* TRUE TTL PIXEL MODE (Single/SDR Sync) 
     * Aligning HDMI receiver TTL bus with the forced FPGA/V4L2 YUYV path. 
     * MUST BE DONE AT THE VERY START! */
    printk(KERN_ERR "[cx511h-ttl] Configuring TTL Pixel Mode (Single/SDR)...\n");

    // 1. Reset TTL Interface
    hdmirxwr(ite6805_handle, 0xc0, 0x00);
    msleep(10);

    // 2. Base Mode (Single Pixel / SDR - Bits [2:1] = 10)
    hdmirxwr(ite6805_handle, 0xc0, 0x02);
    msleep(10);

    // 3. Lane/DDR Config (Standard SDR)
    hdmirxwr(ite6805_handle, 0xc1, 0x00);
    msleep(10);
    printk(KERN_ERR "[cx511h-ttl] Reverted to: 0xc0=0x02, 0xc1=0x00\n");

    // 4. Pixel Mode Enumeration (Auto detection flags)
    hdmirxwr(ite6805_handle, 0xbd, 0x00);
    hdmirxwr(ite6805_handle, 0xbe, 0x00);
    msleep(10);

    // 5. DDR Flag (Disabled for SDR)
    hdmirxwr(ite6805_handle, 0xc4, 0x00);
    msleep(10);
    
    printk(KERN_ERR "[cx511h-ttl] TTL Pixel Mode (Single/SDR) sequence complete.\n");

    //ite6805_get_hdcp_state(ite6805_handle, &hdcp_state);
    //ite6805_set_hdcp_state(ite6805_handle, hdcp_state);

    enum ite6805_audio_sample fe_audioinfo=0;
    ite6805_frameinfo_t *fe_frameinfo=&board_v4l2_cxt->cur_fe_frameinfo; 
    
    //aver_xilinx_frame_info_t detected_frameinfo;
    mesg("%s\n",__func__);
    //aver_xilinx_get_frameinfo(board_v4l2_cxt->aver_xilinx_handle,&detected_frameinfo);
    framemode=framegrabber_g_input_interlace(board_v4l2_cxt->fg_handle);
    framegrabber_g_input_framesize(board_v4l2_cxt->fg_handle,&width,&height);

    //frameinterval=framegrabber_g_input_framerate(board_v4l2_cxt->fg_handle);
    frameinterval=framegrabber_g_out_framerate(board_v4l2_cxt->fg_handle);
    if (frameinterval ==0)
    {
		frameinterval=framegrabber_g_input_framerate(board_v4l2_cxt->fg_handle);
	}
    //framegrabber_s_out_framerate(board_v4l2_cxt->fg_handle,0);
    
    framegrabber_g_out_framesize(board_v4l2_cxt->fg_handle,&out_width,&out_height);
    debug_msg("%s in %dx%d out %dx%d\n",__func__,width,height,out_width,out_height);
    
    if ((out_width ==0) || (out_height ==0))
    {
        debug_msg("debug...\n");
        out_width = 1920;
        out_height = 1080;
        framegrabber_s_out_framesize(board_v4l2_cxt->fg_handle,1920,1080);
    }

    if(framemode==FRAMEGRABBER_FRAMEMODE_INTERLACED)
        vip_cfg.in_videoformat.is_interlace=TRUE;
    else
        vip_cfg.in_videoformat.is_interlace=FALSE;
        
    //audio_rate = framegrabber_g_input_audioinfo(board_v4l2_cxt->fg_handle);
    
    dual_pixel = framegrabber_g_input_dualmode(board_v4l2_cxt->fg_handle);
    
    //aver_xilinx_dual_pixel(board_v4l2_cxt->aver_xilinx_handle,dual_pixel);
    if (dual_pixel ==1)
        vip_cfg.dual_pixel = 1;
    else
        vip_cfg.dual_pixel = 0;   
    
    audio_rate = aver_xilinx_get_audioinfo(board_v4l2_cxt->aver_xilinx_handle);
    vip_cfg.audio_rate = audio_rate;
    
    framegrabber_s_input_audioinfo(board_v4l2_cxt->fg_handle,audio_rate);

    mesg("%s...vip_cfg.audio_rate_index=%d\n",__func__,vip_cfg.audio_rate);
    ite6805_get_frameinfo(ite6805_handle,fe_frameinfo);
    
    debug_msg("%s========fe_frameinfo->packet_colorspace=%d\n",__func__,fe_frameinfo->packet_colorspace);
    
    vip_cfg.in_videoformat.fps = framegrabber_g_input_framerate(board_v4l2_cxt->fg_handle);
    //if(vip_cfg.is_interlace==TRUE)
        //vip_cfg.in_framerate = vip_cfg.in_framerate * 2;
    vip_cfg.packet_colorspace = fe_frameinfo->packet_colorspace;    
    vip_cfg.in_videoformat.vactive = width;
    vip_cfg.in_videoformat.hactive = height;
    //if (vip_cfg.in_videoformat.is_interlace==TRUE)
        //vip_cfg.in_videoformat.hactive *= 2; 
    vip_cfg.out_videoformat.width=out_width;
    vip_cfg.out_videoformat.height=out_height;
    vip_cfg.clip_start.x=0;
    vip_cfg.clip_start.y=0;
    vip_cfg.clip_size.width=width;
    vip_cfg.clip_size.height=height;
//  vip_cfg.valid_mask=CLIP_CFG_VALID_MASK|SCALER_CFG_VALID_MASK;
//  vip_cfg.valid_mask=SCALER_CFG_VALID_MASK|SCALER_CFG_SHRINK_MASK;
    vip_cfg.valid_mask=SCALER_CFG_VALID_MASK|FRAMERATE_CFG_VALID_MASK;
    vip_cfg.pixel_clock=board_v4l2_cxt->cur_fe_frameinfo.pixel_clock;
    vip_cfg.in_ddrmode = fe_frameinfo->ddr_mode;
    //vip_cfg.out_framerate= 0; //for test
    vip_cfg.out_videoformat.fps=frameinterval;
    mesg("%s..in_framerate=%d  out_framerate=%d pixel_clock=%d\n",__func__,vip_cfg.in_videoformat.fps,vip_cfg.out_videoformat.fps,vip_cfg.pixel_clock);

    ite6805_get_workingmode(ite6805_handle, &(vip_cfg.in_workingmode)) ;
    vip_cfg.in_colorspacemode = 0; // 0-yuv, 1-rgb limit, 2-rgb full
    ite6805_get_colorspace(ite6805_handle, &(vip_cfg.in_colorspacemode));
    vip_cfg.in_videoformat.colorspace = VIDEO_RGB_MODE;
    vip_cfg.in_packetsamplingmode = 1; // 0-rgb, 1-422, 2-444	
    vip_cfg.in_packetcsc_bt = COLORMETRY_ITU709;
    ite6805_get_sampingmode(ite6805_handle, &(vip_cfg.in_packetsamplingmode));
    vip_cfg.currentCSC = CAPTURE_BT709_COMPUTER;

    /* === DEBUG: Log all colorspace/CSC fields BEFORE any override === */
    printk(KERN_ERR "[cx511h-color] --- Input Format Detection ---\n");
    printk(KERN_ERR "[cx511h-color]   in_colorspacemode=%u (0=yuv,1=rgbL,2=rgbF)\n",
           vip_cfg.in_colorspacemode);
    printk(KERN_ERR "[cx511h-color]   in_packetsamplingmode=%u (0=rgb,1=422,2=444)\n",
           vip_cfg.in_packetsamplingmode);
    printk(KERN_ERR "[cx511h-color]   in_videoformat.colorspace=%u\n",
           vip_cfg.in_videoformat.colorspace);
    printk(KERN_ERR "[cx511h-color]   packet_colorspace=%d (0=YUV,1=RGBF,2=RGBL)\n",
           vip_cfg.packet_colorspace);
    printk(KERN_ERR "[cx511h-color]   in_packetcsc_bt=%u (1=601,2=709,3=2020)\n",
           vip_cfg.in_packetcsc_bt);
    printk(KERN_ERR "[cx511h-color]   in_workingmode=%u  in_ddrmode=%u\n",
           vip_cfg.in_workingmode, vip_cfg.in_ddrmode);
    printk(KERN_ERR "[cx511h-color]   currentCSC=%d  fe_pkt_cs=%d\n",
           vip_cfg.currentCSC, fe_frameinfo->packet_colorspace);

    /* === INPUT FORMAT OVERRIDE via module parameter ===
     * force_input_mode: 0=auto, 1=YUV422, 2=YUV444, 3=RGB-Full, 4=RGB-Limited
     * Can be changed at runtime: echo N > /sys/module/cx511h/parameters/force_input_mode */
    switch (force_input_mode) {
    case 1: /* YUV 4:2:2 BT.709 Limited */
        vip_cfg.in_colorspacemode = 0;
        vip_cfg.in_packetsamplingmode = 1;  /* 422 */
        vip_cfg.in_packetcsc_bt = COLORMETRY_ITU709;
        vip_cfg.currentCSC = CAPTURE_BT709_STUDIO;
        vip_cfg.in_videoformat.colorspace = VIDEO_422_MODE;
        vip_cfg.packet_colorspace = CS_YUV;
        printk(KERN_ERR "[cx511h-color] MANUAL OVERRIDE: Mode 1 — YUV 4:2:2 BT709 Limited\n");
        break;
    case 2: /* YUV 4:4:4 BT.709 Limited */
        vip_cfg.in_colorspacemode = 0;
        vip_cfg.in_packetsamplingmode = 2;  /* 444 */
        vip_cfg.in_packetcsc_bt = COLORMETRY_ITU709;
        vip_cfg.currentCSC = CAPTURE_BT709_STUDIO;
        vip_cfg.in_videoformat.colorspace = VIDEO_422_MODE;
        vip_cfg.packet_colorspace = CS_YUV;
        printk(KERN_ERR "[cx511h-color] MANUAL OVERRIDE: Mode 2 — YUV 4:4:4 BT709 Limited\n");
        break;
    case 3: /* RGB Full Range (0-255) */
        vip_cfg.in_colorspacemode = 2;
        vip_cfg.in_packetsamplingmode = 0;  /* RGB */
        vip_cfg.in_packetcsc_bt = COLORMETRY_ITU709;
        vip_cfg.currentCSC = CAPTURE_BT709_COMPUTER;
        vip_cfg.in_videoformat.colorspace = VIDEO_RGB_MODE;
        vip_cfg.packet_colorspace = CS_RGB_FULL;
        printk(KERN_ERR "[cx511h-color] MANUAL OVERRIDE: Mode 3 — RGB Full BT709\n");
        break;
    case 4: /* RGB Limited Range (16-235) */
        vip_cfg.in_colorspacemode = 1;
        vip_cfg.in_packetsamplingmode = 0;  /* RGB */
        vip_cfg.in_packetcsc_bt = COLORMETRY_ITU709;
        vip_cfg.currentCSC = CAPTURE_BT709_STUDIO;
        vip_cfg.in_videoformat.colorspace = VIDEO_RGB_MODE;
        vip_cfg.packet_colorspace = CS_RGB_LIMIT;
        printk(KERN_ERR "[cx511h-color] MANUAL OVERRIDE: Mode 4 — RGB Limited BT709\n");
        break;
    default: /* 0 = Auto-detect from ITE6805 */
        if (fe_frameinfo->packet_colorspace == CS_YUV) {
            vip_cfg.in_colorspacemode = 0;
            vip_cfg.in_packetsamplingmode = 1;
            vip_cfg.in_packetcsc_bt = COLORMETRY_ITU709;
            vip_cfg.currentCSC = CAPTURE_BT709_STUDIO;
            vip_cfg.in_videoformat.colorspace = VIDEO_422_MODE;
            printk(KERN_ERR "[cx511h-color] AUTO: YUV 4:2:2 BT709 Limited\n");
        } else if (fe_frameinfo->packet_colorspace == CS_RGB_FULL) {
            vip_cfg.in_colorspacemode = 2;
            vip_cfg.in_packetsamplingmode = 0;
            vip_cfg.in_packetcsc_bt = COLORMETRY_ITU709;
            vip_cfg.currentCSC = CAPTURE_BT709_COMPUTER;
            vip_cfg.in_videoformat.colorspace = VIDEO_RGB_MODE;
            printk(KERN_ERR "[cx511h-color] AUTO: RGB Full BT709\n");
        } else if (fe_frameinfo->packet_colorspace == CS_RGB_LIMIT) {
            vip_cfg.in_colorspacemode = 1;
            vip_cfg.in_packetsamplingmode = 0;
            vip_cfg.in_packetcsc_bt = COLORMETRY_ITU709;
            vip_cfg.currentCSC = CAPTURE_BT709_STUDIO;
            vip_cfg.in_videoformat.colorspace = VIDEO_RGB_MODE;
            printk(KERN_ERR "[cx511h-color] AUTO: RGB Limited BT709\n");
        } else {
            printk(KERN_ERR "[cx511h-color] AUTO: UNKNOWN cs=%d — using blob defaults\n",
                   fe_frameinfo->packet_colorspace);
        }
        break;
    }

	
    //enable video bypass
    if (((vip_cfg.in_videoformat.vactive == 4096) && (vip_cfg.out_videoformat.width == 4096)
       && (vip_cfg.out_videoformat.height == 2160) && (vip_cfg.out_videoformat.height == 2160)) || //4096x2160
       ((vip_cfg.in_videoformat.vactive == 3840) && (vip_cfg.out_videoformat.width == 3840)
       && (vip_cfg.out_videoformat.height == 2160) && (vip_cfg.out_videoformat.height == 2160)) || //3840x2160
       ((vip_cfg.in_videoformat.vactive == 2560) && (vip_cfg.out_videoformat.width == 2560)  
       && (vip_cfg.in_videoformat.hactive == 1600) && (vip_cfg.out_videoformat.height == 1600)) || //2560x1600
       ((vip_cfg.in_videoformat.vactive == 2560) && (vip_cfg.out_videoformat.width == 2560) 
       && (vip_cfg.in_videoformat.hactive == 1440) && (vip_cfg.out_videoformat.height == 1440)) || //2560x1440
       ((vip_cfg.in_videoformat.vactive == 2560) && (vip_cfg.out_videoformat.width == 2560) 
       && (vip_cfg.in_videoformat.hactive == 1080) && (vip_cfg.out_videoformat.height == 1080)) ||  //2560x1080
       ((vip_cfg.in_videoformat.vactive == 2048) && (vip_cfg.out_videoformat.width == 2048) 
       && (vip_cfg.in_videoformat.hactive == 1080) && (vip_cfg.out_videoformat.height == 1080)) ||  //2048x1080
       ((vip_cfg.in_videoformat.vactive == 1920) && (vip_cfg.out_videoformat.width == 1920) 
       && (vip_cfg.in_videoformat.hactive == 1440) && (vip_cfg.out_videoformat.height == 1440)) || //1920x1440
       ((vip_cfg.in_videoformat.vactive == 1856) && (vip_cfg.out_videoformat.width == 1856)
       && (vip_cfg.in_videoformat.hactive == 1392) && (vip_cfg.out_videoformat.height == 1392)) || //1856x1392
       ((vip_cfg.in_videoformat.vactive == 1792) && (vip_cfg.out_videoformat.width == 1792)
       && (vip_cfg.in_videoformat.hactive == 1344) && (vip_cfg.out_videoformat.height == 1344)) || //1792x1344
       ((vip_cfg.in_videoformat.vactive == 2048) && (vip_cfg.out_videoformat.width == 2048)
       && (vip_cfg.in_videoformat.hactive == 1152) && (vip_cfg.out_videoformat.height == 1152)) || //2048x1152
       ((vip_cfg.in_videoformat.vactive == 1920) && (vip_cfg.out_videoformat.width == 1920)
       && (vip_cfg.in_videoformat.hactive == 1200) && (vip_cfg.out_videoformat.height == 1200)) || //1920x1200
       ((vip_cfg.in_videoformat.vactive == 1920) && (vip_cfg.out_videoformat.width == 1920)
       && (vip_cfg.in_videoformat.hactive == 1080) && (vip_cfg.out_videoformat.height == 1080)
       && (vip_cfg.in_videoformat.fps == 120)))
    {
		mesg("%s bypass = 1.. in %dx%d out %dx%d\n",__func__,vip_cfg.in_videoformat.vactive, vip_cfg.in_videoformat.hactive,vip_cfg.out_videoformat.width,vip_cfg.out_videoformat.height);
        //aver_xilinx_video_bypass(board_v4l2_cxt->aver_xilinx_handle,1); //enable video bypass
        vip_cfg.video_bypass = 1;
	}
	else
	{
		mesg("%s bypass = 0.. in %dx%d out %dx%d\n",__func__,vip_cfg.in_videoformat.vactive,vip_cfg.in_videoformat.hactive,vip_cfg.out_videoformat.width,vip_cfg.out_videoformat.height);
		//aver_xilinx_video_bypass(board_v4l2_cxt->aver_xilinx_handle,0); //disable video bypass
		vip_cfg.video_bypass = 0;
	}
	
    // --- PHASE2-FINAL --- Map V4L2 pixel format to FPGA pixel format (fix green screen)
    // Hardware outputs UYVY (BT.656 native), all YUV422 formats must map to FPGA UYVY
    
    // DEBUG OVERRIDE: If debug_pixel_format >= 0, force specific FPGA format
    if (debug_pixel_format >= 0) {
        vip_cfg.pixel_format = debug_pixel_format;
        printk(KERN_ERR "[cx511h-v4l2] DEBUG OVERRIDE: FPGA pixel format forced to %d (0x%02x)\n",
               debug_pixel_format, debug_pixel_format);
    } else if (pixfmt->is_yuv) {
        // All YUV422 formats (YUYV, UYVY, YVYU, VYUY) map to FPGA UYVY (BT.656 native)
        vip_cfg.pixel_format = AVER_XILINX_FMT_UYVY;
        printk(KERN_ERR "[cx511h-v4l2] GREEN SCREEN FIX: YUV422 %s → FPGA UYVY (BT.656 native)\n",
               pixfmt->name);
    } else {
        // Default mapping for non-YUV formats (RGB24, etc.)
        vip_cfg.pixel_format = pixfmt->pixfmt_out;
        printk(KERN_ERR "[cx511h-v4l2] FPGA pixel format: %s → AVER_XILINX_FMT_%d (FourCC: 0x%08x)\n",
               pixfmt->name, pixfmt->pixfmt_out, pixfmt->fourcc);
    }

    /* DEBUG 4: Video Unmute Sequence (from Windows Driver Analysis) */
    {
        printk(KERN_ERR "[cx511h-hdmi] Starting Video UNMUTE sequence...\n");
        hdmirxwr(ite6805_handle, 0x23, 0xb0);  // Unmute / Power Up stage
        msleep(10);
        hdmirxwr(ite6805_handle, 0x23, 0xa0);  // Prepare / Settle stage
        msleep(10);
        hdmirxwr(ite6805_handle, 0x23, 0x02);  // Enable Video stage
        
        printk(KERN_ERR "[cx511h-hdmi] Video UNMUTE sequence complete.\n");
    }

    // --- PHASE2 --- I2C Enable Sequence for Continuous Streaming
    {
        printk(KERN_ERR "[cx511h-phase2] Enabling ITE68051 streaming registers...\n");
        // Video Output Enable (Bit 6)
        hdmirxwr(ite6805_handle, 0x20, 0x40);
        msleep(5);
        // Global Enable
        hdmirxwr(ite6805_handle, 0x86, 0x01);
        msleep(5);
        // IRQ Enable
        hdmirxwr(ite6805_handle, 0x90, 0x8f);
        msleep(5);
        // DMA Channel Enable (Channels 0-2)
        hdmirxwr(ite6805_handle, 0xA0, 0x80);
        hdmirxwr(ite6805_handle, 0xA1, 0x80);
        hdmirxwr(ite6805_handle, 0xA2, 0x80);
        msleep(5);
        // DMA Enable
        hdmirxwr(ite6805_handle, 0xA4, 0x08);
        msleep(5);
        // Buffer Enable
        hdmirxwr(ite6805_handle, 0xB0, 0x01);
        msleep(5);
        printk(KERN_ERR "[cx511h-phase2] ITE68051 streaming registers enabled.\n");
    }

    // --- PHASE2-FIX --- RX DeSkew Error Mitigation (reduce "RX DeSkew Error Interrupt")
    {
        printk(KERN_ERR "[cx511h-phase2] Applying RX DeSkew Error Mitigation...\n");
        hdmirxwr(ite6805_handle, 0x4A, 0x0F);  // Increase tolerance
        hdmirxwr(ite6805_handle, 0x4B, 0x0F);  // Increase tolerance
        msleep(5);
        printk(KERN_ERR "[cx511h-phase2] RX DeSkew registers set to 0x0F\n");
    }

    printk(KERN_ERR "[cx511h-dma] stream_on: BEFORE aver_xilinx_config_video_process\n");
    printk(KERN_ERR "[cx511h-dma]   in=%dx%d out=%dx%d fps_in=%d fps_out=%d\n",
           vip_cfg.in_videoformat.vactive, vip_cfg.in_videoformat.hactive,
           vip_cfg.out_videoformat.width, vip_cfg.out_videoformat.height,
           vip_cfg.in_videoformat.fps, vip_cfg.out_videoformat.fps);
    printk(KERN_ERR "[cx511h-dma]   pclk=%u bypass=%d dual=%d ddr=%d pixfmt=%d\n",
           vip_cfg.pixel_clock, vip_cfg.video_bypass,
           vip_cfg.dual_pixel, vip_cfg.in_ddrmode,
           vip_cfg.pixel_format);
    printk(KERN_ERR "[cx511h-dma]   xilinx_handle=%p\n",
           board_v4l2_cxt->aver_xilinx_handle);

    /* SAFETY 1: Make sure streaming is off before reconfiguring.
     * The blob may not handle a re-config while DMA is running. */
    printk(KERN_ERR "[cx511h-dma] stream_on: disabling streaming before config\n");
    aver_xilinx_enable_video_streaming(board_v4l2_cxt->aver_xilinx_handle, FALSE);

    /* SAFETY 2: Small delay to let the FPGA/DMA engine fully stop */
    msleep(50);

    aver_xilinx_config_video_process(board_v4l2_cxt->aver_xilinx_handle,&vip_cfg);

    // 6. Final CSC Sync (BT.709 Pass-through)
    printk(KERN_ERR "[cx511h-color] Syncing ITE6805 CSC registers...\n");
    hdmirxwr(ite6805_handle, 0x6b, 0x02);  // Input: YUV422
    hdmirxwr(ite6805_handle, 0x6c, 0x02);  // CSC: BT.709
    hdmirxwr(ite6805_handle, 0x6e, 0x01);  // Output: YUV422
    hdmirxwr(ite6805_handle, 0x2a, 0x3a);  // AVI InfoFrame: YUV (BT.709)

    // --- CSC-FIX --- FPGA CSC Configuration (MMIO 0x1040) - Dynamic based on input format
    // Based on Windows driver analysis:
    // - bit0 = 1 (VIDEO_422_MODE) - always set for YUV422 output
    // - bit1 = 1 if RGB->YUV conversion needed (input is RGB)
    // - bit3 = 0 (YUV->RGB disable) - always 0 for YUV output
    // - bits[10:8] = CSC matrix: 
    //   0 = RGB Full -> YUV BT.709 (RGBF_to_BT709)
    //   1 = RGB Limited -> YUV BT.709 (RGBL_to_BT709)
    //   4 = YUV BT.601 -> YUV BT.709 (BT601_to_BT709)
    //   5 = YUV BT.709 -> YUV BT.601 (BT709_to_BT601)
    // - bit5 = 0 (single-pixel mode) - always 0
    {
        u32 csc_value = 0;
        u8 csc_matrix = 0;
        
        // Always set VIDEO_422_MODE (bit0)
        csc_value |= 0x1;
        
        // Determine CSC matrix based on input format
        if (vip_cfg.in_colorspacemode == 0) {
            // Input is YUV
            csc_value &= ~(0x1 << 1); // Clear RGB->YUV bit
            if (vip_cfg.in_packetcsc_bt == COLORMETRY_ITU601) {
                // YUV BT.601 -> YUV BT.709
                csc_matrix = 4; // BT601_to_BT709
            } else {
                // YUV BT.709 -> YUV BT.709 (passthrough)
                csc_matrix = 0; // No conversion needed
            }
        } else if (vip_cfg.in_colorspacemode == 1) {
            // Input is RGB Limited
            csc_value |= (0x1 << 1); // Set RGB->YUV bit
            if (vip_cfg.in_packetcsc_bt == COLORMETRY_ITU601) {
                csc_matrix = 3; // RGBL_to_BT601
            } else {
                csc_matrix = 1; // RGBL_to_BT709
            }
        } else if (vip_cfg.in_colorspacemode == 2) {
            // Input is RGB Full
            csc_value |= (0x1 << 1); // Set RGB->YUV bit
            if (vip_cfg.in_packetcsc_bt == COLORMETRY_ITU601) {
                csc_matrix = 2; // RGBF_to_BT601
            } else {
                csc_matrix = 0; // RGBF_to_BT709
            }
        }
        
        // Set CSC matrix bits [10:8]
        csc_value |= (csc_matrix << 8);
        
        cxt_mgr_handle_t cxt_mgr = get_cxt_manager_from_context(board_v4l2_cxt);
        handle_t pci_handle = NULL;
        
        printk(KERN_ERR "[cx511h-csc] Debug: board_v4l2_cxt=%p\n", board_v4l2_cxt);
        if (cxt_mgr) {
            printk(KERN_ERR "[cx511h-csc] Debug: cxt_mgr=%p\n", cxt_mgr);
            pci_handle = pci_model_get_handle(cxt_mgr);
            if (pci_handle) {
                printk(KERN_ERR "[cx511h-csc] Debug: pci_handle from pci_model_get_handle=%p\n", pci_handle);
            } else {
                printk(KERN_ERR "[cx511h-csc] Debug: pci_model_get_handle returned NULL\n");
            }
        } else {
            printk(KERN_ERR "[cx511h-csc] Debug: cxt_mgr is NULL\n");
        }
        
        // Fallback: use stored pci_handle from board context
        if (!pci_handle && board_v4l2_cxt->pci_handle) {
            pci_handle = board_v4l2_cxt->pci_handle;
            printk(KERN_ERR "[cx511h-csc] Debug: using stored pci_handle=%p\n", pci_handle);
        }
        
        // If still NULL, try to get PCI handle via cxt_manager_get_context
        if (!pci_handle && cxt_mgr) {
            pci_handle = cxt_manager_get_context(cxt_mgr, PCI_CXT_ID, 0);
            if (pci_handle) {
                printk(KERN_ERR "[cx511h-csc] Debug: pci_handle from cxt_manager_get_context=%p\n", pci_handle);
            }
        }
        
        // Write CSC register if we have a valid handle
        if (pci_handle) {
            pci_model_mmio_write(pci_handle, 0, 0x1040, csc_value);
            printk(KERN_ERR "[cx511h-csc] FPGA CSC configured: 0x1040 = 0x%03x ", csc_value);
            printk(KERN_ERR "(in_cs=%u, csc_bt=%u, matrix=%u, %s->YUV)\n",
                   vip_cfg.in_colorspacemode, vip_cfg.in_packetcsc_bt, csc_matrix,
                   (vip_cfg.in_colorspacemode == 0) ? "YUV" : 
                   (vip_cfg.in_colorspacemode == 1) ? "RGB_L" : "RGB_F");
        } else {
            printk(KERN_ERR "[cx511h-csc] ERROR: No valid PCI handle, CSC register not written!\n");
        }
    }


    /* SAFETY 3: Settle delay after config — give the FPGA time to latch
     * the new scaler/CSC/DMA settings before we start the stream.
     * Too short = FPGA uses stale config = DMA writes to wrong addresses. */
    printk(KERN_ERR "[cx511h-dma] stream_on: config done, settling 200ms...\n");
    msleep(200);

    /* Flush all printk messages to the log buffer so they survive a crash */
    printk(KERN_ERR "[cx511h-dma] stream_on: >>> ENABLING STREAMING NOW <<<\n");

    aver_xilinx_enable_video_streaming(board_v4l2_cxt->aver_xilinx_handle, TRUE);
    printk(KERN_ERR "[cx511h-dma] stream_on: AFTER enable_video_streaming — survived!\n");
    
    // --- PHASE2 --- Initial doorbell after starting streaming
    {
        cxt_mgr_handle_t cxt_mgr = get_cxt_manager_from_context(board_v4l2_cxt);
        handle_t pci_handle = NULL;
        
        printk(KERN_ERR "[cx511h-phase2] Debug: board_v4l2_cxt=%p\n", board_v4l2_cxt);
        if (cxt_mgr) {
            printk(KERN_ERR "[cx511h-phase2] Debug: cxt_mgr=%p\n", cxt_mgr);
            pci_handle = pci_model_get_handle(cxt_mgr);
            if (pci_handle) {
                printk(KERN_ERR "[cx511h-phase2] Debug: pci_handle from pci_model_get_handle=%p\n", pci_handle);
            } else {
                printk(KERN_ERR "[cx511h-phase2] Debug: pci_model_get_handle returned NULL\n");
            }
        } else {
            printk(KERN_ERR "[cx511h-phase2] Debug: cxt_mgr is NULL\n");
        }
        
        // Fallback: use stored pci_handle from board context
        if (!pci_handle && board_v4l2_cxt->pci_handle) {
            pci_handle = board_v4l2_cxt->pci_handle;
            printk(KERN_ERR "[cx511h-phase2] Debug: using stored pci_handle=%p\n", pci_handle);
        }
        
        // If still NULL, try to get PCI handle via cxt_manager_get_context
        if (!pci_handle && cxt_mgr) {
            pci_handle = cxt_manager_get_context(cxt_mgr, PCI_CXT_ID, 0);
            if (pci_handle) {
                printk(KERN_ERR "[cx511h-phase2] Debug: pci_handle from cxt_manager_get_context=%p\n", pci_handle);
            }
        }
        
        // Write doorbell register if we have a valid handle
        if (pci_handle) {
            pci_model_mmio_write(pci_handle, 0, 0x304, 0x01);
            printk(KERN_ERR "[cx511h-phase2] Initial doorbell sent after stream start\n");
        } else {
            printk(KERN_ERR "[cx511h-phase2] ERROR: No valid PCI handle, doorbell not sent!\n");
        }
    }

    //ite6805_set_freerun_screen(ite6805_handle,FALSE);    
}

static void cx511h_stream_off(framegrabber_handle_t handle)
{
    board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
    
    printk(KERN_ERR "[cx511h-ttl] Stream OFF - Cleaning up...\n");

    aver_xilinx_enable_video_streaming(board_v4l2_cxt->aver_xilinx_handle,FALSE);
}

static int cx511h_flash_dump(framegrabber_handle_t handle,int start_block, int blocks, U8_T *flash_dump) //
{
	board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
	//char version[10];
	int ret=0;
		
    //ret = aver_xilinx_flash_dump(board_v4l2_cxt->aver_xilinx_handle,start_block,blocks,flash_dump);
    
    return ret;
}

static int cx511h_flash_update(framegrabber_handle_t handle,int start_block, int blocks, U8_T *flash_update)
{
	board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
	//char version[10];
	int ret=0;
	
    //ret = aver_xilinx_flash_update(board_v4l2_cxt->aver_xilinx_handle,start_block,blocks,flash_update);
    
    return ret;
}

static void cx511h_brightness_read(framegrabber_handle_t handle,int *brightness)
{
	board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);//
    //handle_t ite6805_handle=board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0]; 
   
	//ite6805_get_brightness(ite6805_handle,&board_v4l2_cxt->cur_bchs_value); 
    //board_v4l2_cxt->cur_bchs_value = aver_xilinx_get_bright(board_v4l2_cxt->aver_xilinx_handle);
	
	//framegrabber_set_data(handle,board_v4l2_cxt);
	*brightness = board_v4l2_cxt->cur_bchs_value;
	//handle->fg_bchs_value = *brightness;
	debug_msg("%s get brightness=%x\n",__func__,board_v4l2_cxt->cur_bchs_value);
}
static void cx511h_contrast_read(framegrabber_handle_t handle,int *contrast)
{
	board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
    //handle_t ite6805_handle=board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0]; 
   
	//ite6805_get_contrast(ite6805_handle,&board_v4l2_cxt->cur_bchs_value); 
    //board_v4l2_cxt->cur_bchs_value = aver_xilinx_get_contrast(board_v4l2_cxt->aver_xilinx_handle);
	
	//framegrabber_set_data(handle,board_v4l2_cxt);
	*contrast = board_v4l2_cxt->cur_bchs_value;
	//handle->fg_bchs_value = *contrast;
	debug_msg("%s get contrast=%x\n",__func__,board_v4l2_cxt->cur_bchs_value);
}

static void cx511h_hue_read(framegrabber_handle_t handle,int *hue)
{
	board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
    //handle_t ite6805_handle=board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0]; 
   
	//ite6805_get_hue(ite6805_handle,&board_v4l2_cxt->cur_bchs_value); 
    //board_v4l2_cxt->cur_bchs_value = aver_xilinx_get_hue(board_v4l2_cxt->aver_xilinx_handle);	

	//framegrabber_set_data(handle,board_v4l2_cxt);
	*hue = board_v4l2_cxt->cur_bchs_value;
	//handle->fg_bchs_value = *hue;
	debug_msg("%s get hue=%x\n",__func__,board_v4l2_cxt->cur_bchs_value);
}

static void cx511h_saturation_read(framegrabber_handle_t handle,int *saturation)
{
	board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
    //handle_t ite6805_handle=board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0]; 
   
	//ite6805_get_saturation(ite6805_handle,&board_v4l2_cxt->cur_bchs_value); 
    //board_v4l2_cxt->cur_bchs_value = aver_xilinx_get_saturation(board_v4l2_cxt->aver_xilinx_handle);
	
	//framegrabber_set_data(handle,board_v4l2_cxt);
	*saturation = board_v4l2_cxt->cur_bchs_value;
	//handle->fg_bchs_value = *saturation;
	debug_msg("%s get saturation=%x\n",__func__,board_v4l2_cxt->cur_bchs_value);
}

static void cx511h_bchs_write(framegrabber_handle_t handle)
{
//	board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
	//handle_t ite6805_handle=board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0]; 
   
	int bchs_value = handle->fg_bchs_value;
	int bchs_select = handle->fg_bchs_selection;	
		
	
	//debug_msg("%s...bchs_select=%d\n",__func__,bchs_select);
		
	{
		//framegrabber_s_input_bchs(board_v4l2_cxt->fg_handle,board_v4l2_cxt->cur_bchs_value,board_v4l2_cxt->cur_bchs_selection);
		//ite6805_set_bchs(ite6805_handle,&bchs_value,&bchs_select); 
        //aver_xilinx_set_bchs(board_v4l2_cxt->aver_xilinx_handle,bchs_value,bchs_select); 
		if (bchs_select ==0) debug_msg("set brightness=%d\n",bchs_value);
		if (bchs_select ==1) debug_msg("set contrast=%d\n",bchs_value);
		if (bchs_select ==2) debug_msg("set hue=%d\n",bchs_value);
		if (bchs_select ==3) debug_msg("set saturation=%d\n",bchs_value);
	}
}

static void cx511h_hdcp_state_read(framegrabber_handle_t handle,int *hdcp_state)
{
	board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);

    aver_xilinx_get_hdcp_state(board_v4l2_cxt->aver_xilinx_handle,hdcp_state);
}

static void cx511h_hdcp_state_set(framegrabber_handle_t handle,int hdcp_state)
{
	board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);

    aver_xilinx_set_hdcp_state(board_v4l2_cxt->aver_xilinx_handle, hdcp_state);
}

static int cx511h_i2c_read(framegrabber_handle_t handle, unsigned char channel, unsigned int slave, unsigned int sub, unsigned char sublen, unsigned char *data, unsigned int datalen, unsigned int is_10bit)
{
    board_v4l2_context_t *board_v4l2_cxt = framegrabber_get_data(handle);
    i2c_model_handle_t i2c_mgr = board_v4l2_cxt->i2c_model_handle;
    cxt_mgr_handle_t cxt_mgr = cxt_mgr=get_cxt_manager_from_context(i2c_mgr);

    return board_i2c_read(cxt_mgr, channel, slave, sub, data, datalen);
}

static int cx511h_i2c_write(framegrabber_handle_t handle, unsigned char channel, unsigned int slave, unsigned int sub, unsigned char sublen, unsigned char *data, unsigned int datalen, unsigned int is_10bit)
{
    board_v4l2_context_t *board_v4l2_cxt = framegrabber_get_data(handle);
    i2c_model_handle_t i2c_mgr = board_v4l2_cxt->i2c_model_handle;
    cxt_mgr_handle_t cxt_mgr = cxt_mgr=get_cxt_manager_from_context(i2c_mgr);

    return board_i2c_write(cxt_mgr, channel, slave, sub, data, datalen);
}

static int cx511h_reg_read(framegrabber_handle_t handle, unsigned int offset, unsigned char n_bytes, unsigned int *data)
{
    board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
    int ret = 0;

    ret = aver_xilinx_read_register(board_v4l2_cxt->aver_xilinx_handle, offset, n_bytes, data);

    return ret;
}

static int cx511h_reg_write(framegrabber_handle_t handle, unsigned int offset, unsigned char n_bytes, unsigned int data)
{
    board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
    int ret = 0;

    mesg("%s +", __func__);
#if 1
    if (offset == 0 && data == 0)
    {
        //stop task
        board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
        handle_t ite6805_handle=board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0];

        ite6805_power_off(ite6805_handle);

        return 0;
    }
    else if (offset == 0 && data == 1)
    {
        //start task
        board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
        handle_t ite6805_handle=board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0];

        ite6805_power_on(ite6805_handle);

        return 0;
    }
#endif

    ret = aver_xilinx_write_register(board_v4l2_cxt->aver_xilinx_handle, offset, n_bytes, data);

    return ret;
}

static void cx511h_video_buffer_done(void *data)
{
    board_v4l2_context_t *board_v4l2_cxt=data;
    cxt_mgr_handle_t cxt_mgr;
    handle_t pci_handle;
    
    //mesg("%s board_v4l2_cxt %p\n",__func__,board_v4l2_cxt);
    if(board_v4l2_cxt)
    {
        v4l2_model_buffer_done(board_v4l2_cxt->v4l2_handle);
        
        // --- PHASE2 --- Doorbell and IRQ ACK for continuous streaming
        printk(KERN_ERR "[cx511h-phase2] Debug: board_v4l2_cxt=%p\n", board_v4l2_cxt);
        cxt_mgr = get_cxt_manager_from_context(board_v4l2_cxt);
        pci_handle = NULL;
        
        if (cxt_mgr) {
            printk(KERN_ERR "[cx511h-phase2] Debug: cxt_mgr=%p\n", cxt_mgr);
            pci_handle = pci_model_get_handle(cxt_mgr);
            if (pci_handle) {
                printk(KERN_ERR "[cx511h-phase2] Debug: pci_handle from pci_model_get_handle=%p\n", pci_handle);
            } else {
                printk(KERN_ERR "[cx511h-phase2] Debug: pci_model_get_handle returned NULL\n");
            }
        } else {
            printk(KERN_ERR "[cx511h-phase2] Debug: cxt_mgr is NULL\n");
        }
        
        // Fallback: use stored pci_handle from board context
        if (!pci_handle && board_v4l2_cxt->pci_handle) {
            pci_handle = board_v4l2_cxt->pci_handle;
            printk(KERN_ERR "[cx511h-phase2] Debug: using stored pci_handle=%p\n", pci_handle);
        }
        
        // If still NULL, try to get PCI handle via cxt_manager_get_context
        if (!pci_handle && cxt_mgr) {
            pci_handle = cxt_manager_get_context(cxt_mgr, PCI_CXT_ID, 0);
            if (pci_handle) {
                printk(KERN_ERR "[cx511h-phase2] Debug: pci_handle from cxt_manager_get_context=%p\n", pci_handle);
            }
        }
        
        // Write doorbell and IRQ ACK if we have a valid handle
        if (pci_handle) {
            // Doorbell: notify hardware next buffer is ready
            pci_model_mmio_write(pci_handle, 0, 0x304, 0x01);
            
            // IRQ ACK: acknowledge interrupt after buffer done
            pci_model_mmio_write(pci_handle, 0, 0x10, 0x02);
            wmb(); // write memory barrier
            
            printk(KERN_ERR "[cx511h-phase2] Doorbell and IRQ ACK sent\n");
        } else {
            printk(KERN_ERR "[cx511h-phase2] ERROR: No valid PCI handle, doorbell and IRQ ACK not sent!\n");
        }
    }
}

static void cx511h_v4l2_buffer_prepare(v4l2_model_callback_parameter_t *cb_info)
{
    board_v4l2_context_t *board_v4l2_cxt=cb_info->asso_data;
    v4l2_model_buffer_info_t *buffer_info=cb_info->u.buffer_prepare_info.buffer_info;
    //mesg("%s %p buffer_info %p\n",__func__,board_v4l2_cxt,buffer_info);
    if(buffer_info)
    {
        int i;
        v4l2_model_buf_desc_t *desc;
        unsigned framebufsize,remain;
        int width,height;
        unsigned bytesperline;
        
        framegrabber_g_out_framesize(board_v4l2_cxt->fg_handle,&width,&height);
        bytesperline=framegrabber_g_out_bytesperline(board_v4l2_cxt->fg_handle);
        framebufsize=bytesperline*height; 
        //debug_msg("%s %dx%d framesize %u\n",__func__,width,height,framebufsize);
      //  mesg("buf type %d count %d\n",buffer_info->buf_type,buffer_info->buf_count[0]);
        printk(KERN_ERR "[cx511h-dma] buffer_prepare: %dx%d bpl=%u framesize=%u buf_count=%d\n",
               width, height, bytesperline, framebufsize, buffer_info->buf_count[0]);

        for(i=0,desc=buffer_info->buf_info[0],remain=framebufsize;i<buffer_info->buf_count[0];i++)
        {
            printk(KERN_ERR "[cx511h-dma]   desc[%d]: dma_addr=0x%08lx size=0x%lx (%lu) remain=%u\n",
                   i, desc[i].addr, desc[i].size, desc[i].size, remain);

            if(remain >= desc[i].size)
            {
                aver_xilinx_add_to_cur_desclist(board_v4l2_cxt->aver_xilinx_handle,desc[i].addr,desc[i].size);
                remain -= desc[i].size;
            }else
            {
                aver_xilinx_add_to_cur_desclist(board_v4l2_cxt->aver_xilinx_handle,desc[i].addr,remain);
                remain =0;
                break;
            }
        }

        printk(KERN_ERR "[cx511h-dma] buffer_prepare: activating desclist (xilinx=%p)\n",
               board_v4l2_cxt->aver_xilinx_handle);
        aver_xilinx_active_current_desclist(board_v4l2_cxt->aver_xilinx_handle,cx511h_video_buffer_done,board_v4l2_cxt);   
    }  
}

static void cx511h_v4l2_buffer_init(v4l2_model_callback_parameter_t *cb_info)
{
//    board_v4l2_context_t *board_v4l2_cxt=cb_info->asso_data;
    v4l2_model_buffer_info_t *buffer_info=cb_info->u.buffer_prepare_info.buffer_info;
    //mesg("%s %p buffer_info %p\n",__func__,board_v4l2_cxt,buffer_info);
    if(buffer_info)
    {
        int i;
        v4l2_model_buf_desc_t *desc;
        //mesg("buf type %d count %d\n",buffer_info->buf_type,buffer_info->buf_count[0]);
        for(i=0,desc=buffer_info->buf_info[0];i<buffer_info->buf_count[0];i++)
        {
            //mesg("addr %08x size %x\n",desc[i].addr,desc[i].size);
        } 
    }  
}
/*
static void cx511h_v4l2_hardware_init(framegrabber_handle_t handle)
{
    board_v4l2_context_t *board_v4l2_cxt=framegrabber_get_data(handle);
    handle_t ite6805_handle=board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0]; 

    iTE6805_Hardware_Init(ite6805_handle);
}
*/
static void check_signal_stable_task(void *data)
{
    board_v4l2_context_t *board_v4l2_cxt=data;
    //ite6805_frameinfo_t *fe_frameinfo=&board_v4l2_cxt->cur_fe_frameinfo; 
    //handle_t ite6805_handle=board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0]; //check 20170511  
    int width,height;
    int dual_pixel;
    int dual_pixel_set;
    framegrabber_framemode_e framemode;
    aver_xilinx_frame_info_t detected_frameinfo;
    ite6805_frameinfo_t *fe_frameinfo=&board_v4l2_cxt->cur_fe_frameinfo; 
    BOOL_T is_interlace;
    
    debug_msg("%s\n",__func__);
    
    //ite6805_get_frameinfo(ite6805_handle,fe_frameinfo);
    //debug_msg("%s ite6805 detected size %dx%d;framerate %d \n",__func__,fe_frameinfo->width,fe_frameinfo->height,fe_frameinfo->framerate);
        
    aver_xilinx_get_frameinfo(board_v4l2_cxt->aver_xilinx_handle,&detected_frameinfo,fe_frameinfo->pixel_clock/*0*/); 
    framemode=framegrabber_g_input_framemode(board_v4l2_cxt->fg_handle);
    is_interlace=(framemode==FRAMEGRABBER_FRAMEMODE_INTERLACED) ? TRUE : FALSE;
    framegrabber_g_input_framesize(board_v4l2_cxt->fg_handle,&width,&height);
    dual_pixel = framegrabber_g_input_dualmode(board_v4l2_cxt->fg_handle);
    
    if (dual_pixel ==1)
        dual_pixel_set = detected_frameinfo.width*2;
    else
        dual_pixel_set = detected_frameinfo.width; 
    
    debug_msg("%s framegrabber size %dx%d\n",__func__,width,height);
    debug_msg("%s fpga detected size %dx%d\n",__func__,detected_frameinfo.width,detected_frameinfo.height);
    if((((detected_frameinfo.width<320 || detected_frameinfo.height<240)) || (width !=/*detected_frameinfo.width*/dual_pixel_set)|| (height !=detected_frameinfo.height)) && (cnt_retry<3))
    {
		cnt_retry++;
		//debug_msg("%s retry...in %dx%d  get %dx%d\n",__func__,width,height,dual_pixel_set,detected_frameinfo.height); //test
        task_model_run_task_after(board_v4l2_cxt->task_model_handle,board_v4l2_cxt->check_signal_task_handle,100000);
        return;
    }
    
    //debug_msg("%s fpga detected size %dx%d\n",__func__,detected_frameinfo.width,detected_frameinfo.height);
   
    if (fe_frameinfo->is_interlace)
    {
		detected_frameinfo.width = fe_frameinfo->width;
		detected_frameinfo.height = fe_frameinfo->height/2;
	}
	else
	{
		detected_frameinfo.width = fe_frameinfo->width;
		detected_frameinfo.height = fe_frameinfo->height;
	}
      
    
    if(width==detected_frameinfo.width && height==detected_frameinfo.height && (is_interlace==detected_frameinfo.is_interlace))
    {
		debug_msg("%s fix input %dx%d to fpga %dx%d\n",__func__,width,height,detected_frameinfo.width,detected_frameinfo.height);
        framegrabber_mask_s_status(board_v4l2_cxt->fg_handle,FRAMEGRABBER_STATUS_SIGNAL_LOCKED_BIT,FRAMEGRABBER_STATUS_SIGNAL_LOCKED_BIT);
        cnt_retry=0;
    }else
    {
        debug_msg("%s adjust input %dx%d to fpga %dx%d\n",__func__,width,height,detected_frameinfo.width,detected_frameinfo.height);
      
        width=detected_frameinfo.width;
        height=detected_frameinfo.height;
        
        framegrabber_s_input_framesize(board_v4l2_cxt->fg_handle,width,height);
        if(is_interlace!=detected_frameinfo.is_interlace)
        {
            framemode=(detected_frameinfo.is_interlace) ? FRAMEGRABBER_FRAMEMODE_INTERLACED:FRAMEGRABBER_FRAMEMODE_PROGRESS;
            framegrabber_s_input_framemode(board_v4l2_cxt->fg_handle,framemode);
        }
        task_model_run_task_after(board_v4l2_cxt->task_model_handle,board_v4l2_cxt->check_signal_task_handle,50000); // run again to comfirm
    }   
}

static void cx511h_ite6805_event(void *cxt,ite6805_event_e event) 
{
    board_v4l2_context_t *board_v4l2_cxt=cxt;
    ite6805_frameinfo_t *fe_frameinfo=&board_v4l2_cxt->cur_fe_frameinfo; 
    enum ite6805_audio_sample *fe_audioinfo=&board_v4l2_cxt->cur_fe_audioinfo; 
    handle_t ite6805_handle=board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0]; 
    ite6805_out_format_e out_format;
    unsigned int hdcp_flag;
    
    debug_msg("%s event %d\n",__func__,event);
    
    switch(event)
    {
    case ITE6805_LOCK:
    {
        //debug_msg("ITE6805_LOCK\n");
        cx511h_set_led_color(board_v4l2_cxt->aver_xilinx_handle, 0, 1, 0); // Green = signal locked
        //aver_xilinx_get_frameinfo(aver_xilinx_handle,&detected_frameinfo,fe_frameinfo->pixel_clock); 
        
        {
            //aver_xilinx_color_adjust_control(board_v4l2_cxt->aver_xilinx_handle);
            ite6805_get_frameinfo(ite6805_handle,fe_frameinfo);
            
//            aver_xilinx_hdmi_hotplug(board_v4l2_cxt->aver_xilinx_handle);
        
            mesg("%s locked fe %dx%d%c (raw)\n",__func__,fe_frameinfo->width,fe_frameinfo->height,(fe_frameinfo->is_interlace)?'i':'p');

            /* FORCED 1080p OVERRIDE: The ITE6805 EDID RAM still advertises 4K
             * (EDID is cached in chip). Force any resolution above 1920 wide
             * down to 1920x1080p60 so the FPGA/DMA pipeline can handle it.
             * pixel_clock is in Hz (downstream check: >170000000 triggers dual-pixel). */
            if (fe_frameinfo->width > 1920) {
                printk(KERN_ERR "[cx511h-debug] FORCING 1080p mode now "
                       "(was %dx%d pclk=%u dual=%d dpl=%d)\n",
                       fe_frameinfo->width, fe_frameinfo->height,
                       fe_frameinfo->pixel_clock, fe_frameinfo->dual_pixel,
                       fe_frameinfo->dual_pixel_like);
                fe_frameinfo->width = 1920;
                fe_frameinfo->height = 1080;
                fe_frameinfo->is_interlace = 0;
                fe_frameinfo->framerate = 60;
                fe_frameinfo->denominator = 1;
                fe_frameinfo->pixel_clock = 148500000;  /* 148.5 MHz in Hz */
                fe_frameinfo->dual_pixel = 0;
                fe_frameinfo->dual_pixel_like = 0;
                fe_frameinfo->sampling_mode = 0;  /* single pixel */
                fe_frameinfo->ddr_mode = 0;       /* SDR */
            }


   
//work around for ITE6805 detect issue      
            if ((fe_frameinfo->is_interlace==0) && ((fe_frameinfo->height == 240) || (fe_frameinfo->height == 288)))
            {
                //debug_msg("ITE6805 height adapter\n");
                fe_frameinfo->is_interlace =1;
                fe_frameinfo->height += fe_frameinfo->height;	
                fe_frameinfo->framerate /= 2;
            } 
            if ((fe_frameinfo->is_interlace==1) && ((fe_frameinfo->height == 480) || (fe_frameinfo->height == 576)))
            {
                //debug_msg("ITE6805 framerate adapter\n");	
                fe_frameinfo->framerate /= 2;
            } 
            ite6805_get_hdcp_level(ite6805_handle, &hdcp_flag);
            *fe_audioinfo = aver_xilinx_get_audioinfo(board_v4l2_cxt->aver_xilinx_handle);		
            framegrabber_s_input_audioinfo(board_v4l2_cxt->fg_handle,*fe_audioinfo);
			 
            framegrabber_s_input_framerate(board_v4l2_cxt->fg_handle,fe_frameinfo->framerate,fe_frameinfo->denominator);
            
            framegrabber_s_input_interlace(board_v4l2_cxt->fg_handle,fe_frameinfo->is_interlace);
            
            framegrabber_s_input_framesize(board_v4l2_cxt->fg_handle,fe_frameinfo->width,fe_frameinfo->height); //tt 0602
            
            framegrabber_s_input_dualmode(board_v4l2_cxt->fg_handle,fe_frameinfo->dual_pixel); //tt 1003
            
            framegrabber_s_hdcp_flag(board_v4l2_cxt->fg_handle,hdcp_flag);
            
            //aver_xilinx_dual_pixel(board_v4l2_cxt->aver_xilinx_handle,fe_frameinfo->dual_pixel);
     
            if((fe_frameinfo->pixel_clock>170000000/*150000*/) || (fe_frameinfo->dual_pixel_like ==1))//rr1012
            {
                //ite6805_out_format_e out_format;
                if(fe_frameinfo->packet_colorspace==CS_YUV)
                    out_format=ITE6805_OUT_FORMAT_SDR_422_2X24_INTERLEAVE_MODE0;//ITE6805_OUT_FORMAT_SDR_422_24_MODE4;//ADV7619_OUT_FORMAT_SDR_422_2X24_INTERLEAVE_MODE0; //1003
                else
                    out_format=ITE6805_OUT_FORMAT_SDR_444_2X24_INTERLEAVE_MODE0;
                ite6805_set_out_format(ite6805_handle,out_format);
                //ite6805_set_out_format(board_v4l2_cxt->i2c_chip_handle[CX511H_I2C_CHIP_ITE6805_1],out_format);
            }
            else
            {
                out_format=ITE6805_OUT_FORMAT_SDR_ITU656_24_MODE0;
                ite6805_set_out_format(ite6805_handle,out_format);
                //ite6805_set_out_format(board_v4l2_cxt->i2c_chip_handle[CX511H_I2C_CHIP_ITE6805_1],ITE6805_OUT_FORMAT_SDR_ITU656_24_MODE0);
            }
            //debug_msg("=========== ite6805_set_out_format=%02x\n",out_format);
        }
        //sys_msleep(100);
        //debug_msg("pixelclock %d detected %dx%d%c\n",fe_frameinfo->pixel_clock,detected_frameinfo.width,detected_frameinfo.height,(detected_frameinfo.is_interlace) ?'i':'p');
        framegrabber_s_input_status(board_v4l2_cxt->fg_handle,FRAMEGRABBER_INPUT_STATUS_OK);
        framegrabber_mask_s_status(board_v4l2_cxt->fg_handle,FRAMEGRABBER_STATUS_SIGNAL_LOCKED_BIT,FRAMEGRABBER_STATUS_SIGNAL_LOCKED_BIT);

        //task_model_run_task_after(board_v4l2_cxt->task_model_handle,board_v4l2_cxt->check_signal_task_handle,100000);
    }
        break;
    case ITE6805_UNLOCK:
        //debug_msg("ITE6805_UNLOCK\n");
        cx511h_set_led_color(board_v4l2_cxt->aver_xilinx_handle, 1, 0, 0); // Red = no signal
        framegrabber_s_hdcp_flag(board_v4l2_cxt->fg_handle,0);
        framegrabber_s_input_framesize(board_v4l2_cxt->fg_handle,0,0); //tt 0615
        framegrabber_mask_s_status(board_v4l2_cxt->fg_handle,FRAMEGRABBER_STATUS_SIGNAL_LOCKED_BIT,0);
        framegrabber_s_input_status(board_v4l2_cxt->fg_handle,FRAMEGRABBER_INPUT_STATUS_NO_SIGNAL);
        break;
        
    case ITE6805_HDCP:
        //debug_msg("ITE6805_HDCP\n");
        ite6805_get_hdcp_level(ite6805_handle, &hdcp_flag);
		framegrabber_s_hdcp_flag(board_v4l2_cxt->fg_handle,hdcp_flag);
		break;    
    case ITE6805_HPD_LOW:
        aver_xilinx_set_gpio_output(board_v4l2_cxt->aver_xilinx_handle, 2, 0);
        break;
    case ITE6805_HPD_HIGH:
        aver_xilinx_set_gpio_output(board_v4l2_cxt->aver_xilinx_handle, 2, 1);
        break;
    } 
   
}

void board_v4l2_init(cxt_mgr_handle_t cxt_mgr, int board_id)
{
    printk(KERN_ERR "DEBUG: EINTRITT IN board_v4l2_init board_id=%d\n", board_id);
    framegrabber_handle_t framegrabber_handle;
    board_v4l2_context_t *board_v4l2_cxt=NULL;
    v4l2_model_handle_t v4l2_handle;
    task_model_handle_t task_model_handle=NULL;
    i2c_model_handle_t  i2c_mgr=NULL;
    handle_t aver_xilinx_handle=NULL;
    task_handle_t task_handle=NULL;
    handle_t pci_handle=NULL;
    int i;
    
    enum
    {
        BOARD_V4L2_OK = 0,
        BOARD_V4L2_ERROR_CXT_MGR,
        BOARD_V4L2_ERROR_FRAMEGRABBER_INIT,
        BOARD_V4L2_ERROR_V4L2_MODEL_INIT,
        BOARD_V4L2_ERROR_ALLOC_CXT,
        BOARD_V4L2_ERROR_GET_MAIN_CHIP_HANDLE,
        BOARD_V4L2_ERROR_GET_I2C_MGR,
        BOARD_V4L2_ERROR_GET_TASK_MGR,
        BOARD_V4L2_ERROR_ALLOC_TASK,
    } err = BOARD_V4L2_OK;

    do
    {
        if (!cxt_mgr)
        {
            printk("board_v4l2_init: ERROR no cxt_mgr\n");
            err = BOARD_V4L2_ERROR_CXT_MGR;
            break;
        }
        printk("board_v4l2_init: cxt_mgr ok\n");
        
        // --- PHASE2 --- Get PCI handle for IRQ registration
        pci_handle = pci_model_get_handle(cxt_mgr);
        if (!pci_handle) {
            printk("board_v4l2_init: WARNING no PCI handle, IRQ ACK may not work\n");
        }

        framegrabber_handle = framegrabber_init(cxt_mgr, &cl511h_property, &cx511h_ops);
        if (framegrabber_handle == NULL)
        {
            printk("board_v4l2_init: ERROR framegrabber_init failed\n");
            err = BOARD_V4L2_ERROR_FRAMEGRABBER_INIT;
            break;
        }
        printk("board_v4l2_init: framegrabber_init ok\n");
        board_v4l2_cxt = cxt_manager_add_cxt(cxt_mgr, BOARD_V4L2_CXT_ID, board_v4l2_alloc, board_v4l2_release);
        if (!board_v4l2_cxt)
        {
            err = BOARD_V4L2_ERROR_ALLOC_CXT;
            break;
        }
        board_v4l2_cxt->fg_handle = framegrabber_handle;
        board_v4l2_cxt->pci_handle = pci_handle;
        printk(KERN_ERR "[cx511h-phase2] Debug: stored pci_handle=%p to board context\n", pci_handle);
        framegrabber_set_data(framegrabber_handle,board_v4l2_cxt);
        
        printk("board_v4l2_init: calling v4l2_model_init...\n");
        v4l2_handle = v4l2_model_init(cxt_mgr, &device_info,framegrabber_handle);
        if(v4l2_handle==NULL)
	    {
            printk("board_v4l2_init: ERROR v4l2_model_init returned NULL\n");
            err=BOARD_V4L2_ERROR_V4L2_MODEL_INIT;
		    break;
	    }
        printk("board_v4l2_init: v4l2_model_init ok, handle=%p\n", v4l2_handle);
	    board_v4l2_cxt->v4l2_handle=v4l2_handle;
        aver_xilinx_handle=cxt_manager_get_context(cxt_mgr,AVER_XILINX_CXT_ID,0);
	    if(aver_xilinx_handle==NULL)
	    {
            err=BOARD_V4L2_ERROR_GET_MAIN_CHIP_HANDLE;
            break;
	    }
            
        board_v4l2_cxt->aver_xilinx_handle=aver_xilinx_handle;
        i2c_mgr=cxt_manager_get_context(cxt_mgr,I2C_CXT_ID,0);
	    if(i2c_mgr==NULL)
	    {
            err=BOARD_V4L2_ERROR_GET_I2C_MGR;
            break;
	    }
	    board_v4l2_cxt->i2c_model_handle = i2c_mgr;
        for(i=0;i<CL511H_I2C_CHIP_COUNT;i++)
	    {
            board_v4l2_cxt->i2c_chip_handle[i]=i2c_model_get_nth_driver_handle(i2c_mgr,cl511h_chip_desc[i].name,cl511h_chip_desc[i].index);
            debug_msg("board_v4l2 i2c_chip_handle[%d] %p\n",i,board_v4l2_cxt->i2c_chip_handle[i]);
	    }
        task_model_handle=cxt_manager_get_context(cxt_mgr,TASK_MODEL_CXT_ID,0);
        if(task_model_handle==NULL)
        {
            err=BOARD_V4L2_ERROR_GET_TASK_MGR;
            break;
        }
        board_v4l2_cxt->task_model_handle=task_model_handle;
        cxt_manager_ref_context(task_model_handle);
        task_handle=task_model_create_task(task_model_handle,check_signal_stable_task,board_v4l2_cxt,"check_signal_task");
        if(task_handle==NULL)
        {
            err=BOARD_V4L2_ERROR_ALLOC_TASK;
            break;
        }
        
        board_v4l2_cxt->check_signal_task_handle=task_handle;
        
        aver_xilinx_hdmi_hotplug(board_v4l2_cxt->aver_xilinx_handle);
        
        v4l2_model_register_callback(v4l2_handle,V4L2_MODEL_CALLBACK_STREAMON ,&cx511h_v4l2_stream_on, board_v4l2_cxt);
        v4l2_model_register_callback(v4l2_handle,V4L2_MODEL_CALLBACK_STREAMOFF,&cx511h_v4l2_stream_off, board_v4l2_cxt);
        v4l2_model_register_callback(v4l2_handle,V4L2_MODEL_CALLBACK_BUFFER_PREPARE,&cx511h_v4l2_buffer_prepare, board_v4l2_cxt);
        v4l2_model_register_callback(v4l2_handle,V4L2_MODEL_CALLBACK_BUFFER_INIT,&cx511h_v4l2_buffer_init, board_v4l2_cxt);
      
        ite6805_register_callback(board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0],cx511h_ite6805_event,board_v4l2_cxt);
        framegrabber_start(framegrabber_handle);
        cxt_manager_ref_context(aver_xilinx_handle);

        /* Set initial LED color: blue = driver loaded, waiting for signal */
        cx511h_set_led_color(aver_xilinx_handle, 0, 0, 1);
        
        board_v4l2_cxt->board_id = board_id;
        //ite6805_get_board_id(board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0],&board_v4l2_cxt->board_id); 
        
    }
    while (0);
    if (err != BOARD_V4L2_OK)
    {
        switch (err)
        {
        case BOARD_V4L2_ERROR_ALLOC_TASK:
            cxt_manager_unref_context(task_model_handle);
        case BOARD_V4L2_ERROR_GET_TASK_MGR:
        case BOARD_V4L2_ERROR_GET_I2C_MGR:
        case BOARD_V4L2_ERROR_V4L2_MODEL_INIT:
        case BOARD_V4L2_ERROR_ALLOC_CXT:
        case BOARD_V4L2_ERROR_FRAMEGRABBER_INIT:
        case BOARD_V4L2_ERROR_CXT_MGR:
        default:
            break;
        }
        printk("board_v4l2_init: FAILED with err=%d\n", err);
        debug_msg("%s err %d\n", __func__, err);
    }
}

void board_v4l2_suspend(cxt_mgr_handle_t cxt_mgr)
{
    board_v4l2_context_t *board_v4l2_cxt;
    handle_t ite6805_handle;

    board_v4l2_cxt = cxt_manager_get_context(cxt_mgr,BOARD_V4L2_CXT_ID,0);
    if (!board_v4l2_cxt)
    {
        debug_msg("Error: cannot get board_v4l2_cxt");
        return;
    }

    ite6805_handle = board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0];
    if (!ite6805_handle)
    {
        debug_msg("Error: cannot get ite6805_handle");
        return;
    }

    ite6805_power_off(ite6805_handle);
}

void board_v4l2_resume(cxt_mgr_handle_t cxt_mgr)
{
    board_v4l2_context_t *board_v4l2_cxt;
    handle_t ite6805_handle;

    board_v4l2_cxt = cxt_manager_get_context(cxt_mgr,BOARD_V4L2_CXT_ID,0);
    if (!board_v4l2_cxt)
    {
        debug_msg("Error: cannot get board_v4l2_cxt");
        return;
    }

    ite6805_handle = board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0];
    if (!ite6805_handle)
    {
        debug_msg("Error: cannot get ite6805_handle");
        return;
    }

    ite6805_power_on(ite6805_handle);

    aver_xilinx_hdmi_hotplug(board_v4l2_cxt->aver_xilinx_handle);
}

void board_v4l2_stop(cxt_mgr_handle_t cxt_mgr)
{
    board_v4l2_context_t *board_v4l2_cxt;
    handle_t ite6805_handle;

    board_v4l2_cxt = cxt_manager_get_context(cxt_mgr,BOARD_V4L2_CXT_ID,0);
    if (!board_v4l2_cxt)
    {
        debug_msg("Error: cannot get board_v4l2_cxt");
        return;
    }

    /* Stop hardware video streaming */
    cx511h_stream_off(board_v4l2_cxt->fg_handle);
    
    /* Ensure V4L2 model stops streaming */
    v4l2_model_streamoff(board_v4l2_cxt->v4l2_handle);
    
    /* Power off ITE6805 chip */
    ite6805_handle = board_v4l2_cxt->i2c_chip_handle[CL511H_I2C_CHIP_ITE6805_0];
    if (ite6805_handle)
        ite6805_power_off(ite6805_handle);
    
    /* Set LED color to off (black) to indicate driver unloading */
    cx511h_set_led_color(board_v4l2_cxt->aver_xilinx_handle, 0, 0, 0);
    
    debug_msg("board_v4l2_stop done\n");
}
