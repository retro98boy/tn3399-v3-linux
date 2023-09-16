`rk3399_loader_v1.30.130.bin`是rkdeveloptool使用的loader

# 生成步骤

下载Rockchip提供的blob：

```
git clone https://github.com/rockchip-linux/rkbin.git
cd rkbin
```

由于TN3399_V3使用的[K4B8G16](https://semiconductor.samsung.com/dram/ddr/ddr3/k4b8g1646d-myk0/) DDR3颗粒速度为800Mhz，在生成loader前需要修改配置

执行`vi ./RKBOOT/RK3399MINIALL.ini`，修改文件中的`rk3399_ddr_933MHz_v1.30.bin`为`rk3399_ddr_800MHz_v1.30.bin`

最后执行`./tools/boot_merger ./RKBOOT/RK3399MINIALL.ini .`在当前目录生成loader