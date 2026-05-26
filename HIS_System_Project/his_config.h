#ifndef HIS_CONFIG_H
#define HIS_CONFIG_H

/*
 * 系统配置常量头文件
 *   长度常量（ID/名称/密码/时间等最大长度）
 *   ID 生成前缀、数据文件路径、业务默认值
 *   枚举类型（BedStatus/InpatientStatus/RecordType/RoomType/RegStatus）
 */

// ==================== 1. 长度常量定义 ====================
#define MAX_NAME_LEN        50      // 姓名/科室名最大长度
#define MAX_SPECIALTY_LEN   100     // 医生特长最大长度
#define MAX_ID_LEN          20      // ID最大长度
#define MAX_PWD_LEN         20      // 密码最大长度
#define MAX_TIME_LEN        30      // 时间字符串最大长度
#define MAX_DETAIL_LEN      200     // 详情/备注最大长度
#define MAX_LINE_LEN        1024    // 文件读取行最大长度
#define MAX_DATA_SIZE       1024    // 通用数据缓冲区大小

// ==================== 2. ID生成前缀常量 ====================
#define ID_PREFIX_PATIENT       'P'     // 患者ID前缀
#define ID_PREFIX_DOCTOR        'D'     // 医生ID前缀
#define ID_PREFIX_DEPT          'K'     // 科室ID前缀 (K=科)
#define ID_PREFIX_BED           'B'     // 床位ID前缀
#define ID_PREFIX_DRUG          'M'     // 药品ID前缀 (M=Medicine)
#define ID_PREFIX_RECORD        'R'     // 记录ID前缀
#define ID_PREFIX_APPOINTMENT   'A'     // 预约ID前缀
#define ID_PREFIX_SCHEDULE      'S'     // 排班ID前缀，与患者 P 区分

// ==================== 3. 数据文件路径定义 ====================
#define FILE_PATIENT        "data/patient.txt"      // 患者数据文件
#define FILE_DOCTOR         "data/doctor.txt"       // 医生数据文件
#define FILE_DEPT           "data/dept.txt"         // 科室数据文件
#define FILE_BED            "data/bed.txt"          // 床位数据文件
#define FILE_DRUG           "data/drug.txt"         // 药品数据文件
#define FILE_RECORD         "data/record.txt"       // 医疗记录文件
#define FILE_SCHEDULE       "data/schedule.txt"     // 医生排班
#define FILE_APPOINTMENT    "data/appointment.txt"  // 预约挂号

// ==================== 4. 业务默认值配置 ====================
#define REGISTRATION_FEE    1000    // 普通挂号费（分）
#define APPOINTMENT_FEE     2000    // 预约挂号费（分）
#define DEFAULT_INSURANCE   0.7f    // 默认医保报销比例 (70%)
#define DRUG_WARNING_RATIO  0.2f    // 库存预警系数 (低于阈值20%预警)
#define MAX_ID_RETRY        999     // ID 生成最大重试次数

// ==================== 5. 管理员登录配置 ====================
#define ADMIN_USERNAME      "admin"       // 管理员账号
#define ADMIN_PASSWORD      "123456"      // 管理员密码

// ==================== 6. 枚举类型定义 ====================
// 床位状态
typedef enum {
    BED_FREE = 0,               // 空闲
    BED_OCCUPIED = 1            // 占用
} BedStatus;

// 患者住院状态
typedef enum {
    PATIENT_OUT = 0,            // 未住院
    PATIENT_IN = 1              // 住院中
} InpatientStatus;

// 医疗记录类型
typedef enum {
    RECORD_REGISTER = 1,        // 挂号
    RECORD_DIAGNOSIS = 2,       // 诊断
    RECORD_EXAM = 3,            // 检查
    RECORD_INHOSP = 4,          // 住院
    RECORD_PRESCR = 5           // 药品处方
} RecordType;

// 病房类型
typedef enum {
    ROOM_NORMAL = 1,            // 普通病房
    ROOM_SEMI = 2,              // 半私密病房
    ROOM_VIP = 3                // VIP病房
} RoomType;

// 患者挂号状态
typedef enum {
    REG_STATUS_NONE = 0,      // 未挂号
    REG_STATUS_PENDING = 1,   // 待就诊
    REG_STATUS_IN_PROGRESS = 2,// 就诊中（未使用）
    REG_STATUS_DONE = 3       // 已完成
} RegStatus;

// ==================== 7. 通用分隔符定义 ====================
#define MENU_LINE_LEN       56      // 菜单分隔线长度

#endif
