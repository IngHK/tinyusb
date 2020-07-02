/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Scott Shawcroft, 2019 William D. Jones for Adafruit Industries
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2020 Jan Duempelmann
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#include "tusb_option.h"

#if defined (STM32F105x8) || defined (STM32F105xB) || defined (STM32F105xC) || \
    defined (STM32F107xB) || defined (STM32F107xC)
#define STM32F1_SYNOPSYS
#endif

#if defined (STM32L475xx) || defined (STM32L476xx) ||                          \
    defined (STM32L485xx) || defined (STM32L486xx) || defined (STM32L496xx) || \
    defined (STM32L4R5xx) || defined (STM32L4R7xx) || defined (STM32L4R9xx) || \
    defined (STM32L4S5xx) || defined (STM32L4S7xx) || defined (STM32L4S9xx)
#define STM32L4_SYNOPSYS
#endif

#if TUSB_OPT_DEVICE_ENABLED && \
    ( (CFG_TUSB_MCU == OPT_MCU_STM32F1 && defined(STM32F1_SYNOPSYS)) || \
      CFG_TUSB_MCU == OPT_MCU_STM32F2 || \
      CFG_TUSB_MCU == OPT_MCU_STM32F4 || \
      CFG_TUSB_MCU == OPT_MCU_STM32F7 || \
      CFG_TUSB_MCU == OPT_MCU_STM32H7 || \
      (CFG_TUSB_MCU == OPT_MCU_STM32L4 && defined(STM32L4_SYNOPSYS)) \
    )

// EP_MAX       : Max number of bi-directional endpoints including EP0
// EP_FIFO_SIZE : Size of dedicated USB SRAM
#if CFG_TUSB_MCU == OPT_MCU_STM32F1
  #include "stm32f1xx.h"
  #define EP_MAX_FS       4
  #define EP_FIFO_SIZE_FS 1280

#elif CFG_TUSB_MCU == OPT_MCU_STM32F2
  #include "stm32f2xx.h"
  #define EP_MAX_FS       USB_OTG_FS_MAX_IN_ENDPOINTS
  #define EP_FIFO_SIZE_FS USB_OTG_FS_TOTAL_FIFO_SIZE

#elif CFG_TUSB_MCU == OPT_MCU_STM32F4
  #include "stm32f4xx.h"
  #define EP_MAX_FS       USB_OTG_FS_MAX_IN_ENDPOINTS
  #define EP_FIFO_SIZE_FS USB_OTG_FS_TOTAL_FIFO_SIZE
  #define EP_MAX_HS       USB_OTG_HS_MAX_IN_ENDPOINTS
  #define EP_FIFO_SIZE_HS USB_OTG_HS_TOTAL_FIFO_SIZE

#elif CFG_TUSB_MCU == OPT_MCU_STM32H7
  #include "stm32h7xx.h"
  #define EP_MAX_FS       9
  #define EP_FIFO_SIZE_FS 4096
  #define EP_MAX_HS       9
  #define EP_FIFO_SIZE_HS 4096

#elif CFG_TUSB_MCU == OPT_MCU_STM32F7
  #include "stm32f7xx.h"
  #define EP_MAX_FS       6
  #define EP_FIFO_SIZE_FS 1280
  #define EP_MAX_HS       9
  #define EP_FIFO_SIZE_HS 4096

#elif CFG_TUSB_MCU == OPT_MCU_STM32L4
  #include "stm32l4xx.h"
  #define EP_MAX_FS       6
  #define EP_FIFO_SIZE_FS 1280

#else
  #error "Unsupported MCUs"

#endif

#include "device/dcd.h"

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

typedef struct
{
  uint32_t regs;  // registers
  const IRQn_Type irqnum; // IRQ number
//  const uint8_t ep_count; // Max bi-directional Endpoints
}dcd_rhport_t;

// To be consistent across stm32 port. We will number OTG_FS as Rhport0, and OTG_HS as Rhport1
static const dcd_rhport_t _dcd_rhport[] =
{
  { .regs = USB_OTG_FS_PERIPH_BASE, .irqnum = OTG_FS_IRQn }

#ifdef USB_OTG_HS
 ,{ .regs = USB_OTG_HS_PERIPH_BASE, .irqnum = OTG_HS_IRQn }
#endif
};

#define GLOBAL_BASE(_port)     ((USB_OTG_GlobalTypeDef*) _dcd_rhport[_port].regs)
#define DEVICE_BASE(_port)     (USB_OTG_DeviceTypeDef *) (_dcd_rhport[_port].regs + USB_OTG_DEVICE_BASE)
#define OUT_EP_BASE(_port)     (USB_OTG_OUTEndpointTypeDef *) (_dcd_rhport[_port].regs + USB_OTG_OUT_ENDPOINT_BASE)
#define IN_EP_BASE(_port)      (USB_OTG_INEndpointTypeDef *) (_dcd_rhport[_port].regs + USB_OTG_IN_ENDPOINT_BASE)
#define FIFO_BASE(_port, _x)   ((volatile uint32_t *) (_dcd_rhport[_port].regs + USB_OTG_FIFO_BASE + (_x) * USB_OTG_FIFO_SIZE))

enum
{
  DCD_HIGH_SPEED        = 0, // Highspeed mode
  DCD_FULL_SPEED_USE_HS = 1, // Full speed in Highspeed port (probably with internal PHY)
  DCD_FULL_SPEED        = 3, // Full speed with internal PHY
};

/*------------------------------------------------------------------*/
/* MACRO TYPEDEF CONSTANT ENUM
 *------------------------------------------------------------------*/

// Since TinyUSB doesn't use SOF for now, and this interrupt too often (1ms interval)
// We disable SOF for now until needed later on
#define USE_SOF     0

