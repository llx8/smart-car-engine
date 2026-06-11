# 车载中控系统

模拟汽车中控台的多进程系统，各模块独立运行，通过 Unix Domain Socket 通信。主控进程负责状态汇聚、自动落锁和持久化，支持命令行调试和自然语言控制。

## 模块

- **车门模块** — 四门车窗、后备箱、中控落锁等
- **状态仪表模块** — 车速、转速、水温等
- **空调模块** — AC 开关、风量、温度等
- **故障灯模块** — 故障码管理和报警灯
- **主控进程** — 启动时恢复上次状态，运行时每秒检查自动落锁、每 10 秒落盘保存
- **car_ctl** — 命令行调试工具，可以读写任意模块的状态值
- **car_ai** — 接 DeepSeek API，用自然语言控制车辆（比如"把空调调到 24 度"）

## 编译

```bash
mkdir build && cd build
cmake ..
make
```

## 运行

先启动子模块，再启动主控：

```bash
./Car_Door &
./Car_Status &
./Car_Air &
./Car_Fault &
./car_main
```

调试示例：

```bash
./car_ctl air get_all          # 查看空调状态
./car_ctl door write lock_status 1   # 手动落锁
./car_ctl status read speed    # 读取车速
```

## 依赖

- GCC（C++20）
- SQLite3
- libcurl
- nlohmann/json
- GoogleTest

## 优化方向

- 故障诊断策略目前比较简单，可以加更完整的 DTC 逻辑
- 传感器数据现在是模拟的，可以接真实 CAN 总线或者车辆模拟器
- 加一个简单的 Web 仪表盘，替代纯命令行的交互方式
