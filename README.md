# 智能车载引擎

C++20 车载中控后台系统，多进程架构，Unix Domain Socket 通信。

## 系统架构

```
car_ctl / car_ai（客户端）
       │ UDS
       ▼
    car_main（主控）
       │ UDS
  ┌────┼────┬────┐
  ▼    ▼    ▼    ▼
door status air fault（子模块，各自独立进程）
```

每个子模块是独立的 epoll 服务端进程，主控负责状态恢复、自动落锁、定期落盘。

## 编译

```bash
mkdir build && cd build
cmake ..
make
cd bin/
```

## 启动

按顺序启动，子模块先就绪再启动主控：

```bash
./car_door      # 终端1
./car_status    # 终端2
./car_air       # 终端3
./car_fault     # 终端4
./car_main      # 终端5（子模块就绪后）
./car_ctl air get_all   # 终端6（调试用）
./car_ai ./car_ai.conf  # 终端7（AI 对话，可选）
```

Ctrl+C 优雅退出，主控退出前自动落盘。

## car_ctl 命令行工具

```bash
./car_ctl <模块> get_all                # 查询全部字段
./car_ctl <模块> read <字段名>           # 读单个字段
./car_ctl <模块> write <字段名> <值>     # 写入
```

模块：`door` `status` `air` `fault`

示例：
```bash
./car_ctl air write temp_set 26    # 空调设26度
./car_ctl status read speed        # 读车速
./car_ctl door write lock_status 1 # 锁门
```

## car_ai 自然语言控制

需要配置 `car_ai.conf`（从 `car_ai.conf.example` 复制，填入 API Key）。

```bash
./car_ai ./car_ai.conf
```

支持 DeepSeek、OpenAI、Claude、Gemini 等平台，修改 conf 里的 `model` 和 `api_url` 即可切换。

AI 有安全限制：禁止操控档位，车速 > 5km/h 时禁止开门。

## IPC 协议

所有进程间通信用固定长度的 `Car::Msg` 结构体（`CarData.hpp` 定义），通过 UDS 传输。

新增模块只需：在 `CarData.hpp` 加状态结构体 → 写模块实现 → `CMakeLists.txt` 加编译目标 → `car_ctl` 加路由。

## 依赖

- C++20（GCC）
- libmysqlclient-dev（审计日志）
- hiredis（Redis 状态管理）
- libcurl（car_ai HTTP 请求）
- nlohmann/json（JSON 解析）
- GoogleTest（单元测试）