static TU_ATTR_ALIGNED(4) uint32_t _setup_packet[2];

typedef struct {
  uint8_t * buffer;
  uint16_t total_len;
  uint16_t max_size;
} xfer_ctl_t;

typedef volatile uint32_t * usb_fifo_t;

#if TUD_OPT_RHPORT == 1
  #define EP_MAX        EP_MAX_HS
  #define EP_FIFO_SIZE  EP_FIFO_SIZE_HS
#else
  #define EP_MAX        EP_MAX_FS
  #define EP_FIFO_SIZE  EP_FIFO_SIZE_FS
#endif

xfer_ctl_t xfer_status[EP_MAX][2];
#define XFER_CTL_BASE(_ep, _dir) &xfer_status[_ep][_dir]

// EP0 transfers are limited to 1 packet - larger sizes has to be split
static uint16_t ep0_pending[2];     // Index determines direction as tusb_dir_t type

// FIFO RAM allocation so far in words
static uint16_t _allocated_fifo_words;

// Setup the control endpoint 0.
static void bus_reset(uint8_t rhport)
{
  USB_OTG_GlobalTypeDef * usb_otg = GLOBAL_BASE(rhport);
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE(rhport);
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE(rhport);
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE(rhport);

  tu_memclr(xfer_status, sizeof(xfer_status));

  for(uint8_t n = 0; n < EP_MAX; n++) {
    out_ep[n].DOEPCTL |= USB_OTG_DOEPCTL_SNAK;
  }

  dev->DAINTMSK |= (1 << USB_OTG_DAINTMSK_OEPM_Pos) | (1 << USB_OTG_DAINTMSK_IEPM_Pos);
  dev->DOEPMSK |= USB_OTG_DOEPMSK_STUPM | USB_OTG_DOEPMSK_XFRCM;
  dev->DIEPMSK |= USB_OTG_DIEPMSK_TOM | USB_OTG_DIEPMSK_XFRCM;

  // "USB Data FIFOs" section in reference manual
  // Peripheral FIFO architecture
  //
  // --------------- 320 or 1024 ( 1280 or 4096 bytes )
  // | IN FIFO MAX |
  // ---------------
  // |    ...      |
  // --------------- y + x + 16 + GRXFSIZ
  // | IN FIFO 2   |
  // --------------- x + 16 + GRXFSIZ
  // | IN FIFO 1   |
  // --------------- 16 + GRXFSIZ
  // | IN FIFO 0   |
  // --------------- GRXFSIZ
  // | OUT FIFO    |
  // | ( Shared )  |
  // --------------- 0
  //
  // According to "FIFO RAM allocation" section in RM, FIFO RAM are allocated as follows (each word 32-bits):
  // - Each EP IN needs at least max packet size, 16 words is sufficient for EP0 IN
  //
  // - All EP OUT shared a unique OUT FIFO which uses
  //   - 13 for setup packets + control words (up to 3 setup packets).
  //   - 1 for global NAK (not required/used here).
  //   - Largest-EPsize / 4 + 1. ( FS: 64 bytes, HS: 512 bytes). Recommended is  "2 x (Largest-EPsize/4) + 1"
  //   - 2 for each used OUT endpoint
  //
  //   Therefore GRXFSIZ = 13 + 1 + 1 + 2 x (Largest-EPsize/4) + 2 x EPOUTnum
  //   - FullSpeed (64 Bytes ): GRXFSIZ = 15 + 2 x  16 + 2 x EP_MAX = 47  + 2 x EP_MAX
  //   - Highspeed (512 bytes): GRXFSIZ = 15 + 2 x 128 + 2 x EP_MAX = 271 + 2 x EP_MAX
  //
  //   NOTE: Largest-EPsize & EPOUTnum is actual used endpoints in configuration. Since DCD has no knowledge
  //   of the overall picture yet. We will use the worst scenario: largest possible + EP_MAX
  //
  //   FIXME: for Isochronous, largest EP size can be 1023/1024 for FS/HS respectively. In addition if multiple ISO
  //   are enabled at least "2 x (Largest-EPsize/4) + 1" are recommended.  Maybe provide a macro for application to
  //   overwrite this.

#if TUD_OPT_HIGH_SPEED
  _allocated_fifo_words = 271 + 2*EP_MAX;
#else
  _allocated_fifo_words =  47 + 2*EP_MAX;
#endif

  usb_otg->GRXFSIZ = _allocated_fifo_words;

  // Control IN uses FIFO 0 with 64 bytes ( 16 32-bit word )
  usb_otg->DIEPTXF0_HNPTXFSIZ = (16 << USB_OTG_TX0FD_Pos) | _allocated_fifo_words;

  _allocated_fifo_words += 16;

  // TU_LOG2_INT(_allocated_fifo_words);

  // Fixed control EP0 size to 64 bytes
  in_ep[0].DIEPCTL &= ~(0x03 << USB_OTG_DIEPCTL_MPSIZ_Pos);
  xfer_status[0][TUSB_DIR_OUT].max_size = xfer_status[0][TUSB_DIR_IN].max_size = 64;

  out_ep[0].DOEPTSIZ |= (3 << USB_OTG_DOEPTSIZ_STUPCNT_Pos);

  usb_otg->GINTMSK |= USB_OTG_GINTMSK_OEPINT | USB_OTG_GINTMSK_IEPINT;
}

