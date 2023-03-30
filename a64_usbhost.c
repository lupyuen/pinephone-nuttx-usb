/****************************************************************************
 * boards/arm64/a64/a641020-evk/src/a64_usbhost.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <sched.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/kthread.h>
#include <nuttx/usb/usbdev.h>
#include <nuttx/usb/usbhost.h>
#include <nuttx/usb/usbdev_trace.h>
#include <nuttx/usb/ehci.h>

#include <a64_ehci.h>

// TODO #include "hardware/a64_pinmux.h"
#include "hardware/a64_usbotg.h"
// TODO #include "a64_periphclks.h"
// TODO #include "a641020-evk.h"
#include "arm64_arch.h"

#include <arch/board/board.h>  /* Must always be included last */

#if defined(CONFIG_A64_USBOTG) || defined(CONFIG_USBHOST)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_USBHOST_DEFPRIO
#  define CONFIG_USBHOST_DEFPRIO 50
#endif

#ifndef CONFIG_USBHOST_STACKSIZE
#  ifdef CONFIG_USBHOST_HUB
#    define CONFIG_USBHOST_STACKSIZE 1536
#  else
#    define CONFIG_USBHOST_STACKSIZE 1024
#  endif
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Retained device driver handle */

static struct usbhost_connection_s *g_ehciconn;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: ehci_waiter
 *
 * Description:
 *   Wait for USB devices to be connected to the EHCI root hub.
 *
 ****************************************************************************/

static int ehci_waiter(int argc, char *argv[])
{
  _info("!!!\n");////
  struct usbhost_hubport_s *hport;

  uinfo("ehci_waiter:  Running\n");
  for (; ; )
    {
      /* Wait for the device to change state */

      DEBUGVERIFY(CONN_WAIT(g_ehciconn, &hport));
      syslog(LOG_INFO, "ehci_waiter: %s\n",
             hport->connected ? "connected" : "disconnected");

      /* Did we just become connected? */

      if (hport->connected)
        {
          /* Yes.. enumerate the newly connected device */

          _info("@@@\n");////
          CONN_ENUMERATE(g_ehciconn, hport);
        }
    }

  /* Keep the compiler from complaining */

  return 0;
}

#define A64_CCU_ADDR        0x01c20000 /* CCU             0x01c2:0000-0x01c2:03ff 1K */

// Set the bit at an address
static void set_bit(unsigned long addr, uint8_t bit)
{
  _info("0x%lx Bit %d\n", addr, bit);
  modreg32(1 << bit, 1 << bit, addr);
}

// Enable USB Clocks
// https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-clocks
// https://github.com/lupyuen/pinephone-nuttx-usb#enable-usb-controller-clocks
static void a64_usbhost_clk_enable(void)
{
  // usb0_phy: CLK_USB_PHY0
  // 0x0cc BIT(8)
  _info("CLK_USB_PHY0\n");
  #define CLK_USB_PHY0 (A64_CCU_ADDR + 0x0cc)
  #define CLK_USB_PHY0_BIT 8
  set_bit(CLK_USB_PHY0, CLK_USB_PHY0_BIT);

  // usb1_phy: CLK_USB_PHY1
  // 0x0cc BIT(9)
  _info("CLK_USB_PHY1\n");
  #define CLK_USB_PHY1 (A64_CCU_ADDR + 0x0cc)
  #define CLK_USB_PHY1_BIT 9
  set_bit(CLK_USB_PHY1, CLK_USB_PHY1_BIT);

  // EHCI0: CLK_BUS_OHCI0, CLK_BUS_EHCI0, CLK_USB_OHCI0
  // 0x060 BIT(28)
  _info("CLK_BUS_OHCI0\n");
  #define CLK_BUS_OHCI0 (A64_CCU_ADDR + 0x060)
  #define CLK_BUS_OHCI0_BIT 28
  set_bit(CLK_BUS_OHCI0, CLK_BUS_OHCI0_BIT);

  // 0x060 BIT(24)
  _info("CLK_BUS_EHCI0\n");
  #define CLK_BUS_EHCI0 (A64_CCU_ADDR + 0x060)
  #define CLK_BUS_EHCI0_BIT 24
  set_bit(CLK_BUS_EHCI0, CLK_BUS_EHCI0_BIT);

  // 0x0cc BIT(16)
  _info("CLK_USB_OHCI0\n");
  #define CLK_USB_OHCI0 (A64_CCU_ADDR + 0x0cc)
  #define CLK_USB_OHCI0_BIT 16
  set_bit(CLK_USB_OHCI0, CLK_USB_OHCI0_BIT);

  // EHCI1: CLK_BUS_OHCI1, CLK_BUS_EHCI1, CLK_USB_OHCI1
  // 0x060 BIT(29)
  _info("CLK_BUS_OHCI1\n");
  #define CLK_BUS_OHCI1 (A64_CCU_ADDR + 0x060)
  #define CLK_BUS_OHCI1_BIT 29
  set_bit(CLK_BUS_OHCI1, CLK_BUS_OHCI1_BIT);

  // 0x060 BIT(25)
  _info("CLK_BUS_EHCI1\n");
  #define CLK_BUS_EHCI1 (A64_CCU_ADDR + 0x060)
  #define CLK_BUS_EHCI1_BIT 25
  set_bit(CLK_BUS_EHCI1, CLK_BUS_EHCI1_BIT);

  // 0x0cc BIT(17)
  _info("CLK_USB_OHCI1\n");
  #define CLK_USB_OHCI1 (A64_CCU_ADDR + 0x0cc)
  #define CLK_USB_OHCI1_BIT 17
  set_bit(CLK_USB_OHCI1, CLK_USB_OHCI1_BIT);
}

