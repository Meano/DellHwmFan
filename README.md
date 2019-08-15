# Dell Hwm IO Fan
用比Smm更底层的Hwm IO直接控制EC芯片
## 起因
在某鱼收到了两个Dell Optiplex 7060mff的微型主机的工程版（大概是内部研发用的，红色主板，南桥的Q370芯片都是ES版的）。  
Bios也是17年3代主板都还没有发售的时候的版本，所以有了更新Bios到最新版的想法。  
研究了Dell的UEFI Bios结构之后，重新合成了带有主机当前DVAR及MAC等Factory信息的最新版UEFI Bios。  
通过自己写的 [FlashTool](https://github.com/Meano/FlashTool) 和 [STFlashRW](https://github.com/Meano/STFlashRW) 两个软件的配合直接读写Flash芯片成功将Bios更新到了最新版。  
但发现Dell新版Bios的风扇策略比开发版的Bios更谨慎一些，原来待机2000RPM变成了1050RPM，稍微烤一下机CPU就碰了温度上限疯狂降频。  
夏天来临，为了让小主机长期稳定运行，于是有了进一步控制风扇想法。  
## 研究过程
对Dell UEFI结构的研究见FlashTool Repo。  
1. 通过UEFITool对字符串"Fan"的搜索定位了几个SMM Module。
2. 反编译这几个DXE Module，由于内部数据结构和GUID反复注册调用比较复杂，在这里卡了很久。
3. 搜索了一些资料包括 [Dell GUID List](https://raw.githubusercontent.com/Jerry2613/Code_in_Python/8eea88baeff9febf1203dda05ba55f4407e4f80d/Exercise/Log_Guid_Transfer/Guid_all.txt) 还有 [EDK II Source Code](https://github.com/tianocore/edk2) 对反编译的伪代码的理解和数据结构的拼接有不少的帮助。
4. 研究了DxeFansProtocol模块，基本完整的反编译了GetSpeedStep，SetFanStep，GetSpeedRPM，SetFanSpeed，GetTolerance，GetRequiredinRange，GetMaxSamples，fanctl8_hwioinit，TjmaxSet这些函数，这些函数可以被CPU Smm中断调用，但是没有直接或间接控制风扇转速的接口。
5. 发现了在DXE模块中对Smm的初始化中有很多HwmIO efi模块的调用，猜测是南桥跟EC的IO交互。
6. 查找了主板使用的EC芯片的Datasheet，确认了EC芯片含三路风扇的自动控制，大概确认了HwmIO是对EC芯片参数的配置。
7. 反编译了HwmIO的读写函数，对应反编译的机器码重写了Dell Hwm Fan的C代码来尝试对HwmIO进行调整。
8. 反复测试了HwmIO的0x0000~0x0300地址的数据调试，终于得到了EC寄存器中配置风扇的地址。
## 编译
    gcc -o DellHwmFan DellHwmFan.c
## 命令用法
    sudo ./DellHwmFan
## 地址映射
    0x85 Point1 斜率（越小加速度越快）
    0x86 Point2 斜率（越小加速度越快）

    0x90 Point1 温度 (val - 0x40）
    0x91 Point2 温度 (val - 0x40)

    0x8A Point1 起转PWM（val + 0x15）
    0x280~0x281 (High Speed)

    0x285 最小PWM

    0x26 CPUtemp+0x40
    0x27 MBtemp
    0x28 HDDtemp

    0x36 cur pwm(0~255)