// Set turn-around timeout according to link speed
static void set_turnaround(USB_OTG_GlobalTypeDef * usb_otg, tusb_speed_t speed)
{
  usb_otg->GUSBCFG &= ~USB_OTG_GUSBCFG_TRDT;

  if ( speed == TUSB_SPEED_HIGH )
  {
    // Use fixed 0x09 for Highspeed
    usb_otg->GUSBCFG |= (0x09 << USB_OTG_GUSBCFG_TRDT_Pos);
  }
  else
  {
    // Turnaround timeout depends on the MCU clock
    extern uint32_t SystemCoreClock;
    uint32_t turnaround;

    if ( SystemCoreClock >= 32000000U )
      turnaround = 0x6U;
    else if ( SystemCoreClock >= 27500000U )
      turnaround = 0x7U;
    else if ( SystemCoreClock >= 24000000U )
      turnaround = 0x8U;
    else if ( SystemCoreClock >= 21800000U )
      turnaround = 0x9U;
    else if ( SystemCoreClock >= 20000000U )
      turnaround = 0xAU;
    else if ( SystemCoreClock >= 18500000U )
      turnaround = 0xBU;
    else if ( SystemCoreClock >= 17200000U )
      turnaround = 0xCU;
    else if ( SystemCoreClock >= 16000000U )
      turnaround = 0xDU;
    else if ( SystemCoreClock >= 15000000U )
      turnaround = 0xEU;
    else
      turnaround = 0xFU;

    // Fullspeed depends on MCU clocks, but we will use 0x06 for 32+ Mhz
    usb_otg->GUSBCFG |= (turnaround << USB_OTG_GUSBCFG_TRDT_Pos);
  }
}

static tusb_speed_t get_speed(uint8_t rhport)
{
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE(rhport);
  uint32_t const enum_spd = (dev->DSTS & USB_OTG_DSTS_ENUMSPD_Msk) >> USB_OTG_DSTS_ENUMSPD_Pos;
  return (enum_spd == DCD_HIGH_SPEED) ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL;
}

static void set_speed(uint8_t rhport, tusb_speed_t speed)
{
  uint32_t bitvalue;

  if ( rhport == 1 )
  {
    bitvalue = ((TUSB_SPEED_HIGH == speed) ? DCD_HIGH_SPEED : DCD_FULL_SPEED_USE_HS);
  }
  else
  {
    bitvalue = DCD_FULL_SPEED;
  }

  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE(rhport);

  // Clear and set speed bits
  dev->DCFG &= ~(3 << USB_OTG_DCFG_DSPD_Pos);
  dev->DCFG |= (bitvalue << USB_OTG_DCFG_DSPD_Pos);
}

#if defined(USB_HS_PHYC) && TUD_OPT_HIGH_SPEED
static bool USB_HS_PHYCInit(void)
{
  USB_HS_PHYC_GlobalTypeDef *usb_hs_phyc = (USB_HS_PHYC_GlobalTypeDef*) USB_HS_PHYC_CONTROLLER_BASE;

  // Enable LDO
  usb_hs_phyc->USB_HS_PHYC_LDO |= USB_HS_PHYC_LDO_ENABLE;

  // Wait until LDO ready
  while ( 0 == (usb_hs_phyc->USB_HS_PHYC_LDO & USB_HS_PHYC_LDO_STATUS) ) {}

  uint32_t phyc_pll = 0;

  // TODO Try to get HSE_VALUE from registers instead of depending CFLAGS
  switch ( HSE_VALUE )
  {
    case 12000000: phyc_pll = USB_HS_PHYC_PLL1_PLLSEL_12MHZ   ; break;
    case 12500000: phyc_pll = USB_HS_PHYC_PLL1_PLLSEL_12_5MHZ ; break;
    case 16000000: phyc_pll = USB_HS_PHYC_PLL1_PLLSEL_16MHZ   ; break;
    case 24000000: phyc_pll = USB_HS_PHYC_PLL1_PLLSEL_24MHZ   ; break;
    case 25000000: phyc_pll = USB_HS_PHYC_PLL1_PLLSEL_25MHZ   ; break;
    case 32000000: phyc_pll = USB_HS_PHYC_PLL1_PLLSEL_Msk     ; break; // Value not defined in header
    default:
      TU_ASSERT(0);
  }
  usb_hs_phyc->USB_HS_PHYC_PLL = phyc_pll;

  // Control the tuning interface of the High Speed PHY
  // Use magic value (USB_HS_PHYC_TUNE_VALUE) from ST driver
  usb_hs_phyc->USB_HS_PHYC_TUNE |= 0x00000F13U;

  // Enable PLL internal PHY
  usb_hs_phyc->USB_HS_PHYC_PLL |= USB_HS_PHYC_PLL_PLLEN;

  // Original ST code has 2 ms delay for PLL stabilization.
  // Primitive test shows that more than 10 USB un/replug cycle showed no error with enumeration

  return true;
}
#endif

static void edpt_schedule_packets(uint8_t rhport, uint8_t const epnum, uint8_t const dir, uint16_t const num_packets, uint16_t total_bytes) {
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE(rhport);
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE(rhport);
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE(rhport);

  // EP0 is limited to one packet each xfer
  // We use multiple transaction of xfer->max_size length to get a whole transfer done
  if(epnum == 0) {
    xfer_ctl_t * const xfer = XFER_CTL_BASE(epnum, dir);
    total_bytes = tu_min16(ep0_pending[dir], xfer->max_size);
    ep0_pending[dir] -= total_bytes;
  }

  // IN and OUT endpoint xfers are interrupt-driven, we just schedule them here.
  if(dir == TUSB_DIR_IN) {
    // A full IN transfer (multiple packets, possibly) triggers XFRC.
    in_ep[epnum].DIEPTSIZ = (num_packets << USB_OTG_DIEPTSIZ_PKTCNT_Pos) |
        ((total_bytes << USB_OTG_DIEPTSIZ_XFRSIZ_Pos) & USB_OTG_DIEPTSIZ_XFRSIZ_Msk);

    in_ep[epnum].DIEPCTL |= USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_CNAK;
    // Enable fifo empty interrupt only if there are something to put in the fifo.
    if(total_bytes != 0) {
      dev->DIEPEMPMSK |= (1 << epnum);
    }
  } else {
    // A full OUT transfer (multiple packets, possibly) triggers XFRC.
    out_ep[epnum].DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_PKTCNT_Msk | USB_OTG_DOEPTSIZ_XFRSIZ);
    out_ep[epnum].DOEPTSIZ |= (num_packets << USB_OTG_DOEPTSIZ_PKTCNT_Pos) |
        ((total_bytes << USB_OTG_DOEPTSIZ_XFRSIZ_Pos) & USB_OTG_DOEPTSIZ_XFRSIZ_Msk);

    out_ep[epnum].DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
  }
}

