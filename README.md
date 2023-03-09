# PinePhone USB Driver for Apache NuttX RTOS

Read the article...

-   ["NuttX RTOS for PinePhone: Exploring USB"](https://lupyuen.github.io/articles/usb2)

We're porting the [NXP i.MX RT EHCI USB Driver](https://github.com/apache/nuttx/blob/master/arch/arm/src/imxrt/imxrt_ehci.c#L4970) to PinePhone and Allwinner A64...

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

We'll check the U-Boot source code...

-   [u-boot/board/sunxi/board.c](https://github.com/u-boot/u-boot/blob/master/board/sunxi/board.c#L676)

# Power On the USB Controller

TODO

Sunxi Board

-   [u-boot/board/sunxi/board.c](https://github.com/u-boot/u-boot/blob/master/board/sunxi/board.c#L676)

Generic EHCI Driver

-   [u-boot/drivers/usb/host/ehci-generic.c](https://github.com/u-boot/u-boot/blob/master/drivers/usb/host/ehci-generic.c)

USB PHY Power Doc

-   [u-boot/doc/device-tree-bindings/phy/sun4i-usb-phy.txt](https://github.com/u-boot/u-boot/blob/master/doc/device-tree-bindings/phy/sun4i-usb-phy.txt)

USB PHY Driver: [u-boot/drivers/phy/allwinner/phy-sun4i-usb.c](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L217-L231)

-   [sun4i_usb_phy_init](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L259-L327)

-   [sun4i_usb_phy_power_on](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L217-L231)

Route USB PHY to EHCI:

```c
static int sun4i_usb_phy_init(struct phy *phy) {
    ...
#ifdef CONFIG_USB_MUSB_SUNXI
	/* Needed for HCI and conflicts with MUSB, keep PHY0 on MUSB */
	if (usb_phy->id != 0)
		sun4i_usb_phy_passby(phy, true);

	/* Route PHY0 to MUSB to allow USB gadget */
	if (data->cfg->phy0_dual_route)
		sun4i_usb_phy0_reroute(data, true);
#else
	sun4i_usb_phy_passby(phy, true);

	/* Route PHY0 to HCI to allow USB host */
	if (data->cfg->phy0_dual_route)
		sun4i_usb_phy0_reroute(data, false);
#endif
```

[(Source)](https://github.com/u-boot/u-boot/blob/master/drivers/phy/allwinner/phy-sun4i-usb.c#L217-L231)

`CONFIG_USB_MUSB_SUNXI`:

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
356930 bytes read in 20 ms (17 MiB/s)
Uncompressed size: 10534912 = 0xA0C000
36162 bytes read in 5 ms (6.9 MiB/s)
1078500 bytes read in 50 ms (20.6 MiB/s)
## Flattened Device Tree blob at 4fa00000
   Booting using the fdt blob at 0x4fa00000
   Loading Ramdisk to 49ef8000, end 49fff4e4 ... OK
   Loading Device Tree to 0000000049eec000, end 0000000049ef7d41 ... OK

Starting kernel ...

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
a64_printreg: 01c1b014->00000000
EHCI ERROR: Timed out waiting for HCHalted. USBSTS: 000000
EHCI ERROR: a64_reset failed: 110
a64_usbhost_initialize: ERROR: a64_ehci_initialize failed
ERROR: Couldn't start usb -19
nsh: mkfatfs: command not found

NuttShell (NSH) NuttX-12.0.3
nsh> 
```
