# 车载中控系统

练手项目，多进程架构，用 Unix Domain Socket 通信。

## 编译

```bash
mkdir build && cd build
cmake ..
make
```

## 运行

先启动子模块，再启动主控：

```bash
./Car_Door
./Car_Status
./Car_Air
./Car_Fault
./car_main
```

用 car_ctl 调试：
```bash
./car_ctl air get_all
./car_ctl door write lock_status 1
```

## 依赖

- GCC（C++20）
- SQLite3
- libcurl
- nlohmann/json
- GoogleTest