/*------------------------------------------------------------------*/
/* Controller API
 *------------------------------------------------------------------*/
void dcd_init (uint8_t rhport)
{
  // Programming model begins in the last section of the chapter on the USB
  // peripheral in each Reference Manual.

  USB_OTG_GlobalTypeDef * usb_otg = GLOBAL_BASE(rhport);

  // No HNP/SRP (no OTG support), program timeout later.
#if TUD_OPT_HIGH_SPEED   // TODO may pass parameter instead of using macro for HighSpeed
  if ( rhport == 1 )
  {
    // On selected MCUs HS port1 can be used with external PHY via ULPI interface

    // deactivate internal PHY
    usb_otg->GCCFG &= ~USB_OTG_GCCFG_PWRDWN;

    // Init The UTMI Interface
    usb_otg->GUSBCFG &= ~(USB_OTG_GUSBCFG_TSDPS | USB_OTG_GUSBCFG_ULPIFSLS | USB_OTG_GUSBCFG_PHYSEL);

    // Select default internal VBUS Indicator and Drive for ULPI
    usb_otg->GUSBCFG &= ~(USB_OTG_GUSBCFG_ULPIEVBUSD | USB_OTG_GUSBCFG_ULPIEVBUSI);

    #if defined(USB_HS_PHYC)
    // Highspeed with embedded UTMI PHYC

    // Select UTMI Interface
    usb_otg->GUSBCFG &= ~USB_OTG_GUSBCFG_ULPI_UTMI_SEL;
    usb_otg->GCCFG |= USB_OTG_GCCFG_PHYHSEN;

    // Enables control of a High Speed USB PHY
    USB_HS_PHYCInit();
    #endif
  }
  else
#endif
  {
    // Enable internal PHY
    usb_otg->GUSBCFG |= USB_OTG_GUSBCFG_PHYSEL;
  }

  // Reset core after selecting PHY
  // Wait AHB IDLE, reset then wait until it is cleared
  while ((usb_otg->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL) == 0U) {}
  usb_otg->GRSTCTL |= USB_OTG_GRSTCTL_CSRST;
  while ((usb_otg->GRSTCTL & USB_OTG_GRSTCTL_CSRST) == USB_OTG_GRSTCTL_CSRST) {}

  // Restart PHY clock
  *((volatile uint32_t *)(_dcd_rhport[rhport].regs + USB_OTG_PCGCCTL_BASE)) = 0;

  // Clear all interrupts
  usb_otg->GINTSTS |= usb_otg->GINTSTS;

  // Required as part of core initialization.
  // TODO: How should mode mismatch be handled? It will cause
  // the core to stop working/require reset.
  usb_otg->GINTMSK |= USB_OTG_GINTMSK_OTGINT | USB_OTG_GINTMSK_MMISM;

  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE(rhport);

  // If USB host misbehaves during status portion of control xfer
  // (non zero-length packet), send STALL back and discard.
  dev->DCFG |=  USB_OTG_DCFG_NZLSOHSK;

  set_speed(rhport, TUD_OPT_HIGH_SPEED ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL);

  // Enable internal USB transceiver.
  if ( rhport == 0 ) usb_otg->GCCFG |= USB_OTG_GCCFG_PWRDWN;

  usb_otg->GINTMSK |= USB_OTG_GINTMSK_USBRST   | USB_OTG_GINTMSK_ENUMDNEM |
                      USB_OTG_GINTMSK_USBSUSPM | USB_OTG_GINTMSK_WUIM     |
                      USB_OTG_GINTMSK_RXFLVLM  | (USE_SOF ? USB_OTG_GINTMSK_SOFM : 0);

  // Enable global interrupt
  usb_otg->GAHBCFG |= USB_OTG_GAHBCFG_GINT;
}

void dcd_int_enable (uint8_t rhport)
{
  NVIC_EnableIRQ(_dcd_rhport[rhport].irqnum);
}

void dcd_int_disable (uint8_t rhport)
{
  NVIC_DisableIRQ(_dcd_rhport[rhport].irqnum);
}

void dcd_set_address (uint8_t rhport, uint8_t dev_addr)
{
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE(rhport);
  dev->DCFG |= (dev_addr << USB_OTG_DCFG_DAD_Pos) & USB_OTG_DCFG_DAD_Msk;

  // Response with status after changing device address
  dcd_edpt_xfer(rhport, tu_edpt_addr(0, TUSB_DIR_IN), NULL, 0);
}

void dcd_remote_wakeup(uint8_t rhport)
{
  (void) rhport;
}

void dcd_connect(uint8_t rhport)
{
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE(rhport);

  dev->DCTL &= ~USB_OTG_DCTL_SDIS;
}

void dcd_disconnect(uint8_t rhport)
{
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE(rhport);

  dev->DCTL |= USB_OTG_DCTL_SDIS;
}