// Deassert USB Resets
// https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-reset
// https://github.com/lupyuen/pinephone-nuttx-usb#reset-usb-controller
static void a64_usbhost_reset_deassert(void)
{
  // usb0_reset: RST_USB_PHY0
  // 0x0cc BIT(0)
  _info("RST_USB_PHY0\n");
  #define RST_USB_PHY0 (A64_CCU_ADDR + 0x0cc)
  #define RST_USB_PHY0_BIT 0
  set_bit(RST_USB_PHY0, RST_USB_PHY0_BIT);

  // usb1_reset: RST_USB_PHY1
  // 0x0cc BIT(1)
  _info("RST_USB_PHY1\n");
  #define RST_USB_PHY1 (A64_CCU_ADDR + 0x0cc)
  #define RST_USB_PHY1_BIT 1
  set_bit(RST_USB_PHY1, RST_USB_PHY1_BIT);

  // EHCI0: RST_BUS_OHCI0, RST_BUS_EHCI0
  // 0x2c0 BIT(28)
  _info("RST_BUS_OHCI0\n");
  #define RST_BUS_OHCI0 (A64_CCU_ADDR + 0x2c0)
  #define RST_BUS_OHCI0_BIT 28
  set_bit(RST_BUS_OHCI0, RST_BUS_OHCI0_BIT);

  // 0x2c0 BIT(24)
  _info("RST_BUS_EHCI0\n");
  #define RST_BUS_EHCI0 (A64_CCU_ADDR + 0x2c0)
  #define RST_BUS_EHCI0_BIT 24
  set_bit(RST_BUS_EHCI0, RST_BUS_EHCI0_BIT);

  // EHCI1: RST_BUS_OHCI1, RST_BUS_EHCI1
  // 0x2c0 BIT(29)
  _info("RST_BUS_OHCI1\n");
  #define RST_BUS_OHCI1 (A64_CCU_ADDR + 0x2c0)
  #define RST_BUS_OHCI1_BIT 29
  set_bit(RST_BUS_OHCI1, RST_BUS_OHCI1_BIT);

  // 0x2c0 BIT(25)
  _info("RST_BUS_EHCI1\n");
  #define RST_BUS_EHCI1 (A64_CCU_ADDR + 0x2c0)
  #define RST_BUS_EHCI1_BIT 25
  set_bit(RST_BUS_EHCI1, RST_BUS_EHCI1_BIT);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: a64_usbhost_initialize
 *
 * Description:
 *   Called at application startup time to initialize the USB host
 *   functionality.
 *   This function will start a thread that will monitor for device
 *   connection/disconnection events.
 *
 ****************************************************************************/

int a64_usbhost_initialize(void)
{
  int ret;

  // Enable the USB Clocks
  a64_usbhost_clk_enable();

  // Deassert the USB Resets
  a64_usbhost_reset_deassert();

  _info("TODO: a64_clockall_usboh3\n");////
  // TODO: a64_clockall_usboh3();

  /* Make sure we don't accidentally switch on USB bus power */

  _info("TODO: switch off USB bus power\n");////
  // TODO: *((uint32_t *)A64_USBNC_USB_OTG1_CTRL) = USBNC_PWR_POL;
  // TODO: *((uint32_t *)0x400d9030)                = (1 << 21);
  // TODO: *((uint32_t *)0x400d9000)                = 0;

  /* Setup pins, with power initially off */

  _info("TODO: Setup pins, with power initially off\n");////
  // TODO: a64_config_gpio(GPIO_USBOTG_PWR);
  // TODO: a64_config_gpio(GPIO_USBOTG_OC);
  // TODO: a64_config_gpio(GPIO_USBOTG_ID);

  /* First, register all of the class drivers needed to support the drivers
   * that we care about
   */

#ifdef CONFIG_USBHOST_HUB
  /* Initialize USB hub support */

  ret = usbhost_hub_initialize();
  if (ret < 0)
    {
      syslog(LOG_ERR, "ERROR: usbhost_hub_initialize failed: %d\n", ret);
    }
#endif

#ifdef CONFIG_USBHOST_MSC
  /* Register the USB host Mass Storage Class */

  ret = usbhost_msc_initialize();
  if (ret != OK)
    {
      syslog(LOG_ERR,
             "ERROR: Failed to register the mass storage class: %d\n", ret);
    }
#endif

#ifdef CONFIG_USBHOST_CDCACM
  /* Register the CDC/ACM serial class */

  ret = usbhost_cdcacm_initialize();
  if (ret != OK)
    {
      uerr("ERROR: Failed to register the CDC/ACM serial class\n");
    }
#endif

#ifdef CONFIG_USBHOST_HIDKBD
  /* Register the USB host HID keyboard class driver */

  ret = usbhost_kbdinit();
  if (ret != OK)
    {
      uerr("ERROR: Failed to register the KBD class\n");
    }
#endif

  /* Then get an instance of the USB EHCI interface. */

  g_ehciconn = a64_ehci_initialize(0);

  if (!g_ehciconn)
    {
      uerr("ERROR: a64_ehci_initialize failed\n");
      return -ENODEV;
    }

  /* Start a thread to handle device connection. */

  _info("1\n");////
  ret = kthread_create("EHCI Monitor", CONFIG_USBHOST_DEFPRIO,
                       CONFIG_USBHOST_STACKSIZE,
                       ehci_waiter, NULL);
  if (ret < 0)
    {
      _info("2\n");////
      uerr("ERROR: Failed to create ehci_waiter task: %d\n", ret);
      return -ENODEV;
    }

  return OK;
}

/****************************************************************************
 * Name: a64_usbhost_vbusdrive
 *
 * Description:
 *   Enable/disable driving of VBUS 5V output.  This function must be
 *   provided by each platform that implements the OHCI or EHCI host
 *   interface
 *
 * Input Parameters:
 *   rhport - Selects root hub port to be powered host interface.
 *            Since the A64 has only a downstream port, zero is
 *            the only possible value for this parameter.
 *   enable - true: enable VBUS power; false: disable VBUS power
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

#define HCOR ((volatile struct ehci_hcor_s *)A64_USBOTG_HCOR_BASE)

void a64_usbhost_vbusdrive(int rhport, bool enable)
{
  uint32_t regval;

  uinfo("RHPort%d: enable=%d\n", rhport + 1, enable);

  /* The A64 has only a single root hub port */

  if (rhport == 0)
    {
      /* Then enable or disable VBUS power */

      regval = HCOR->portsc[rhport];
      regval &= ~EHCI_PORTSC_PP;
      if (enable)
        {
          regval |= EHCI_PORTSC_PP;
        }

      HCOR->portsc[rhport] = regval;
    }
}

/****************************************************************************
 * Name: a64_setup_overcurrent
 *
 * Description:
 *   Setup to receive an interrupt-level callback if an overcurrent condition
 *   is detected.
 *
 * Input Parameters:
 *   handler - New overcurrent interrupt handler
 *   arg     - The argument that will accompany the interrupt
 *
 * Returned Value:
 *   Zero (OK) returned on success; a negated errno value is returned on
 *   failure.
 *
 ****************************************************************************/

#if 0 /* Not ready yet */
int a64_setup_overcurrent(xcpt_t handler, void *arg)
{
  irqstate_t flags;

  /* Disable interrupts until we are done.  This guarantees that the
   * following operations are atomic.
   */

  flags = enter_critical_section();

  /* Configure the interrupt */

#warning Missing logic

  leave_critical_section(flags);
  return OK;
}
#endif /* 0 */

#endif /* CONFIG_A64_USBOTG || CONFIG_USBHOST */
