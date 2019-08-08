# ribp

ripb 是 `Riscv integer instruction resource borrowing protocols` (risc-v 整数指令资源借用协议) 的协议与参考实现, 目标是:

  - 实现 `march=rv32im`, 不包含 `CSR` 寄存器的虚拟机(目标软件可直接运行在rv32im CPU上)
  - 定义一套API接口 (`ribpapi`), 由宿主机实现, 提供给虚拟机内软件调用
  - 定义一套协议, 用于程序的传输, 资源的发现
  - 资源权限由宿主机管理
  - `ribp` 协议的传输层可以是任何可以进行数据交换的方式, `ribp` 不关心数据的传输. 例如: TCP/IP, 蓝牙, WiFi, 文件系统, 管道, socket, 二维码, 卡车运输的硬盘,漂流瓶

#### 术语定义

  - `宿主机` - 实现了 `ribp` 设备
  - `虚拟机` - 运行 `rv32im` 的虚拟机或 `rv32im` CPU, 必须实现 `ribpapi` 
  - `虚拟软件` - `rv32im` 的机器码
  - `借用机` - 向宿主机借用资源的设备

#### 运作过程

  借用机通过 ribp 协协议发现宿主机支持的资源, 并决定借用宿主机的资源, 借用机将虚拟软件发送给宿主机, 宿主机检测权限, 如果权限满足, 宿主机加载虚拟软件到虚拟机,并运行虚拟机.
    
#### 运用举例

  一台实现了ribp 和蓝牙PBAP协议的手机, 手机上存储了上千条通讯录, 一个性能很低的实现了PBAP的蓝牙设备, 蓝牙设备有个显示屏, 可以用来显示通讯录, 蓝牙设备CPU性能很低且只有很小的RAM和存储空间, 无法存储手机上的全部通讯录, PBAP是通过vcard传输通讯录, 蓝牙设备解析vcard占用很多CPU. 现在要实现在蓝牙设备上显示手机上的通讯录, 并且是按语言排序的, 例如中文的拼音, 英文的字母排序, 同时要支持搜索. 借助ribp, 这可以很容易实现, 首先编写解析vcard,排序通讯录, 并把通讯录存储起来的软件. 将软件编译成rv32im机器码, 存储到蓝牙设备. 蓝牙和手机链接, 通过ribp协议, 蓝牙获取了手机的PBAP协议的资源, 并把rv32im机器码通过RFCOMM传输到手机, 手机验证权限OK, 将PBAP资源链接到rv32im虚拟机并运行虚拟机, 虚拟机里面的软件将vcard解析排序后存储. 此时用户操作蓝牙设备, 搜索联系人'ABC',蓝牙将请求发给虚拟机, 虚拟机执行搜索,并将结果通过RFCOMM传给蓝牙.

#### ribpapi

    - int hostapi_open(const char *path, int flags, ...) - 同Unix open 系统调用
    - int hostapi_close(int fd) - 同Unix close 系统调用
    - int hostapi_read(int fd, void *buf, unsigned size) - 同Unix read 系统调用
    - int hostapi_write(int fd, const void *buf, unsigned size) - 同Unix write 系统调用
    - int hostapi_lseek(int fd, unsigned offset, int whencr) - 同Unix lseek 系统调用
    - int hostapi_poll(struct pollfd *pfds, int pfdn, int timeout) - 同 Linux poll 系统调用 

### 参考实现测试

编译虚拟机
```sh
$ cd ribp
$ cmake -GNinja -B build
$ cmake --build build
```

编译ribp-hello-world

```sh
$ git clone https://github.com/LuoZhongYao/rv32im.git
$ cd rv32im
$ cmake -GNinja -B build
$ cmake --build build
```

运行测试

```sh
$ ./build/src/rscv -r ${rv32im}/rom.bin
```