/*------------------------------------------------------------------*/
/* DCD Endpoint port
 *------------------------------------------------------------------*/

bool dcd_edpt_open (uint8_t rhport, tusb_desc_endpoint_t const * desc_edpt)
{
  USB_OTG_GlobalTypeDef * usb_otg = GLOBAL_BASE(rhport);
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE(rhport);
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE(rhport);
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE(rhport);

  uint8_t const epnum = tu_edpt_number(desc_edpt->bEndpointAddress);
  uint8_t const dir   = tu_edpt_dir(desc_edpt->bEndpointAddress);

  TU_ASSERT(epnum < EP_MAX);

  // TODO ISO endpoint can be up to 1024 bytes
  TU_ASSERT(desc_edpt->wMaxPacketSize.size <= (get_speed(rhport) == TUSB_SPEED_HIGH ? 512 : 64));

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);
  xfer->max_size = desc_edpt->wMaxPacketSize.size;

  if(dir == TUSB_DIR_OUT)
  {
    out_ep[epnum].DOEPCTL |= (1 << USB_OTG_DOEPCTL_USBAEP_Pos) |
                             (desc_edpt->bmAttributes.xfer << USB_OTG_DOEPCTL_EPTYP_Pos) |
                             (desc_edpt->wMaxPacketSize.size << USB_OTG_DOEPCTL_MPSIZ_Pos);

    dev->DAINTMSK |= (1 << (USB_OTG_DAINTMSK_OEPM_Pos + epnum));
  }
  else
  {
    // "USB Data FIFOs" section in reference manual
    // Peripheral FIFO architecture
    //
    // --------------- 320 or 1024 ( 1280 or 4096 bytes )
    // | IN FIFO MAX |
    // ---------------
    // |    ...      |
    // --------------- y + x + 16 + GRXFSIZ
    // | IN FIFO 2   |
    // --------------- x + 16 + GRXFSIZ
    // | IN FIFO 1   |
    // --------------- 16 + GRXFSIZ
    // | IN FIFO 0   |
    // --------------- GRXFSIZ
    // | OUT FIFO    |
    // | ( Shared )  |
    // --------------- 0
    //
    // In FIFO is allocated by following rules:
    // - IN EP 1 gets FIFO 1, IN EP "n" gets FIFO "n".
    // - Offset: allocated so far
    // - Size
    //    - Interrupt is EPSize
    //    - Bulk/ISO is max(EPSize, remaining-fifo / non-opened-EPIN)

    uint16_t const fifo_remaining = EP_FIFO_SIZE/4 - _allocated_fifo_words;
    uint16_t fifo_size = desc_edpt->wMaxPacketSize.size / 4;

    if ( desc_edpt->bmAttributes.xfer != TUSB_XFER_INTERRUPT )
    {
      uint8_t opened = 0;
      for(uint8_t i = 0; i < EP_MAX; i++)
      {
        if ( (i != epnum) && (xfer_status[i][TUSB_DIR_IN].max_size > 0) ) opened++;
      }

      // EP Size or equally divided of remaining whichever is larger
      fifo_size = tu_max16(fifo_size, fifo_remaining / (EP_MAX - opened));
    }

    // FIFO overflows, we probably need a better allocating scheme
    TU_ASSERT(fifo_size <= fifo_remaining);

    // DIEPTXF starts at FIFO #1.
    // Both TXFD and TXSA are in unit of 32-bit words.
    usb_otg->DIEPTXF[epnum - 1] = (fifo_size << USB_OTG_DIEPTXF_INEPTXFD_Pos) | _allocated_fifo_words;

    _allocated_fifo_words += fifo_size;

    in_ep[epnum].DIEPCTL |= (1 << USB_OTG_DIEPCTL_USBAEP_Pos) |
                            (epnum << USB_OTG_DIEPCTL_TXFNUM_Pos) |
                            (desc_edpt->bmAttributes.xfer << USB_OTG_DIEPCTL_EPTYP_Pos) |
                            (desc_edpt->bmAttributes.xfer != TUSB_XFER_ISOCHRONOUS ? USB_OTG_DOEPCTL_SD0PID_SEVNFRM : 0) |
                            (desc_edpt->wMaxPacketSize.size << USB_OTG_DIEPCTL_MPSIZ_Pos);

    dev->DAINTMSK |= (1 << (USB_OTG_DAINTMSK_IEPM_Pos + epnum));
  }

  return true;
}

bool dcd_edpt_xfer (uint8_t rhport, uint8_t ep_addr, uint8_t * buffer, uint16_t total_bytes)
{
  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, dir);
  xfer->buffer      = buffer;
  xfer->total_len   = total_bytes;

  // EP0 can only handle one packet
  if(epnum == 0) {
    ep0_pending[dir] = total_bytes;
    // Schedule the first transaction for EP0 transfer
    edpt_schedule_packets(rhport, epnum, dir, 1, ep0_pending[dir]);
    return true;
  }

  uint16_t num_packets = (total_bytes / xfer->max_size);
  uint8_t const short_packet_size = total_bytes % xfer->max_size;

  // Zero-size packet is special case.
  if(short_packet_size > 0 || (total_bytes == 0)) {
    num_packets++;
  }

  // Schedule packets to be sent within interrupt
  edpt_schedule_packets(rhport, epnum, dir, num_packets, total_bytes);

  return true;
}

