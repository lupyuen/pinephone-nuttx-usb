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

-   Enable "System Type > Allwinner A64 Peripheral Selection > USB EHCI"

-   Enable "RTOS Features > Work queue support > Low priority (kernel) worker thread"

-   Enable "Device Drivers > USB Host Driver Support"

-   Enable "Device Drivers > USB Host Driver Support > USB Hub Support"

TODO
