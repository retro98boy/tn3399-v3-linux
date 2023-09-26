下载地址：http://mirrors.ustc.edu.cn/armbian/pool/main/a/armbian-firmware-full/

- `fw_bcm43455c0_ag.bin`：WLAN固件
- `fw_bcm43455c0_ag_apsta.bin`：未知，非必需
- `fw_bcm43455c0_ag_p2p.bin`：未知，非必需
- `nvram_ap6255.txt`：WLAN NVRAM
- `BCM4345C0.hcd`：BlueZ固件

WLAN的固件和NVRAM存放位置由BSP内核配置决定，[mrfixit2001](https://github.com/mrfixit2001/rockchip-kernel)维护的版本默认到/lib/firmware/brcm寻找固件和NVRAM

BlueZ固件一般放在/lib/firmware/brcm下