// TODO: The logic for STALLing and disabling an endpoint is very similar
// (send STALL versus NAK handshakes back). Refactor into resuable function.
void dcd_edpt_stall (uint8_t rhport, uint8_t ep_addr)
{
  USB_OTG_GlobalTypeDef * usb_otg = GLOBAL_BASE(rhport);
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE(rhport);
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE(rhport);
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE(rhport);

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  if(dir == TUSB_DIR_IN) {
    // Only disable currently enabled non-control endpoint
    if ( (epnum == 0) || !(in_ep[epnum].DIEPCTL & USB_OTG_DIEPCTL_EPENA) ){
      in_ep[epnum].DIEPCTL |= (USB_OTG_DIEPCTL_SNAK | USB_OTG_DIEPCTL_STALL);
    } else {
      // Stop transmitting packets and NAK IN xfers.
      in_ep[epnum].DIEPCTL |= USB_OTG_DIEPCTL_SNAK;
      while((in_ep[epnum].DIEPINT & USB_OTG_DIEPINT_INEPNE) == 0);

      // Disable the endpoint.
      in_ep[epnum].DIEPCTL |= (USB_OTG_DIEPCTL_STALL | USB_OTG_DIEPCTL_EPDIS);
      while((in_ep[epnum].DIEPINT & USB_OTG_DIEPINT_EPDISD_Msk) == 0);
      in_ep[epnum].DIEPINT = USB_OTG_DIEPINT_EPDISD;
    }

    // Flush the FIFO, and wait until we have confirmed it cleared.
    usb_otg->GRSTCTL |= ((epnum - 1) << USB_OTG_GRSTCTL_TXFNUM_Pos);
    usb_otg->GRSTCTL |= USB_OTG_GRSTCTL_TXFFLSH;
    while((usb_otg->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH_Msk) != 0);
  } else {
    // Only disable currently enabled non-control endpoint
    if ( (epnum == 0) || !(out_ep[epnum].DOEPCTL & USB_OTG_DOEPCTL_EPENA) ){
      out_ep[epnum].DOEPCTL |= USB_OTG_DOEPCTL_STALL;
    } else {
      // Asserting GONAK is required to STALL an OUT endpoint.
      // Simpler to use polling here, we don't use the "B"OUTNAKEFF interrupt
      // anyway, and it can't be cleared by user code. If this while loop never
      // finishes, we have bigger problems than just the stack.
      dev->DCTL |= USB_OTG_DCTL_SGONAK;
      while((usb_otg->GINTSTS & USB_OTG_GINTSTS_BOUTNAKEFF_Msk) == 0);

      // Ditto here- disable the endpoint.
      out_ep[epnum].DOEPCTL |= (USB_OTG_DOEPCTL_STALL | USB_OTG_DOEPCTL_EPDIS);
      while((out_ep[epnum].DOEPINT & USB_OTG_DOEPINT_EPDISD_Msk) == 0);
      out_ep[epnum].DOEPINT = USB_OTG_DOEPINT_EPDISD;

      // Allow other OUT endpoints to keep receiving.
      dev->DCTL |= USB_OTG_DCTL_CGONAK;
    }
  }
}

void dcd_edpt_clear_stall (uint8_t rhport, uint8_t ep_addr)
{
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE(rhport);
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE(rhport);

  uint8_t const epnum = tu_edpt_number(ep_addr);
  uint8_t const dir   = tu_edpt_dir(ep_addr);

  if(dir == TUSB_DIR_IN) {
    in_ep[epnum].DIEPCTL &= ~USB_OTG_DIEPCTL_STALL;

    uint8_t eptype = (in_ep[epnum].DIEPCTL & USB_OTG_DIEPCTL_EPTYP_Msk) >> USB_OTG_DIEPCTL_EPTYP_Pos;
    // Required by USB spec to reset DATA toggle bit to DATA0 on interrupt and bulk endpoints.
    if(eptype == 2 || eptype == 3) {
      in_ep[epnum].DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
    }
  } else {
    out_ep[epnum].DOEPCTL &= ~USB_OTG_DOEPCTL_STALL;

    uint8_t eptype = (out_ep[epnum].DOEPCTL & USB_OTG_DOEPCTL_EPTYP_Msk) >> USB_OTG_DOEPCTL_EPTYP_Pos;
    // Required by USB spec to reset DATA toggle bit to DATA0 on interrupt and bulk endpoints.
    if(eptype == 2 || eptype == 3) {
      out_ep[epnum].DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM;
    }
  }
}

/*------------------------------------------------------------------*/

// Read a single data packet from receive FIFO
static void read_fifo_packet(uint8_t rhport, uint8_t * dst, uint16_t len){
  usb_fifo_t rx_fifo = FIFO_BASE(rhport, 0);

  // Reading full available 32 bit words from fifo
  uint16_t full_words = len >> 2;
  for(uint16_t i = 0; i < full_words; i++) {
    uint32_t tmp = *rx_fifo;
    dst[0] = tmp & 0x000000FF;
    dst[1] = (tmp & 0x0000FF00) >> 8;
    dst[2] = (tmp & 0x00FF0000) >> 16;
    dst[3] = (tmp & 0xFF000000) >> 24;
    dst += 4;
  }

  // Read the remaining 1-3 bytes from fifo
  uint8_t bytes_rem = len & 0x03;
  if(bytes_rem != 0) {
    uint32_t tmp = *rx_fifo;
    dst[0] = tmp & 0x000000FF;
    if(bytes_rem > 1) {
      dst[1] = (tmp & 0x0000FF00) >> 8;
    }
    if(bytes_rem > 2) {
      dst[2] = (tmp & 0x00FF0000) >> 16;
    }
  }
}

