#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace Car{
constexpr const char* SOCK_DOOR = "/tmp/car_door.sock";
constexpr const char* SOCK_STATUS = "/tmp/car_status.sock";
constexpr const char* SOCK_AIR = "/tmp/car_air.sock";
constexpr const char* SOCK_FAULT  = "/tmp/car_fault.sock";

constexpr const char* LOG_FILE_PATH = "./car_log.txt";
constexpr const char* INI_FILE_PATH = "./car_info.ini";
constexpr int MAX_FAULT_CODE = 10;
constexpr int DEVICE_NAME_LEN = 32;
constexpr size_t MSG_STR_SIZE = 64;
constexpr size_t MSG_ARR_SIZE = 32;
constexpr uint16_t CAR_MSG_MAGIC = 0xCA12;
constexpr uint8_t  CAR_MSG_VERSION = 1;

enum class ModuleID : uint8_t { DOOR = 1, STATUS, AIR, FAULT };
enum class MsgType : uint8_t { CMD = 1, RESPONSE = 2 };
enum class CmdType : uint8_t { READ = 1, WRITE = 2, GET_ALL = 3 };
enum class ValType : uint8_t { U8, I32, F32, STR, STR_U8, STR_U16, STR_I32, STR_F32} ;
enum class Gear : uint8_t { P = 0, R = 1, N = 2, D = 3 };

#pragma pack(push, 1)
struct DoorState { uint8_t front_left, front_right, back_left, back_right, trunk, lock_status; };
struct StatusState { float speed; int rpm; float water_temp, oil_temp, fuel, battery_voltage; uint8_t gear, hand_brake; };
struct AirState { uint8_t ac_switch, fan_speed; int temp_set; uint8_t inner_cycle; };
struct FaultState { uint8_t fault_count; uint16_t fault_codes[MAX_FAULT_CODE]; uint8_t wring_light; };

struct Msg {
    uint16_t magic   = CAR_MSG_MAGIC;
    uint8_t  version = CAR_MSG_VERSION;
    uint8_t  reserved = 0;
    ModuleID mod_id;
    MsgType msg_type;
    CmdType cmd_type;
    uint8_t item_id;
    ValType val_type;
    union {
        uint8_t u8;
        int32_t i32;
        float f32;
        char str[MSG_STR_SIZE];
        uint8_t arr_u8[MSG_ARR_SIZE];
        uint16_t arr_u16[MSG_ARR_SIZE];
        int32_t arr_i32[MSG_ARR_SIZE];
    } value;
    int result;
};

#pragma pack(pop)

[[nodiscard]] constexpr bool isValidMsg(const Msg& msg) {
    return msg.magic == CAR_MSG_MAGIC && msg.version == CAR_MSG_VERSION;
}

static_assert(sizeof(DoorState)   <= sizeof(Msg::value), "DoorState too large for Msg union");
static_assert(sizeof(StatusState) <= sizeof(Msg::value), "StatusState too large for Msg union");
static_assert(sizeof(AirState)    <= sizeof(Msg::value), "AirState too large for Msg union");
static_assert(sizeof(FaultState)  <= sizeof(Msg::value), "FaultState too large for Msg union");

struct FieldMeta {
    const char* module;
    const char* field;
    const char* sock_path;
    uint8_t     item_id;
    ValType     val_type;
    ModuleID    mod_id;
    bool        ai_accessible;
};

inline constexpr FieldMeta FIELD_TABLE[] = {
    {"door", "front_left",   SOCK_DOOR,   1, ValType::U8,  ModuleID::DOOR,   true},
    {"door", "front_right",  SOCK_DOOR,   2, ValType::U8,  ModuleID::DOOR,   true},
    {"door", "back_left",    SOCK_DOOR,   3, ValType::U8,  ModuleID::DOOR,   true},
    {"door", "back_right",   SOCK_DOOR,   4, ValType::U8,  ModuleID::DOOR,   true},
    {"door", "trunk",        SOCK_DOOR,   5, ValType::U8,  ModuleID::DOOR,   true},
    {"door", "lock_status",  SOCK_DOOR,   6, ValType::U8,  ModuleID::DOOR,   true},
    {"status", "speed",           SOCK_STATUS, 1, ValType::F32, ModuleID::STATUS, false},
    {"status", "rpm",             SOCK_STATUS, 2, ValType::I32, ModuleID::STATUS, false},
    {"status", "water_temp",      SOCK_STATUS, 3, ValType::F32, ModuleID::STATUS, false},
    {"status", "oil_temp",        SOCK_STATUS, 4, ValType::F32, ModuleID::STATUS, false},
    {"status", "fuel",            SOCK_STATUS, 5, ValType::F32, ModuleID::STATUS, false},
    {"status", "battery_voltage", SOCK_STATUS, 6, ValType::F32, ModuleID::STATUS, false},
    {"status", "gear",            SOCK_STATUS, 7, ValType::U8,  ModuleID::STATUS, false},
    {"status", "hand_brake",      SOCK_STATUS, 8, ValType::U8,  ModuleID::STATUS, true},
    {"air", "ac_switch",    SOCK_AIR, 1, ValType::U8,  ModuleID::AIR, true},
    {"air", "fan_speed",    SOCK_AIR, 2, ValType::U8,  ModuleID::AIR, true},
    {"air", "temp_set",     SOCK_AIR, 3, ValType::I32, ModuleID::AIR, true},
    {"air", "inner_cycle",  SOCK_AIR, 4, ValType::U8,  ModuleID::AIR, true},
    {"fault", "fault_count", SOCK_FAULT, 1, ValType::U8,      ModuleID::FAULT, false},
    {"fault", "fault_codes", SOCK_FAULT, 2, ValType::STR_U16, ModuleID::FAULT, false},
    {"fault", "wring_light", SOCK_FAULT, 3, ValType::U8,      ModuleID::FAULT, false},
};

inline const FieldMeta* findField(const std::string& module, const std::string& field) {
    for (const auto& m : FIELD_TABLE) {
        if (m.module == module && m.field == field)
            return &m;
    }
    return nullptr;
}

[[nodiscard]] constexpr const char* gearToText(uint8_t gear) {
    switch (static_cast<Gear>(gear)) {
        case Gear::P: return "P";
        case Gear::R: return "R";
        case Gear::N: return "N";
        case Gear::D: return "D";
        default: return "UNKNOWN";
    }
}

} // namespace Car