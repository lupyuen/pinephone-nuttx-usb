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
```

TODO