// Write a single data packet to EPIN FIFO
static void write_fifo_packet(uint8_t rhport, uint8_t fifo_num, uint8_t * src, uint16_t len){
  usb_fifo_t tx_fifo = FIFO_BASE(rhport, fifo_num);

  // Pushing full available 32 bit words to fifo
  uint16_t full_words = len >> 2;
  for(uint16_t i = 0; i < full_words; i++){
    *tx_fifo = (src[3] << 24) | (src[2] << 16) | (src[1] << 8) | src[0];
    src += 4;
  }

  // Write the remaining 1-3 bytes into fifo
  uint8_t bytes_rem = len & 0x03;
  if(bytes_rem){
    uint32_t tmp_word = 0;
    tmp_word |= src[0];
    if(bytes_rem > 1){
      tmp_word |= src[1] << 8;
    }
    if(bytes_rem > 2){
      tmp_word |= src[2] << 16;
    }
    *tx_fifo = tmp_word;
  }
}

static void handle_rxflvl_ints(uint8_t rhport, USB_OTG_OUTEndpointTypeDef * out_ep) {
  USB_OTG_GlobalTypeDef * usb_otg = GLOBAL_BASE(rhport);
  usb_fifo_t rx_fifo = FIFO_BASE(rhport, 0);

  // Pop control word off FIFO
  uint32_t ctl_word = usb_otg->GRXSTSP;
  uint8_t pktsts = (ctl_word & USB_OTG_GRXSTSP_PKTSTS_Msk) >> USB_OTG_GRXSTSP_PKTSTS_Pos;
  uint8_t epnum = (ctl_word &  USB_OTG_GRXSTSP_EPNUM_Msk) >>  USB_OTG_GRXSTSP_EPNUM_Pos;
  uint16_t bcnt = (ctl_word & USB_OTG_GRXSTSP_BCNT_Msk) >> USB_OTG_GRXSTSP_BCNT_Pos;

  switch(pktsts) {
    case 0x01: // Global OUT NAK (Interrupt)
    break;

    case 0x02: // Out packet recvd
    {
      xfer_ctl_t * xfer = XFER_CTL_BASE(epnum, TUSB_DIR_OUT);

      // Read packet off RxFIFO
      read_fifo_packet(rhport, xfer->buffer, bcnt);

      // Increment pointer to xfer data
      xfer->buffer += bcnt;

      // Truncate transfer length in case of short packet
      if(bcnt < xfer->max_size) {
        xfer->total_len -= (out_ep[epnum].DOEPTSIZ & USB_OTG_DOEPTSIZ_XFRSIZ_Msk) >> USB_OTG_DOEPTSIZ_XFRSIZ_Pos;
        if(epnum == 0) {
          xfer->total_len -= ep0_pending[TUSB_DIR_OUT];
          ep0_pending[TUSB_DIR_OUT] = 0;
        }
      }
    }
    break;

    case 0x03: // Out packet done (Interrupt)
      break;

    case 0x04: // Setup packet done (Interrupt)
      out_ep[epnum].DOEPTSIZ |= (3 << USB_OTG_DOEPTSIZ_STUPCNT_Pos);
    break;

    case 0x06: // Setup packet recvd
      // We can receive up to three setup packets in succession, but
      // only the last one is valid.
      _setup_packet[0] = (* rx_fifo);
      _setup_packet[1] = (* rx_fifo);
    break;

    default: // Invalid
      TU_BREAKPOINT();
    break;
  }
}

static void handle_epout_ints(uint8_t rhport, USB_OTG_DeviceTypeDef * dev, USB_OTG_OUTEndpointTypeDef * out_ep) {
  // DAINT for a given EP clears when DOEPINTx is cleared.
  // OEPINT will be cleared when DAINT's out bits are cleared.
  for(uint8_t n = 0; n < EP_MAX; n++) {
    xfer_ctl_t * xfer = XFER_CTL_BASE(n, TUSB_DIR_OUT);

    if(dev->DAINT & (1 << (USB_OTG_DAINT_OEPINT_Pos + n))) {
      // SETUP packet Setup Phase done.
      if(out_ep[n].DOEPINT & USB_OTG_DOEPINT_STUP) {
        out_ep[n].DOEPINT =  USB_OTG_DOEPINT_STUP;
        dcd_event_setup_received(rhport, (uint8_t*) &_setup_packet[0], true);
      }

      // OUT XFER complete
      if(out_ep[n].DOEPINT & USB_OTG_DOEPINT_XFRC) {
        out_ep[n].DOEPINT = USB_OTG_DOEPINT_XFRC;

        // EP0 can only handle one packet
        if((n == 0) && ep0_pending[TUSB_DIR_OUT]) {
          // Schedule another packet to be received.
          edpt_schedule_packets(rhport, n, TUSB_DIR_OUT, 1, ep0_pending[TUSB_DIR_OUT]);
        } else {
          dcd_event_xfer_complete(rhport, n, xfer->total_len, XFER_RESULT_SUCCESS, true);
        }
      }
    }
  }
}

