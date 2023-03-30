![USB Controller Block Diagram in Allwinner A64 User Manual (Page 583)](https://lupyuen.github.io/images/usb3-title.jpg)

# PinePhone USB Driver for Apache NuttX RTOS

Read the articles...

-   ["NuttX RTOS for PinePhone: Simpler USB with EHCI (Enhanced Host Controller Interface)"](https://lupyuen.github.io/articles/usb3)

-   ["NuttX RTOS for PinePhone: Exploring USB"](https://lupyuen.github.io/articles/usb2)

We're porting the [NXP i.MX RT EHCI USB Driver](https://github.com/apache/nuttx/blob/master/arch/arm/src/imxrt/imxrt_ehci.c#L4970) from Apache NuttX RTOS to PinePhone and Allwinner A64...

-   ["Enhanced Host Controller Interface for USB"](https://lupyuen.github.io/articles/usb2#appendix-enhanced-host-controller-interface-for-usb)

To add the PinePhone USB Driver to our NuttX Project...

```bash
cd nuttx/arch/arm64/src/a64
git submodule add https://github.com/lupyuen/pinephone-nuttx-usb
ln -s pinephone-nuttx-usb/a64_ehci.c .
ln -s pinephone-nuttx-usb/a64_ehci.h .
ln -s pinephone-nuttx-usb/a64_usbhost.c .
pushd hardware
ln -s ../pinephone-nuttx-usb/a64_usbotg.h .
popd
```

Add the USB Driver to Kconfig, Makefile and PinePhone Bringup...

-   [arch/arm64/src/a64/Kconfig](https://github.com/lupyuen2/wip-pinephone-nuttx/pull/26/files#diff-9121fb138b3c72a19188ce2e9e3ba478f943ce1c6a231312b8520f2e375dff1e)

-   [arch/arm64/src/a64/Make.defs](https://github.com/lupyuen2/wip-pinephone-nuttx/pull/26/files#diff-90722370fba9b923c4e5e0a9f6dad7ca4d9f4884867e107b4d0380a056ef9e09)

-   [boards/arm64/a64/pinephone/src/pinephone_bringup.c](https://github.com/lupyuen2/wip-pinephone-nuttx/pull/26/files#diff-112a7881b2d4b766a1ec106dfa92a4e963f7001ef7efebdadf7b6310d64c517e)

Configure the NuttX Build...

```bash
tools/configure.sh pinephone:lvgl
make menuconfig
```

Select these options in `menuconfig`...

-   Enable "Build Setup > Debug Options > USB Error, Warninigs and Info"

-   Enable "System Type > Allwinner A64 Peripheral Selection > USB EHCI"

-   Enable "RTOS Features > Work queue support > Low priority (kernel) worker thread"

-   Enable "Device Drivers > USB Host Driver Support"

-   Enable "Device Drivers > USB Host Driver Support > USB Hub Support"

Let's boot the NuttX USB EHCI Driver on PinePhone...

# 64-Bit Update for EHCI Driver

When PinePhone boots the NuttX USB EHCI Driver, it halts with an Assertion Failure...

```text
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: chip/a64_ehci.c:4996 task: nsh_main 0x4008b0d0
```

Here's the assertion, which says that the `a64_qh_s` struct must be aligned to 32 bytes...

https://github.com/lupyuen/pinephone-nuttx-usb/blob/b80499b3b8ec837fe2110e9476e8a6ad0f194cde/a64_ehci.c#L4996

Size of the `a64_qh_s` struct is 72 bytes...

```text
sizeof(struct a64_qh_s)=72
```

Which isn't aligned to 32 bytes...

https://github.com/lupyuen/pinephone-nuttx-usb/blob/b80499b3b8ec837fe2110e9476e8a6ad0f194cde/a64_ehci.c#L186-L200

Because it contains a 64-bit pointer `epinfo`...

https://github.com/lupyuen/pinephone-nuttx-usb/blob/b80499b3b8ec837fe2110e9476e8a6ad0f194cde/a64_ehci.c#L197

_How has `a64_qh_s` changed for 32-bit platforms vs 64-bit platforms?_

On 32-bit platforms: `a64_qh_s` was previously 64 bytes. (48 + 4 + 4 + 8)

On 64-bit platforms: `a64_qh_s` is now 72 bytes. (48 + 8 + 4 + 8, round up for 4-byte alignment)

In the EHCI Driver we need to align `a64_qh_s` to 32 bytes. So we pad `a64_qh_s` from 72 bytes to 96 bytes...

```c
uint8_t pad2[96 - 72]; // TODO: Pad from 72 to 96 bytes for 64-bit platform
```

Like this...

https://github.com/lupyuen/pinephone-nuttx-usb/blob/2e1f9ab090b14f88afb8c3a36ec40a0dbbb23d49/a64_ehci.c#L190-L202

Which fixes the Assertion Failure.

_What about other structs?_

To be safe, we verified that the other Struct Sizes are still valid for 64-bit platforms...

https://github.com/lupyuen/pinephone-nuttx-usb/blob/2e1f9ab090b14f88afb8c3a36ec40a0dbbb23d49/a64_ehci.c#L4999-L5004

Here are the Struct Sizes...

```text
a64_ehci_initialize: sizeof(struct a64_qh_s)=72
a64_ehci_initialize: sizeof(struct a64_qtd_s)=32
a64_ehci_initialize: sizeof(struct ehci_itd_s)=64
a64_ehci_initialize: sizeof(struct ehci_sitd_s)=28
a64_ehci_initialize: sizeof(struct ehci_qtd_s)=32
a64_ehci_initialize: sizeof(struct ehci_overlay_s)=32
a64_ehci_initialize: sizeof(struct ehci_qh_s)=48
a64_ehci_initialize: sizeof(struct ehci_fstn_s)=8
```

We need to fix this typo in NuttX: `SIZEOF_EHCI_OVERLAY` is defined twice...

https://github.com/apache/nuttx/blob/master/include/nuttx/usb/ehci.h#L955-L974

# Halt Timeout for USB Controller

The NuttX USB EHCI Driver fails with a timeout when booting on PinePhone...

```text
a64_usbhost_initialize: TODO: a64_clockall_usboh3
a64_usbhost_initialize: TODO: switch off USB bus power
a64_usbhost_initialize: TODO: Setup pins, with power initially off
usbhost_registerclass: Registering class:0x40124838 nids:2
EHCI Initializing EHCI Stack
a64_ehci_initialize: TODO: a64_clockall_usboh3
a64_ehci_initialize: TODO: Reset the controller from the OTG peripheral
a64_ehci_initialize: TODO: Program the controller to be the USB host controller
a64_printreg: 01c1b010<-00000000
a64_printreg: 01c1b014->00000000
EHCI ERROR: Timed out waiting for HCHalted. USBSTS: 000000
EHCI ERROR: a64_reset failed: 110
a64_usbhost_initialize: ERROR: a64_ehci_initialize failed
```

[(Source)](https://github.com/lupyuen/pinephone-nuttx-usb/blob/b921aa5259ef94ece41610ebf806ebd0fa19dee5/README.md#output-log)

The timeout happens while waiting for the USB Controller to Halt...

https://github.com/lupyuen/pinephone-nuttx-usb/blob/2e1f9ab090b14f88afb8c3a36ec40a0dbbb23d49/a64_ehci.c#L4831-L4917

_What are 01c1 b010 and 01c1 b014?_

`01c1` `b000` is the Base Address of the USB EHCI Controller on Allwinner A64. [(See this)](https://lupyuen.github.io/articles/usb2#appendix-enhanced-host-controller-interface-for-usb)

`01c1` `b010` is the USB Command Register USBCMD. [(Page 18)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

`01c1` `b014` is the USB Status Register USBSTS. [(Page 21)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

```text
a64_printreg: 01c1b010<-00000000
a64_printreg: 01c1b014->00000000
```

[(Source)](https://github.com/lupyuen/pinephone-nuttx-usb/blob/b921aa5259ef94ece41610ebf806ebd0fa19dee5/README.md#output-log)

According the log, the driver wrote Command 0 (Stop) to USB Command Register USBCMD. Which will Halt the USB Controller.

Then we read USB Status Register USBSTS. This returns 0, which means that the USB Controller has NOT been halted. (HCHalted = 0)

That's why the USB Driver failed: It couldn't Halt the USB Controller at startup.

_Why?_

Probably because we haven't powered on the USB Controller? According to the log...

```text
a64_usbhost_initialize: TODO: a64_clockall_usboh3
a64_usbhost_initialize: TODO: switch off USB bus power
a64_usbhost_initialize: TODO: Setup pins, with power initially off
a64_ehci_initialize: TODO: a64_clockall_usboh3
a64_ehci_initialize: TODO: Reset the controller from the OTG peripheral
a64_ehci_initialize: TODO: Program the controller to be the USB host controller
```

[(Source)](https://github.com/lupyuen/pinephone-nuttx-usb/blob/b921aa5259ef94ece41610ebf806ebd0fa19dee5/README.md#output-log)

And maybe we need to init the USB PHY (Physical Layer)?

_How do we power on the USB Controller?_

Let's check the U-Boot source code...

# PinePhone USB Drivers in U-Boot Bootloader

Let's find the PinePhone USB Driver in the U-Boot Bootloader, to understand how it powers on the USB Controller.

When we search for PinePhone in the Source Code of the [U-Boot Bootloader](https://github.com/u-boot/u-boot), we find this Build Configuration: [pinephone_defconfig](https://github.com/u-boot/u-boot/blob/master/configs/pinephone_defconfig#L3)

```text
CONFIG_DEFAULT_DEVICE_TREE="sun50i-a64-pinephone-1.2"
```

Which refers to this PinePhone Device Tree: [sun50i-a64-pinephone-1.2.dts](https://github.com/u-boot/u-boot/blob/master/arch/arm/dts/sun50i-a64-pinephone-1.2.dts#L6)

```text
#include "sun50i-a64-pinephone.dtsi"
```

Which includes another PinePhone Device Tree: [sun50i-a64-pinephone.dtsi](https://github.com/u-boot/u-boot/blob/master/arch/arm/dts/sun50i-a64-pinephone.dtsi#L153-L516)

```text
#include "sun50i-a64.dtsi"
#include "sun50i-a64-cpu-opp.dtsi"
...
&ehci0 { status = "okay"; };
&ehci1 { status = "okay"; };

&usb_otg {
  dr_mode = "peripheral";
  status = "okay";
};

&usb_power_supply { status = "okay"; };
&usbphy { status = "okay"; };
```

Which includes this Allwinner A64 Device Tree: [sun50i-a64.dtsi](https://github.com/u-boot/u-boot/blob/master/arch/arm/dts/sun50i-a64.dtsi#L575-L659)

```text
usb_otg: usb@1c19000 {
  compatible = "allwinner,sun8i-a33-musb";
  reg = <0x01c19000 0x0400>;
  clocks = <&ccu CLK_BUS_OTG>;
  resets = <&ccu RST_BUS_OTG>;
  interrupts = <GIC_SPI 71 IRQ_TYPE_LEVEL_HIGH>;
  interrupt-names = "mc";
  phys = <&usbphy 0>;
  phy-names = "usb";
  extcon = <&usbphy 0>;
  dr_mode = "otg";
  status = "disabled";
};

usbphy: phy@1c19400 {
  compatible = "allwinner,sun50i-a64-usb-phy";
  reg = 
    <0x01c19400 0x14>,
    <0x01c1a800 0x4>,
    <0x01c1b800 0x4>;
  reg-names = 
    "phy_ctrl",
    "pmu0",
    "pmu1";
  clocks = 
    <&ccu CLK_USB_PHY0>,
    <&ccu CLK_USB_PHY1>;
  clock-names = 
    "usb0_phy",
    "usb1_phy";
  resets = 
    <&ccu RST_USB_PHY0>,
    <&ccu RST_USB_PHY1>;
  reset-names = 
    "usb0_reset",
    "usb1_reset";
  status = "disabled";
  #phy-cells = <1>;
};

ehci0: usb@1c1a000 {
  compatible = "allwinner,sun50i-a64-ehci", "generic-ehci";
  reg = <0x01c1a000 0x100>;
  interrupts = <GIC_SPI 72 IRQ_TYPE_LEVEL_HIGH>;
  clocks = 
    <&ccu CLK_BUS_OHCI0>,
    <&ccu CLK_BUS_EHCI0>,
    <&ccu CLK_USB_OHCI0>;
  resets = 
    <&ccu RST_BUS_OHCI0>,
    <&ccu RST_BUS_EHCI0>;
  phys = <&usbphy 0>;
  phy-names = "usb";
  status = "disabled";
};

ehci1: usb@1c1b000 {
  compatible = "allwinner,sun50i-a64-ehci", "generic-ehci";
  reg = <0x01c1b000 0x100>;
  interrupts = <GIC_SPI 74 IRQ_TYPE_LEVEL_HIGH>;
  clocks = 
    <&ccu CLK_BUS_OHCI1>,
    <&ccu CLK_BUS_EHCI1>,
    <&ccu CLK_USB_OHCI1>;
  resets = 
    <&ccu RST_BUS_OHCI1>,
    <&ccu RST_BUS_EHCI1>;
  phys = <&usbphy 1>;
  phy-names = "usb";
  status = "disabled";
};
```

Which says that the USB Drivers are...

-   __EHCI0 and EHCI1 (Enhanced Host Controller Interface):__ "allwinner,sun50i-a64-ehci", "generic-ehci"

    [usb/host/ehci-generic.c](https://github.com/u-boot/u-boot/blob/master/drivers/usb/host/ehci-generic.c#L160)

-   __USB OTG (On-The-Go):__ "allwinner,sun8i-a33-musb"

    [usb/musb-new/sunxi.c](https://github.com/u-boot/u-boot/blob/master/drivers/usb/musb-new/sunxi.c#L527)

-   __USB PHY (Physical Layer):__ "allwinner,sun50i-a64-usb-phy"

    [phy/allwinner/phy-sun4i-usb.c](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L654)

Why so many USB drivers? Let's talk about it...

# USB Enhanced Host Controller Interface vs On-The-Go

According to the [USB Controller Block Diagram in Allwinner A64 User Manual (Page 583)](https://github.com/lupyuen/pinephone-nuttx/releases/download/doc/Allwinner_A64_User_Manual_V1.1.pdf)...

![USB Controller Block Diagram in Allwinner A64 User Manual (Page 583)](https://lupyuen.github.io/images/usb3-title.jpg)

There are two USB Ports in Allwinner A64: __USB0 and USB1__...

-   __Port USB0__ is exposed as the External USB Port on PinePhone

-   __Port USB1__ is connected to the Internal LTE Modem

| USB Port | Alternate Name | Base Address
|:--------:|------------------|-------------
| __Port USB0__ | USB-OTG-EHCI / OHCI | __`0x01C1` `A000`__ (USB_HCI0)
| __Port USB1__ | USB-EHCI0 / OHCI0   | __`0x01C1` `B000`__ (USB_HCI1)

(Port USB0 Base Address isn't documented, but it appears in the __Memory Mapping__ (Page 73) of the [__Allwinner A64 User Manual__](https://github.com/lupyuen/pinephone-nuttx/releases/download/doc/Allwinner_A64_User_Manual_V1.1.pdf))

-   Only Port USB0 supports [USB On-The-Go (OTG)](https://en.wikipedia.org/wiki/USB_On-The-Go). Which means if we connect PinePhone to a computer, it will appear as a USB Drive. (Assuming the right drivers are started)

    (That's why Port USB0 is exposed as the External USB Port on PinePhone)

-   Ports USB0 and USB1 both support [Enhanced Host Controller Interface (EHCI)](https://lupyuen.github.io/articles/usb2#appendix-enhanced-host-controller-interface-for-usb). Which will work only as a USB Host (not USB Device)

Today we'll talk only about __Port USB1__ (EHCI / Non-OTG), since it's connected to the LTE Modem.

# Power On the USB Controller

Earlier we [searched for the USB Drivers](https://github.com/lupyuen/pinephone-nuttx-usb#pinephone-usb-drivers-in-u-boot-bootloader) for PinePhone and found these...

-   __EHCI0 and EHCI1 (Enhanced Host Controller Interface):__ 

    [usb/host/ehci-generic.c](https://github.com/u-boot/u-boot/blob/master/drivers/usb/host/ehci-generic.c#L160)

-   __USB PHY (Physical Layer):__

    [phy/allwinner/phy-sun4i-usb.c](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L654)

We disregard the USB OTG Driver because we're only interested in the [EHCI Driver (Non-OTG)](https://github.com/lupyuen/pinephone-nuttx-usb#usb-enhanced-host-controller-interface-vs-on-the-go) for PinePhone.

The USB PHY Driver handles the Physical Layer (physical wires) that connect to the USB Controller.

To power on the USB Controller ourselves, let's look inside the USB PHY Driver: [sun4i_usb_phy_init](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L259-L327)

```c
static int sun4i_usb_phy_init(struct phy *phy)
{
  struct sun4i_usb_phy_data *data = dev_get_priv(phy->dev);
  struct sun4i_usb_phy_plat *usb_phy = &data->usb_phy[phy->id];
  u32 val;
  int ret;

  ret = clk_enable(&usb_phy->clocks);
  if (ret) {
    dev_err(phy->dev, "failed to enable usb_%ldphy clock\n",
      phy->id);
    return ret;
  }
```

In the code above we enable the USB Clocks. We'll explain here...

-   ["USB Controller Clocks"](https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-clocks)

Then we deassert the USB Reset...

```c
  ret = reset_deassert(&usb_phy->resets);
  if (ret) {
    dev_err(phy->dev, "failed to deassert usb_%ldreset reset\n",
      phy->id);
    return ret;
  }
```

We'll explain the USB Reset here...

-   ["USB Controller Reset"](https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-reset)

TODO: Is PMU is needed for PinePhone Port USB1? If PMU is not needed, we skip this part...

```c
  // `hci_phy_ctl_clear` is `PHY_CTL_H3_SIDDQ`, which is `1 << 1`
  // https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-configuration
  if (usb_phy->pmu && data->cfg->hci_phy_ctl_clear) {
    val = readl(usb_phy->pmu + REG_HCI_PHY_CTL);
    val &= ~data->cfg->hci_phy_ctl_clear;
    writel(val, usb_phy->pmu + REG_HCI_PHY_CTL);
  }
```

[(FYI: PinePhone Port USB0 is connected to the PMIC, according to PinePhone Schematic Page 6)](https://files.pine64.org/doc/PinePhone/PinePhone%20v1.2b%20Released%20Schematic.pdf)

PinePhone is `sun50i_a64_phy`, so we skip this part...

```c
  // Skip this part because PinePhone is `sun50i_a64_phy`
  if (data->cfg->type == sun8i_a83t_phy ||
      data->cfg->type == sun50i_h6_phy) {
    if (phy->id == 0) {
      val = readl(data->base + data->cfg->phyctl_offset);
      val |= PHY_CTL_VBUSVLDEXT;
      val &= ~PHY_CTL_SIDDQ;
      writel(val, data->base + data->cfg->phyctl_offset);
    }
```

PinePhone is `sun50i_a64_phy`, so we run this instead...

```c
  } else {
    if (usb_phy->id == 0)
      sun4i_usb_phy_write(phy, PHY_RES45_CAL_EN,
              PHY_RES45_CAL_DATA,
              PHY_RES45_CAL_LEN);

    /* Adjust PHY's magnitude and rate */
    sun4i_usb_phy_write(phy, PHY_TX_AMPLITUDE_TUNE,
            PHY_TX_MAGNITUDE | PHY_TX_RATE,
            PHY_TX_AMPLITUDE_LEN);

    /* Disconnect threshold adjustment */
    sun4i_usb_phy_write(phy, PHY_DISCON_TH_SEL,
            data->cfg->disc_thresh, PHY_DISCON_TH_LEN);
  }
```

Which will...

-   Set PHY_RES45_CAL (TODO: What's this?)

-   Set USB PHY Magnitude and Rate

-   Disconnect USB PHY Threshold Adjustment

As explained here...

-   ["Set USB Magnitude / Rate / Threshold"](https://github.com/lupyuen/pinephone-nuttx-usb#set-usb-magnitude--rate--threshold)

TODO: Is `usb_phy->id` set to 1 for USB Port 1?

Assume `CONFIG_USB_MUSB_SUNXI` is undefined. So we skip this part...

```c
#ifdef CONFIG_USB_MUSB_SUNXI
  // Skip this part because `CONFIG_USB_MUSB_SUNXI` is undefined
  /* Needed for HCI and conflicts with MUSB, keep PHY0 on MUSB */
  if (usb_phy->id != 0)
    sun4i_usb_phy_passby(phy, true);

  /* Route PHY0 to MUSB to allow USB gadget */
  if (data->cfg->phy0_dual_route)
    sun4i_usb_phy0_reroute(data, true);
```

`CONFIG_USB_MUSB_SUNXI` is undefined, so we run this instead...

```c
#else
  sun4i_usb_phy_passby(phy, true);

  /* Route PHY0 to HCI to allow USB host */
  if (data->cfg->phy0_dual_route)
    sun4i_usb_phy0_reroute(data, false);
#endif

  return 0;
}
```

Which will...

-   Enable USB PHY Bypass

-   Route USB PHY0 to EHCI (instead of Mentor Graphics OTG MUSB)

    [(`phy0_dual_route` is true for PinePhone)](https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-configuration)

`sun4i_usb_phy_passby` and `sun4i_usb_phy0_reroute` are defined here...

-   [sun4i_usb_phy_passby](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L190-L215)

-   [sun4i_usb_phy0_reroute](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L244-L257)

_What's `CONFIG_USB_MUSB_SUNXI`?_

`CONFIG_USB_MUSB_SUNXI` enables support for the Mentor Graphics OTG / DRC USB Controller...

```text
config USB_MUSB_SUNXI
  bool "Enable sunxi OTG / DRC USB controller"
  depends on ARCH_SUNXI
  select USB_MUSB_PIO_ONLY
  default y
  ---help---
  Say y here to enable support for the sunxi OTG / DRC USB controller
  used on almost all sunxi boards.
```

[(Source)](https://github.com/u-boot/u-boot/blob/master/drivers/usb/musb-new/Kconfig#L68-L75)

We assume `CONFIG_USB_MUSB_SUNXI` is disabled because we won't be using USB OTG for NuttX (yet).

# USB Controller Clocks

Earlier we looked at the Source Code for the [USB PHY Driver for PinePhone](https://github.com/lupyuen/pinephone-nuttx-usb#power-on-the-usb-controller)...

-   ["Power On the USB Controller"](https://github.com/lupyuen/pinephone-nuttx-usb#power-on-the-usb-controller)

And we saw this code that will enable the USB Clocks: [sun4i_usb_phy_init](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L266-L271)

```c
  ret = clk_enable(&usb_phy->clocks);
```

`clk_enable` is explained here...

-   ["Enable USB Controller Clocks"](https://github.com/lupyuen/pinephone-nuttx-usb#enable-usb-controller-clocks)

_What's `usb_phy->clocks`?_

According to the [PinePhone Device Tree](https://github.com/lupyuen/pinephone-nuttx-usb#pinephone-usb-drivers-in-u-boot-bootloader), the USB Clocks are...

-   __usb0_phy:__ CLK_USB_PHY0

-   __usb1_phy:__ CLK_USB_PHY1

-   __EHCI0:__ CLK_BUS_OHCI0, CLK_BUS_EHCI0, CLK_USB_OHCI0

-   __EHCI1:__ CLK_BUS_OHCI1, CLK_BUS_EHCI1, CLK_USB_OHCI1

_What are the values of the above USB Clocks?_

The USB Clocks are defined in [clock/sun50i-a64-ccu.h](https://github.com/u-boot/u-boot/blob/master/include/dt-bindings/clock/sun50i-a64-ccu.h)...

```c
#define CLK_BUS_EHCI0		42
#define CLK_BUS_EHCI1		43
#define CLK_BUS_OHCI0		44
#define CLK_BUS_OHCI1		45
#define CLK_USB_PHY0		86
#define CLK_USB_PHY1		87
#define CLK_USB_OHCI0		91
#define CLK_USB_OHCI1		93
```

Which are consistent with the values in the PinePhone JumpDrive Device Tree: [sun50i-a64-pinephone-1.2.dts](https://github.com/lupyuen/pinephone-nuttx/blob/main/sun50i-a64-pinephone-1.2.dts#L661-L721)

The Allwinner A64 Register Addresses for USB Clocks are defined here...

-   ["Enable USB Controller Clocks"](https://github.com/lupyuen/pinephone-nuttx-usb#enable-usb-controller-clocks)

Here's the definition of USB Clocks in our U-Boot Device Tree: [sun50i-a64.dtsi](https://github.com/u-boot/u-boot/blob/master/arch/arm/dts/sun50i-a64.dtsi#L575-L659)

```text
usbphy: phy@1c19400 {
  reg = 
    <0x01c19400 0x14>,
    <0x01c1a800 0x4>,
    <0x01c1b800 0x4>;
  reg-names = 
    "phy_ctrl",
    "pmu0",
    "pmu1";
  clocks = 
    <&ccu CLK_USB_PHY0>,
    <&ccu CLK_USB_PHY1>;
  clock-names = 
    "usb0_phy",
    "usb1_phy";
    ...

ehci0: usb@1c1a000 {
  reg = <0x01c1a000 0x100>;
  clocks = 
    <&ccu CLK_BUS_OHCI0>,
    <&ccu CLK_BUS_EHCI0>,
    <&ccu CLK_USB_OHCI0>;
    ...

ehci1: usb@1c1b000 {
  reg = <0x01c1b000 0x100>;
  clocks = 
    <&ccu CLK_BUS_OHCI1>,
    <&ccu CLK_BUS_EHCI1>,
    <&ccu CLK_USB_OHCI1>;
  resets = 
    <&ccu RST_BUS_OHCI1>,
    <&ccu RST_BUS_EHCI1>;
```

(CCU means Clock Control Unit)

_What are the USB PHY Reg Values from above?_

```text
usbphy: phy@1c19400 {
  reg = 
    <0x01c19400 0x14>,
    <0x01c1a800 0x4>,
    <0x01c1b800 0x4>;
  reg-names = 
    "phy_ctrl",
    "pmu0",
    "pmu1";
```

According to the Allwinner A64 User Manual (Memory Mapping, Page 73)...

-   __phy_ctrl:__ `0x01c1` `9400` (Offset `0x14`)

    Belongs to USB-OTG-Device (USB Port 0)

-   __pmu0:__ `0x01c1` `a800` (Offset `0x4`)

    Belongs to USB-OTG-EHCI (USB Port 0)

-   __pmu1:__ `0x01c1` `b800` (Offset `0x4`)

    Belongs to USB-EHCI0 (USB Port 1)

# USB Controller Reset

Earlier we looked at the Source Code for the [USB PHY Driver for PinePhone](https://github.com/lupyuen/pinephone-nuttx-usb#power-on-the-usb-controller)...

-   ["Power On the USB Controller"](https://github.com/lupyuen/pinephone-nuttx-usb#power-on-the-usb-controller)

And we saw this code that will deassert the USB Reset: [sun4i_usb_phy_init](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L273-L278)

```c
  ret = reset_deassert(&usb_phy->resets);
```

`reset_deassert` is explained here...

-   ["Reset USB Controller"](https://github.com/lupyuen/pinephone-nuttx-usb#reset-usb-controller)

_What's `usb_phy->resets`?_

According to the [PinePhone Device Tree](https://github.com/lupyuen/pinephone-nuttx-usb#pinephone-usb-drivers-in-u-boot-bootloader), the USB Resets are...

-   __usb0_reset:__ RST_USB_PHY0

-   __usb1_reset:__ RST_USB_PHY1

-   __EHCI0:__ RST_BUS_OHCI0, RST_BUS_EHCI0

-   __EHCI1:__ RST_BUS_OHCI1, RST_BUS_EHCI1

_What are the values of the USB Resets?_

The USB Resets are defined in [reset/sun50i-a64-ccu.h](https://github.com/u-boot/u-boot/blob/master/include/dt-bindings/reset/sun50i-a64-ccu.h)...

```c
#define RST_USB_PHY0		0
#define RST_USB_PHY1		1
#define RST_BUS_EHCI0		19
#define RST_BUS_EHCI1		20
#define RST_BUS_OHCI0		21
#define RST_BUS_OHCI1		22
```

Which are consistent with the values in the PinePhone JumpDrive Device Tree: [sun50i-a64-pinephone-1.2.dts](https://github.com/lupyuen/pinephone-nuttx/blob/main/sun50i-a64-pinephone-1.2.dts#L661-L721)

The Allwinner A64 Register Addresses for USB Resets are defined here...

-   ["Reset USB Controller"](https://github.com/lupyuen/pinephone-nuttx-usb#reset-usb-controller)

Here's the definition of USB Resets in our U-Boot Device Tree: [sun50i-a64.dtsi](https://github.com/u-boot/u-boot/blob/master/arch/arm/dts/sun50i-a64.dtsi#L575-L659)

```text
usbphy: phy@1c19400 {
  resets = 
    <&ccu RST_USB_PHY0>,
    <&ccu RST_USB_PHY1>;
  reset-names = 
    "usb0_reset",
    "usb1_reset";
    ...

ehci0: usb@1c1a000 {
  resets = 
    <&ccu RST_BUS_OHCI0>,
    <&ccu RST_BUS_EHCI0>;
    ...

ehci1: usb@1c1b000 {
  resets = 
    <&ccu RST_BUS_OHCI1>,
    <&ccu RST_BUS_EHCI1>;
```

# Enable USB Controller Clocks

Earlier we saw this code that will enable the USB Clocks: [sun4i_usb_phy_init](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L266-L271)

```c
  ret = clk_enable(&usb_phy->clocks);
```

[(USB Clocks `usb_phy->clocks` are defined here)](https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-clocks)

[`clk_enable`](https://github.com/u-boot/u-boot/blob/master/drivers/clk/sunxi/clk_sunxi.c#L58-L61) calls [`sunxi_set_gate`](https://github.com/u-boot/u-boot/blob/master/drivers/clk/sunxi/clk_sunxi.c#L30-L56)

_Which A64 Registers will our NuttX USB Driver set?_

Our NuttX USB Driver will set the CCU Registers, defined in Allwinner A64 User Manual, Page 81.

(CCU Base Address is `0x01C2` `0000`)

Based on the [USB Clocks `usb_phy->clocks`](https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-clocks), our NuttX USB Driver will set the following CCU Registers: [clk_a64.c](https://github.com/u-boot/u-boot/blob/master/drivers/clk/sunxi/clk_a64.c#L16-L66)

```c
static const struct ccu_clk_gate a64_gates[] = {
  [CLK_BUS_EHCI0]		= GATE(0x060, BIT(24)),
  [CLK_BUS_EHCI1]		= GATE(0x060, BIT(25)),
  [CLK_BUS_OHCI0]		= GATE(0x060, BIT(28)),
  [CLK_BUS_OHCI1]		= GATE(0x060, BIT(29)),
  [CLK_USB_PHY0]		= GATE(0x0cc, BIT(8)),
  [CLK_USB_PHY1]		= GATE(0x0cc, BIT(9)),
  [CLK_USB_OHCI0]		= GATE(0x0cc, BIT(16)),
  [CLK_USB_OHCI1]		= GATE(0x0cc, BIT(17)),
```

So to enable the USB Clock CLK_BUS_EHCI0, we'll set Bit 24 of the CCU Register at `0x060` + `0x01C2` `0000`.

This will be similar to setting SCLK_GATING of DE_CLK_REG as described here...

-   ["Initialising the Allwinner A64 Display Engine"](https://lupyuen.github.io/articles/de#appendix-initialising-the-allwinner-a64-display-engine)

TODO: What about OHCI1_12M_SRC_SEL, OHCI0_12M_SRC_SEL? (Allwinner A64 User Manual Page 113)

# Reset USB Controller

Earlier we saw this code that will deassert the USB Reset: [sun4i_usb_phy_init](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L273-L278)

```c
  ret = reset_deassert(&usb_phy->resets);
```

[(USB Resets `usb_phy->resets` are defined here)](https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-reset)

[`reset_deassert`](https://github.com/u-boot/u-boot/blob/master/drivers/reset/reset-uclass.c#L207-L214) calls...

-   [`rst_deassert`](https://github.com/u-boot/u-boot/blob/master/drivers/reset/reset-sunxi.c#L71-L75), which calls...

-   [`sunxi_reset_deassert`](https://github.com/u-boot/u-boot/blob/master/drivers/reset/reset-sunxi.c#L66-L69), which calls...

-   [`sunxi_set_reset`](https://github.com/u-boot/u-boot/blob/master/drivers/reset/reset-sunxi.c#L36-L59)

_Which A64 Registers will our NuttX USB Driver set?_

Our NuttX USB Driver will set the CCU Registers, defined in Allwinner A64 User Manual, Page 81.

(CCU Base Address is `0x01C2` `0000`)

Based on the [USB Resets `usb_phy->resets`](https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-reset), our NuttX USB Driver will set the following CCU Registers: [clk_a64.c](https://github.com/u-boot/u-boot/blob/master/drivers/clk/sunxi/clk_a64.c#L68-L100)

```c
static const struct ccu_reset a64_resets[] = {
  [RST_USB_PHY0]          = RESET(0x0cc, BIT(0)),
  [RST_USB_PHY1]          = RESET(0x0cc, BIT(1)),
  [RST_BUS_EHCI0]         = RESET(0x2c0, BIT(24)),
  [RST_BUS_EHCI1]         = RESET(0x2c0, BIT(25)),
  [RST_BUS_OHCI0]         = RESET(0x2c0, BIT(28)),
  [RST_BUS_OHCI1]         = RESET(0x2c0, BIT(29)),
```

So to deassert the USB Reset RST_USB_PHY0, we'll set Bit 0 of the CCU Register at `0x0cc` + `0x01C2` `0000`.

This will be similar to setting DE_RST of BUS_SOFT_RST_REG1 as described here...

-   ["Initialising the Allwinner A64 Display Engine"](https://lupyuen.github.io/articles/de#appendix-initialising-the-allwinner-a64-display-engine)

# NuttX EHCI Driver Starts OK on PinePhone

Earlier the NuttX USB EHCI Driver fails during PinePhone startup...

-   ["Halt Timeout for USB Controller"](https://github.com/lupyuen/pinephone-nuttx-usb#halt-timeout-for-usb-controller)

Then we discovered how the U-Boot Bootloader enables the USB Clocks and deasserts the USB Resets...

-   ["USB Controller Clocks"](https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-clocks)

-   ["Enable USB Controller Clocks"](https://github.com/lupyuen/pinephone-nuttx-usb#enable-usb-controller-clocks)

-   ["USB Controller Reset"](https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-reset)

-   ["Reset USB Controller"](https://github.com/lupyuen/pinephone-nuttx-usb#reset-usb-controller)

So we do the same for NuttX. And now the NuttX EHCI Driver starts OK on PinePhone yay! 🎉

This is how we enable the USB Clocks and deassert the USB Resets on PinePhone...

https://github.com/lupyuen/pinephone-nuttx-usb/blob/0e1632ed351975a6432b7e4fde1857d6bcc0940a/a64_usbhost.c#L118-L279

Here's the log...

```text
a64_usbhost_clk_enable: CLK_USB_PHY0
set_bit: 0x1c200cc Bit 8
a64_usbhost_clk_enable: CLK_USB_PHY1
set_bit: 0x1c200cc Bit 9
a64_usbhost_clk_enable: CLK_BUS_OHCI0
set_bit: 0x1c20060 Bit 28
a64_usbhost_clk_enable: CLK_BUS_EHCI0
set_bit: 0x1c20060 Bit 24
a64_usbhost_clk_enable: CLK_USB_OHCI0
set_bit: 0x1c200cc Bit 16
a64_usbhost_clk_enable: CLK_BUS_OHCI1
set_bit: 0x1c20060 Bit 29
a64_usbhost_clk_enable: CLK_BUS_EHCI1
set_bit: 0x1c20060 Bit 25
a64_usbhost_clk_enable: CLK_USB_OHCI1
set_bit: 0x1c200cc Bit 17
a64_usbhost_reset_deassert: RST_USB_PHY0
set_bit: 0x1c200cc Bit 0
a64_usbhost_reset_deassert: RST_USB_PHY1
set_bit: 0x1c200cc Bit 1
a64_usbhost_reset_deassert: RST_BUS_OHCI0
set_bit: 0x1c202c0 Bit 28
a64_usbhost_reset_deassert: RST_BUS_EHCI0
set_bit: 0x1c202c0 Bit 24
a64_usbhost_reset_deassert: RST_BUS_OHCI1
set_bit: 0x1c202c0 Bit 29
a64_usbhost_reset_deassert: RST_BUS_EHCI1
set_bit: 0x1c202c0 Bit 25
```

[(Source)](https://github.com/lupyuen/pinephone-nuttx-usb/blob/5238bc5246bcae896883f056d24691ebaa050f83/README.md#output-log)

This log above shows NuttX enabling the USB Clocks and deasserting the USB Resets for...

-   USB PHY0 and USB PHY1

-   EHCI0 and OHCI0

-   EHCI1 and OHCI1

(Yeah this looks excessive. We probably need only USB PHY1, EHCI1 and OHCI1)

Then the NuttX EHCI Driver starts...

```text
a64_usbhost_initialize: TODO: a64_clockall_usboh3
a64_usbhost_initialize: TODO: switch off USB bus power
a64_usbhost_initialize: TODO: Setup pins, with power initially off
usbhost_registerclass: Registering class:0x40124838 nids:2
a64_ehci_initialize: sizeof(struct a64_qh_s)=96
a64_ehci_initialize: sizeof(struct a64_qtd_s)=32
a64_ehci_initialize: sizeof(struct ehci_itd_s)=64
a64_ehci_initialize: sizeof(struct ehci_sitd_s)=28
a64_ehci_initialize: sizeof(struct ehci_qtd_s)=32
a64_ehci_initialize: sizeof(struct ehci_overlay_s)=32
a64_ehci_initialize: sizeof(struct ehci_qh_s)=48
a64_ehci_initialize: sizeof(struct ehci_fstn_s)=8
EHCI Initializing EHCI Stack
a64_ehci_initialize: TODO: a64_clockall_usboh3
a64_ehci_initialize: TODO: Reset the controller from the OTG peripheral
a64_ehci_initialize: TODO: Program the controller to be the USB host controller
a64_printreg: 01c1b010<-00000000
a64_printreg: 01c1b014->00001000
a64_printreg: 01c1b010->00000000
a64_printreg: 01c1b010<-00000002
a64_printreg: 01c1b010->00080b00
a64_ehci_initialize: TODO: Re-program the USB host controller
a64_printreg: 01c1b018<-00000000
a64_printreg: 01c1b014<-0000003f
EHCI HCIVERSION 1.00
a64_printreg: 01c1b004->00001101
EHCI nports=1, HCSPARAMS=1101
a64_printreg: 01c1b008->0000a026
EHCI HCCPARAMS=00a026
a64_printreg: 01c1b028<-40a87fa0
a64_printreg: 01c1b024<-40a95000
a64_printreg: 01c1b010->00080b00
a64_printreg: 01c1b010<-00080b30
a64_printreg: 01c1b010->00080b30
a64_printreg: 01c1b010<-00080b31
a64_printreg: 01c1b050->00000000
a64_printreg: 01c1b050<-00000001
a64_printreg: 01c1b014->00000000
a64_ehci_initialize: TODO: irq_attach
a64_printreg: 01c1b018<-00000037
a64_ehci_initialize: TODO: up_enable_irq
a64_ehci_initialize: TODO: a64_usbhost_vbusdrive
a64_printreg: 01c1b054->00001000
EHCI USB EHCI Initialized
NuttShell (NSH) NuttX-12.0.3
nsh> 
```

[(Source)](https://github.com/lupyuen/pinephone-nuttx-usb/blob/5238bc5246bcae896883f056d24691ebaa050f83/README.md#output-log)

The log above says that NuttX has successfully started the EHCI Controller. Yay!

```text
EHCI HCIVERSION 1.00
EHCI nports=1, HCSPARAMS=1101
EHCI HCCPARAMS=00a026
EHCI USB EHCI Initialized
```

# Decode EHCI Register Values

_In the NuttX Log above, what are the values of the EHCI Registers?_

Let's decode the values of the USB EHCI Registers, to make sure that PinePhone is returning the right values on NuttX...

1.  EHCI Log begins with...

    ```text
    EHCI Initializing EHCI Stack
    a64_ehci_initialize: TODO: a64_clockall_usboh3
    a64_ehci_initialize: TODO: Reset the controller from the OTG peripheral
    a64_ehci_initialize: TODO: Program the controller to be the USB host controller
    ```

    [(Source)](https://github.com/lupyuen/pinephone-nuttx-usb/blob/73e2c24be48a2eb9e0a9c478e0b2a121e6ac8e6a/README.md#output-log)

    Then we see the EHCI Registers and their values...

1.  Write USBCMD: USB Command Register [(EHCI Page 18)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    (To halt the USB Controller)

    Run / Stop (RS, Bit 0) = 0
    - Halt the Host Controller

    ```text
    a64_printreg: 01c1b010<-00000000
    ```

1.  Read USBSTS: USB Status Register [(EHCI Page 21)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    (To verify that USB Controller has been halted)

    HCHalted (Bit 12) = 1
    - Host Controller has halted

    ```text
    a64_printreg: 01c1b014->00001000
    ```

1.  Read USBCMD: USB Command Register [(EHCI Page 18)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    (To verify that USB Controller has been halted)

    Run / Stop (RS, Bit 0) = 0
    - Host Controller has halted

    ```text
    a64_printreg: 01c1b010->00000000
    ```

1.  Write USBCMD: USB Command Register (EHCI Page 18)

    (To reset the USB Controller)

    Host Controller Reset (HCRESET, Bit 1) = 1
    - Reset the Host Controller

    ```text
    a64_printreg: 01c1b010<-00000002
    ```

1.  Read USBCMD: USB Command Register [(EHCI Page 18)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    Asynchronous Schedule Park Mode Count (Bits 8-9) = 3 (default)

    Asynchronous Schedule Park Mode Enable (Bit 11) = 1 (default)

    Interrupt Threshold Control (Bits 16-23) = 8 (default)
    - 8 micro-frames / 1 ms

    ```text
    a64_printreg: 01c1b010->00080b00
    a64_ehci_initialize: TODO: Re-program the USB host controller
    ```

1.  Write USBINTR: USB Interrupt Enable Register [(EHCI Page 22)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    Disable all interrupts

    ```text
    a64_printreg: 01c1b018<-00000000
    ```

1.  Write USBSTS: USB Status Register [(EHCI Page 21)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    USB Interrupt (USBINT, Bit 0) = 1 (Reset)

    USB Error Interrupt (USBERRINT, Bit 1) = 1 (Reset)

    Port Change Detect (Bit 2) = 1 (Reset)

    Frame List Rollover (Bit 3) = 1 (Reset)

    Host System Error (Bit 4) = 1 (Reset)

    Interrupt on Async Advance (Bit 5) = 1 (Reset)

    ```text
    a64_printreg: 01c1b014<-0000003f
    EHCI HCIVERSION 1.00
    ```

1.  Read HCSPARAMS: Structural Parameters [(EHCI Page 14)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    N_PORTS (Bits 0 to 3) = 1 (Number of physical downstream ports)

    Number of Ports per Companion Controller (N_PCC, Bits 8 to 11) = 1
    - Number of ports supported per companion host controller
    
    Number of Companion Controller (N_CC, Bits 12 to 15) = 1
    - Number of companion controllers

    ```text
    a64_printreg: 01c1b004->00001101
    EHCI nports=1, HCSPARAMS=1101
    ```

1.  Read HCCPARAMS: Capability Parameters [(EHCI Page 15)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    64-bit Addressing Capability (Bit 0) = 0
    - 32-bit address memory pointers only

    Programmable Frame List Flag (Bit 1) = 1
    - System software can specify and use a smaller frame list

    Asynchronous Schedule Part Capability (Bit 2) = 1
    - Support park feature for high-speed queue heads in the Asynchronous Schedule

    Isochronous Scheduling Threshold (Bits 4 to 7) = 2

    EHCI Extended Capabilities Pointer (EECP, Bits 8 to 15) = 0xA0

    ```text
    a64_printreg: 01c1b008->0000a026
    EHCI HCCPARAMS=00a026
    ```

1.  Write ASYNCLISTADDR: Current Asynchronous List Address Register [(EHCI Page 25)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    Address of the next asynchronous queue head to be executed

    ```text
    a64_printreg: 01c1b028<-40a86fa0
    ```

1.  Write PERIODICLISTBASE: Periodic Frame List Base Address Register [(EHCI Page 24)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    Beginning address of the Periodic Frame List in the system memory

    ```text
    a64_printreg: 01c1b024<-40a97000
    ```

1.  Read USBCMD: Same as above

    ```text
    a64_printreg: 01c1b010->00080b00
    ```

1.  Write USBCMD: USB Command Register [(EHCI Page 18)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    Periodic Schedule Enable (Bit 4) = 1
    - Use the PERIODICLISTBASE register to access the Periodic Schedule

    Asynchronous Schedule Enable (Bit 5) = 1
    - Use the ASYNCLISTADDR register to access the Asynchronous Schedule

    Asynchronous Schedule Park Mode Count (Bits 8-9) = 3 (default)
    
    Asynchronous Schedule Park Mode Enable (Bit 11) = 1 (default)

    Interrupt Threshold Control (Bits 16-23) = 8 (default)
    - 8 micro-frames / 1 ms

    ```text
    a64_printreg: 01c1b010<-00080b30
    ```

1.  Read USBCMD: Same as above

    ```text
    a64_printreg: 01c1b010->00080b30
    ```

1.  Write USBCMD: Same as above, plus...

    (To start the USB Controller)

    Run / Stop (RS, Bit 0) = 1
    - Start Host Controller

    ```text
    a64_printreg: 01c1b010<-00080b31
    ```

1.  Read CONFIGFLAG: Configure Flag Register [(EHCI Page 25)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    Configure Flag (CF, Bit 0) = 0
    - Route each port to an implementation dependent classic host controller

    ```text
    a64_printreg: 01c1b050->00000000
    ```

1.  Write CONFIGFLAG: Configure Flag Register [(EHCI Page 25)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    Configure Flag (CF, Bit 0) = 1
    - Route all ports to this host controller

    ```text
    a64_printreg: 01c1b050<-00000001
    ```

1.  Read USBSTS: USB Status Register [(EHCI Page 21)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    (To verify that USB Controller has been started)

    HCHalted (Bit 12) = 0
    - Host Controller has started

    ```text
    a64_printreg: 01c1b014->00000000
    a64_ehci_initialize: TODO: irq_attach
    ```

1.  Write USBINTR: USB Interrupt Enable Register

    (To enable the USB Interrupts)

    USB Interrupt Enable (Bit 0) = 1

    USB Error Interrupt Enable (Bit 1) = 1

    Port Change Interrupt Enable (Bit 2) = 1

    Host System Error Enable (Bit 4) = 1

    Interrupt on Async Advance Enable (Bit 5) = 1

    ```text
    a64_printreg: 01c1b018<-00000037
    a64_ehci_initialize: TODO: up_enable_irq
    a64_ehci_initialize: TODO: a64_usbhost_vbusdrive
    ```

1.  Read PORTSC: Port Status and Control Register [(EHCI Page 26)](https://www.intel.sg/content/www/xa/en/products/docs/io/universal-serial-bus/ehci-specification-for-usb.html)

    Port Power (PP, Bit 12) = 1
    - Host controller has port power control switches

    ```text
    a64_printreg: 01c1b054->00001000
    EHCI USB EHCI Initialized
    ```

The above values look OK for PinePhone.

# "ls" crashes when USB Hub Support is enabled

When we enable USB Hub Support and run `ls`, NuttX halts with an Assertion Failure...

```text
NuttShell (NSH) NuttX-12.0.3
nsh> ls /dev
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: mm_heap/mm_malloc.c:200 task: nsh_main 0x4008b380
up_dump_register: stack = 0x4012e660
up_dump_register: x0:   0x4012e660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f3b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x40a8e218          x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008ecc0          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a95608          x21:  0x400f392d
up_dump_register: x22:  0x0                 x23:  0xc8
up_dump_register: x24:  0x400f1277          x25:  0x40125000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b328        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a97900        
up_dump_register: SP_ELX:    0x40a97610        
up_dump_register: TPIDR_EL0: 0x40a95608        
up_dump_register: TPIDR_EL1: 0x40a95608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
```

[(Source)](https://github.com/lupyuen/pinephone-nuttx-usb/blob/0ba3be1b69937f3e31ea57c13c21645550859f44/README.md#output-log)

Here's the Assertion Failure: [mm/mm_heap/mm_malloc.c](https://github.com/apache/nuttx/blob/master/mm/mm_heap/mm_malloc.c#L200-L202)

```c
FAR void *mm_malloc(FAR struct mm_heap_s *heap, size_t size) {
  ...
  DEBUGASSERT((next->size & MM_ALLOC_BIT) != 0 &&
              (next->size & MM_PREVFREE_BIT) != 0 &&
              next->preceding == nodesize);
```

TODO: What does this mean?

TODO: Enable Memory Allocation Log

TODO: Lookup the addresses in the Register Dump

# Enumerate USB Devices on PinePhone

TODO: Can we enumerate the LTE Modem on PinePhone?

_How does NuttX enumerate USB Devices?_

[a64_usbhost_initialize](https://github.com/lupyuen/pinephone-nuttx-usb/blob/9eb27ca0cc7b1087e0a6b49316bf6ce568337dbd/a64_usbhost.c#L330-L341) creates a thread for...

-   [ehci_waiter](https://github.com/lupyuen/pinephone-nuttx-usb/blob/main/a64_usbhost.c#L82-L118), which calls....

-   [a64_wait](https://github.com/lupyuen/pinephone-nuttx-usb/blob/main/a64_ehci.c#L3474-L3573) to wait for a USB Device to be connected...

    But it blocks on a Semaphore `pscsem`, waiting for a change in the Connection State of a Root Hub Port.

    `pcsem` is signalled by [a64_connect](https://github.com/lupyuen/pinephone-nuttx-usb/blob/main/a64_ehci.c#L4766-L4815)

If [a64_wait](https://github.com/lupyuen/pinephone-nuttx-usb/blob/main/a64_ehci.c#L3474-L3573) is successful, [ehci_waiter](https://github.com/lupyuen/pinephone-nuttx-usb/blob/main/a64_usbhost.c#L82-L118) will call [a64_enumerate](https://github.com/lupyuen/pinephone-nuttx-usb/blob/main/a64_ehci.c#L3820-L3861)

[a64_enumerate](https://github.com/lupyuen/pinephone-nuttx-usb/blob/main/a64_ehci.c#L3820-L3861) 
calls...

-   [usbhost_enumerate](https://github.com/lupyuen2/wip-pinephone-nuttx/blob/usb/drivers/usbhost/usbhost_enumerate.c#L250-L581) 

-   [usbhost_devdesc](https://github.com/lupyuen2/wip-pinephone-nuttx/blob/usb/drivers/usbhost/usbhost_enumerate.c#L91-L121) will print the USB Descriptor

TODO: Handle USB interrupt

[a64_ehci_interrupt](https://github.com/lupyuen/pinephone-nuttx-usb/blob/main/a64_ehci.c#L3415-L3472)

USB IRQ:

-   USB-OTG: 103
-   USB-OTG-EHCI: 104
-   USB-OTG-OHCI: 105
-   USB-EHCI0: 106
-   USB-OHCI0: 107

We will only handle USB-EHCI0 (IRQ 106)

# Testing CDC ACM

We're now testing USB CDC ACM on PinePhone, to see if NuttX can enumerate USB Devices correctly.

Select these options in `menuconfig`...

-   Enable "RTOS Features > Work queue support > Low priority (kernel) worker thread"

    Set "Number of low-priority worker threads" to 2

-   Enable "Device Drivers > USB Host Driver Support > USB Hub Support"

    And "CDC/ACM Support"

_Does the Quectel LTE Modem run on USB CDC ACM?_

The `lsusb` and `dmesg` logs for PinePhone might be helpful later...

-   [`lsusb` log for PinePhone](https://gist.github.com/lupyuen/5410d76a7e9851b42b69c2058ce448ae)

-   [`dmesg` log for PinePhone](https://gist.github.com/lupyuen/9e6f9acdc4ecec24445d726884609b1f)

TODO: Get the USB Config File for PinePhone: /etc/udev/rules.d

_Why is "Number of low-priority worker threads" set to 2?_

Because "CDC/ACM Support" won't compile with the default value of 1.

# Set USB Magnitude / Rate / Threshold

Earlier we saw this code for setting the [USB Magnitude, Rate and Threshold](https://github.com/lupyuen/pinephone-nuttx-usb#power-on-the-usb-controller) in the USB PHY Driver: [sun4i_usb_phy_init](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L259-L327)

```c
static int sun4i_usb_phy_init(struct phy *phy) {
  ...
  // Assume ID is 1 for Port USB 1
  if (usb_phy->id == 0)
    sun4i_usb_phy_write(phy, PHY_RES45_CAL_EN,
      PHY_RES45_CAL_DATA,
      PHY_RES45_CAL_LEN);

  /* Adjust PHY's magnitude and rate */
  sun4i_usb_phy_write(phy, PHY_TX_AMPLITUDE_TUNE,
    PHY_TX_MAGNITUDE | PHY_TX_RATE,
    PHY_TX_AMPLITUDE_LEN);

  /* Disconnect threshold adjustment */
  sun4i_usb_phy_write(phy, PHY_DISCON_TH_SEL,
    data->cfg->disc_thresh, PHY_DISCON_TH_LEN);
```

[(`sun4i_usb_phy_write` is defined here)](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L145-L188)

[(`disc_thresh` is 3)](https://github.com/lupyuen/pinephone-nuttx-usb#usb-controller-configuration)

TODO: Is this needed for NuttX?

# USB Controller Configuration

Here's the PinePhone USB Controller Configuration in U-Boot Bootloader: [phy-sun4i-usb.c](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L622-L630)

```c
static const struct sun4i_usb_phy_cfg sun50i_a64_cfg = {
  .num_phys = 2,
  .type = sun50i_a64_phy,
  .disc_thresh = 3,
  .phyctl_offset = REG_PHYCTL_A33,
  .dedicated_clocks = true,
  .hci_phy_ctl_clear = PHY_CTL_H3_SIDDQ,
  .phy0_dual_route = true,
};
```

(`PHY_CTL_H3_SIDDQ` is `1 << 1`)

# TODO

TODO

-   USB PHY Power Doc: [sun4i-usb-phy.txt](https://github.com/u-boot/u-boot/blob/master/doc/device-tree-bindings/phy/sun4i-usb-phy.txt)

-   [sun4i_usb_phy_power_on](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L217-L231)

# Output Log

```text
DRAM: 2048 MiB
Trying to boot from MMC1
NOTICE:  BL31: v2.2(release):v2.2-904-gf9ea3a629
NOTICE:  BL31: Built : 15:32:12, Apr  9 2020
NOTICE:  BL31: Detected Allwinner A64/H64/R18 SoC (1689)
NOTICE:  BL31: Found U-Boot DTB at 0x4064410, model: PinePhone
NOTICE:  PSCI: System suspend is unavailable


U-Boot 2020.07 (Nov 08 2020 - 00:15:12 +0100)

DRAM:  2 GiB
MMC:   Device 'mmc@1c11000': seq 1 is in use by 'mmc@1c10000'
mmc@1c0f000: 0, mmc@1c10000: 2, mmc@1c11000: 1
Loading Environment from FAT... *** Warning - bad CRC, using default environment

starting USB...
No working controllers found
Hit any key to stop autoboot:  0 
switch to partitions #0, OK
mmc0 is current device
Scanning mmc 0:1...
Found U-Boot script /boot.scr
653 bytes read in 3 ms (211.9 KiB/s)
## Executing script at 4fc00000
gpio: pin 114 (gpio 114) value is 1
357662 bytes read in 20 ms (17.1 MiB/s)
Uncompressed size: 10534912 = 0xA0C000
36162 bytes read in 5 ms (6.9 MiB/s)
1078500 bytes read in 50 ms (20.6 MiB/s)
## Flattened Device Tree blob at 4fa00000
   Booting using the fdt blob at 0x4fa00000
   Loading Ramdisk to 49ef8000, end 49fff4e4 ... OK
   Loading Device Tree to 0000000049eec000, end 0000000049ef7d41 ... OK

Starting kernel ...

a64_usbhost_clk_enable: CLK_USB_PHY0
set_bit: 0x1c200cc Bit 8
a64_usbhost_clk_enable: CLK_USB_PHY1
set_bit: 0x1c200cc Bit 9
a64_usbhost_clk_enable: CLK_BUS_OHCI0
set_bit: 0x1c20060 Bit 28
a64_usbhost_clk_enable: CLK_BUS_EHCI0
set_bit: 0x1c20060 Bit 24
a64_usbhost_clk_enable: CLK_USB_OHCI0
set_bit: 0x1c200cc Bit 16
a64_usbhost_clk_enable: CLK_BUS_OHCI1
set_bit: 0x1c20060 Bit 29
a64_usbhost_clk_enable: CLK_BUS_EHCI1
set_bit: 0x1c20060 Bit 25
a64_usbhost_clk_enable: CLK_USB_OHCI1
set_bit: 0x1c200cc Bit 17
a64_usbhost_reset_deassert: RST_USB_PHY0
set_bit: 0x1c200cc Bit 0
a64_usbhost_reset_deassert: RST_USB_PHY1
set_bit: 0x1c200cc Bit 1
a64_usbhost_reset_deassert: RST_BUS_OHCI0
set_bit: 0x1c202c0 Bit 28
a64_usbhost_reset_deassert: RST_BUS_EHCI0
set_bit: 0x1c202c0 Bit 24
a64_usbhost_reset_deassert: RST_BUS_OHCI1
set_bit: 0x1c202c0 Bit 29
a64_usbhost_reset_deassert: RST_BUS_EHCI1
set_bit: 0x1c202c0 Bit 25
a64_usbhost_initialize: TODO: a64_clockall_usboh3
a64_usbhost_initialize: TODO: switch off USB bus power
a64_usbhost_initialize: TODO: Setup pins, with power initially off
usbhost_registerclass: Registering class:0x40124838 nids:2
a64_ehci_initialize: sizeof(struct a64_qh_s)=96
a64_ehci_initialize: sizeof(struct a64_qtd_s)=32
a64_ehci_initialize: sizeof(struct ehci_itd_s)=64
a64_ehci_initialize: sizeof(struct ehci_sitd_s)=28
a64_ehci_initialize: sizeof(struct ehci_qtd_s)=32
a64_ehci_initialize: sizeof(struct ehci_overlay_s)=32
a64_ehci_initialize: sizeof(struct ehci_qh_s)=48
a64_ehci_initialize: sizeof(struct ehci_fstn_s)=8
EHCI Initializing EHCI Stack
a64_ehci_initialize: TODO: a64_clockall_usboh3
a64_ehci_initialize: TODO: Reset the controller from the OTG peripheral
a64_ehci_initialize: TODO: Program the controller to be the USB host controller
a64_printreg: 01c1b010<-00000000
a64_printreg: 01c1b014->00001000
a64_printreg: 01c1b010->00000000
a64_printreg: 01c1b010<-00000002
a64_printreg: 01c1b010->00080b00
a64_ehci_initialize: TODO: Re-program the USB host controller
a64_printreg: 01c1b018<-00000000
a64_printreg: 01c1b014<-0000003f
EHCI HCIVERSION 1.00
a64_printreg: 01c1b004->00001101
EHCI nports=1, HCSPARAMS=1101
a64_printreg: 01c1b008->0000a026
EHCI HCCPARAMS=00a026
a64_printreg: 01c1b028<-40a87fa0
a64_printreg: 01c1b024<-40a98000
a64_printreg: 01c1b010->00080b00
a64_printreg: 01c1b010<-00080b30
a64_printreg: 01c1b010->00080b30
a64_printreg: 01c1b010<-00080b31
a64_printreg: 01c1b050->00000000
a64_printreg: 01c1b050<-00000001
a64_printreg: 01c1b014->00000000
a64_ehci_initialize: TODO: irq_attach
a64_printreg: 01c1b018<-00000037
a64_ehci_initialize: TODO: up_enable_irq
a64_ehci_initialize: TODO: a64_usbhost_vbusdrive
a64_printreg: 01c1b054->00001000
EHCI USB EHCI Initialized
a64_usbhost_initialize: 1
nshe:h cmik_fwaatiftse:r :c o!m!m!a
n
de hncoit_ wfaoiutnedr
:
 
e
hNcuit_twSahietlelr :( N SRHu)n nNiuntgt
X
-a1624._0w.a3i
t
:n s#h#># 
.
[K
nsh> 
nsh> 

nsh> ls
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: mm_heap/mm_malloc.c:200 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x40a8d218          x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc8
up_dump_register: x24:  0x400f0277          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a96610        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a96550        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a96490        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a963d0        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a96310        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a96250        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a96190        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a960d0        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a96010        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95f50        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95e90        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95dd0        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95d10        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95c50        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95b90        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95ad0        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95a10        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95950        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95890        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a957d0        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95710        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95650        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95590        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a954d0        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95410        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95350        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95290        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a951d0        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95110        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
_assert: Current Version: NuttX  12.0.3 4d922be-dirty Mar  7 2023 15:54:47 arm64
_assert: Assertion failed : at file: misc/lib_mutex.c:194 task: nsh_main 0x4008b230
up_dump_register: stack = 0x4012d660
up_dump_register: x0:   0x4012d660          x1:   0xa
up_dump_register: x2:   0x20                x3:   0x400f2b22
up_dump_register: x4:   0x4a10              x5:   0x0
up_dump_register: x6:   0x4                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x0
up_dump_register: x20:  0x40a94608          x21:  0x400f292d
up_dump_register: x22:  0x0                 x23:  0xc2
up_dump_register: x24:  0x400effd8          x25:  0x40124000
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008b1d8        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x40000005        
up_dump_register: ELR:       0x40081000        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a95050        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0xffffffffffffffff
up_dump_register: stack = 0x40a94f90
up_dump_register: x0:   0x66                x1:   0xffffffff
up_dump_register: x2:   0x0                 x3:   0x400f2b22
up_dump_register: x4:   0x4                 x5:   0xfffffffffffffffc
up_dump_register: x6:   0xf                 x7:   0x88
up_dump_register: x8:   0x888               x9:   0x0
up_dump_register: x10:  0x2                 x11:  0xa200023
up_dump_register: x12:  0x1c                x13:  0x1
up_dump_register: x14:  0x0                 x15:  0x1c28000
up_dump_register: x16:  0x4008eb70          x17:  0x1
up_dump_register: x18:  0x0                 x19:  0x40a94848
up_dump_register: x20:  0x40a948c8          x21:  0x66
up_dump_register: x22:  0x0                 x23:  0x40a968a8
up_dump_register: x24:  0x400f15a7          x25:  0x400f0957
up_dump_register: x26:  0x0                 x27:  0x0
up_dump_register: x28:  0x0                 x29:  0x0
up_dump_register: x30:  0x4008904c        
up_dump_register: 
up_dump_register: STATUS Registers:
up_dump_register: SPSR:      0x60000005        
up_dump_register: ELR:       0x4008907c        
up_dump_register: SP_EL0:    0x40a96900        
up_dump_register: SP_ELX:    0x40a94f90        
up_dump_register: TPIDR_EL0: 0x40a94608        
up_dump_register: TPIDR_EL1: 0x40a94608        
up_dump_register: EXE_DEPTH: 0x0               
```