static void handle_epin_ints(uint8_t rhport, USB_OTG_DeviceTypeDef * dev, USB_OTG_INEndpointTypeDef * in_ep) {
  // DAINT for a given EP clears when DIEPINTx is cleared.
  // IEPINT will be cleared when DAINT's out bits are cleared.
  for ( uint8_t n = 0; n < EP_MAX; n++ )
  {
    xfer_ctl_t *xfer = XFER_CTL_BASE(n, TUSB_DIR_IN);

    if ( dev->DAINT & (1 << (USB_OTG_DAINT_IEPINT_Pos + n)) )
    {
      // IN XFER complete (entire xfer).
      if ( in_ep[n].DIEPINT & USB_OTG_DIEPINT_XFRC )
      {
        in_ep[n].DIEPINT = USB_OTG_DIEPINT_XFRC;

        // EP0 can only handle one packet
        if((n == 0) && ep0_pending[TUSB_DIR_IN]) {
          // Schedule another packet to be transmitted.
          edpt_schedule_packets(rhport, n, TUSB_DIR_IN, 1, ep0_pending[TUSB_DIR_IN]);
        } else {
          dcd_event_xfer_complete(rhport, n | TUSB_DIR_IN_MASK, xfer->total_len, XFER_RESULT_SUCCESS, true);
        }
      }

      // XFER FIFO empty
      if ( (in_ep[n].DIEPINT & USB_OTG_DIEPINT_TXFE) && (dev->DIEPEMPMSK & (1 << n)) )
      {
        // DIEPINT's TXFE bit is read-only, software cannot clear it.
        // It will only be cleared by hardware when written bytes is more than
        // - 64 bytes or
        // - Half of TX FIFO size (configured by DIEPTXF)

        uint16_t remaining_packets = (in_ep[n].DIEPTSIZ & USB_OTG_DIEPTSIZ_PKTCNT_Msk) >> USB_OTG_DIEPTSIZ_PKTCNT_Pos;

        // Process every single packet (only whole packets can be written to fifo)
        for(uint16_t i = 0; i < remaining_packets; i++){
          uint16_t remaining_bytes = (in_ep[n].DIEPTSIZ & USB_OTG_DIEPTSIZ_XFRSIZ_Msk) >> USB_OTG_DIEPTSIZ_XFRSIZ_Pos;
          // Packet can not be larger than ep max size
          uint16_t packet_size = tu_min16(remaining_bytes, xfer->max_size);

          // It's only possible to write full packets into FIFO. Therefore DTXFSTS register of current
          // EP has to be checked if the buffer can take another WHOLE packet
          if(packet_size > ((in_ep[n].DTXFSTS & USB_OTG_DTXFSTS_INEPTFSAV_Msk) << 2)){
            break;
          }

          // Push packet to Tx-FIFO
          write_fifo_packet(rhport, n, xfer->buffer, packet_size);

          // Increment pointer to xfer data
          xfer->buffer += packet_size;
        }

        // Turn off TXFE if all bytes are written.
        if (((in_ep[n].DIEPTSIZ & USB_OTG_DIEPTSIZ_XFRSIZ_Msk) >> USB_OTG_DIEPTSIZ_XFRSIZ_Pos) == 0)
        {
          dev->DIEPEMPMSK &= ~(1 << n);
        }
      }
    }
  }
}

void dcd_int_handler(uint8_t rhport)
{
  USB_OTG_GlobalTypeDef * usb_otg = GLOBAL_BASE(rhport);
  USB_OTG_DeviceTypeDef * dev = DEVICE_BASE(rhport);
  USB_OTG_OUTEndpointTypeDef * out_ep = OUT_EP_BASE(rhport);
  USB_OTG_INEndpointTypeDef * in_ep = IN_EP_BASE(rhport);

  uint32_t int_status = usb_otg->GINTSTS;

  if(int_status & USB_OTG_GINTSTS_USBRST) {
    // USBRST is start of reset.
    usb_otg->GINTSTS = USB_OTG_GINTSTS_USBRST;
    bus_reset(rhport);
  }

  if(int_status & USB_OTG_GINTSTS_ENUMDNE) {
    // ENUMDNE is the end of reset where speed of the link is detected

    usb_otg->GINTSTS = USB_OTG_GINTSTS_ENUMDNE;

    tusb_speed_t const speed = get_speed(rhport);

    set_turnaround(usb_otg, speed);
    dcd_event_bus_reset(rhport, speed, true);
  }

  if(int_status & USB_OTG_GINTSTS_USBSUSP)
  {
    usb_otg->GINTSTS = USB_OTG_GINTSTS_USBSUSP;
    dcd_event_bus_signal(rhport, DCD_EVENT_SUSPEND, true);
  }

  if(int_status & USB_OTG_GINTSTS_WKUINT)
  {
    usb_otg->GINTSTS = USB_OTG_GINTSTS_WKUINT;
    dcd_event_bus_signal(rhport, DCD_EVENT_RESUME, true);
  }

  if(int_status & USB_OTG_GINTSTS_OTGINT)
  {
    // OTG INT bit is read-only
    uint32_t const otg_int = usb_otg->GOTGINT;

    if (otg_int & USB_OTG_GOTGINT_SEDET)
    {
      dcd_event_bus_signal(rhport, DCD_EVENT_UNPLUGGED, true);
    }

    usb_otg->GOTGINT = otg_int;
  }

#if USE_SOF
  if(int_status & USB_OTG_GINTSTS_SOF) {
    usb_otg->GINTSTS = USB_OTG_GINTSTS_SOF;
    dcd_event_bus_signal(rhport, DCD_EVENT_SOF, true);
  }
#endif

  // RxFIFO non-empty interrupt handling.
  if(int_status & USB_OTG_GINTSTS_RXFLVL) {
    // RXFLVL bit is read-only

    // Mask out RXFLVL while reading data from FIFO
    usb_otg->GINTMSK &= ~USB_OTG_GINTMSK_RXFLVLM;

    // Loop until all available packets were handled
    do {
      handle_rxflvl_ints(rhport, out_ep);
      int_status = usb_otg->GINTSTS;
    } while(int_status & USB_OTG_GINTSTS_RXFLVL);

    usb_otg->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM;
  }

  // OUT endpoint interrupt handling.
  if(int_status & USB_OTG_GINTSTS_OEPINT) {
    // OEPINT is read-only
    handle_epout_ints(rhport, dev, out_ep);
  }

  // IN endpoint interrupt handling.
  if(int_status & USB_OTG_GINTSTS_IEPINT) {
    // IEPINT bit read-only
    handle_epin_ints(rhport, dev, in_ep);
  }
}

#endif
