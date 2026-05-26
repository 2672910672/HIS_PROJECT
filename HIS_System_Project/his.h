#define _CRT_SECURE_NO_WARNINGS
#ifndef HIS_H
#define HIS_H

/*
 * HIS 系统全局头文件
 *   包含所有结构体定义（Patient/Doctor/Department/Bed/Drug/MedicalRecord 等）
 *   全局链表变量声明（供所有模块共用）
 *   所有外部函数声明（跨模块调用入口）
 *   安全字符串拷贝宏 HIS_STRNCPY
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "his_config.h"  // 全局配置常量

/* 安全字符串拷贝，保证目标以 '\0' 结尾；cap 为目标缓冲区的总字节数 */
#define HIS_STRNCPY(dst, src, cap) do { \
    char* _his_d = (dst); \
    size_t _his_c = (size_t)(cap); \
    const char* _his_s = (src); \
    if (_his_d && _his_c > 0U) { \
        strncpy(_his_d, (_his_s) ? (_his_s) : "", _his_c - 1U); \
        _his_d[_his_c - 1U] = '\0'; \
    } \
} while (0)

// ==================== 1. 统一结构体定义 (使用各模块中的宏) ====================

// 患者结构体
typedef struct {
    char id[MAX_ID_LEN];               // 患者ID
    char name[MAX_NAME_LEN];           // 姓名
    int age;                           // 年龄
    char gender[10];                   // 性别
    float insurance_ratio;             // 医保报销比例
    long long balance;                 // 账户余额（分）
    InpatientStatus is_inpatient;      // 是否住院 (枚举)
    char bed_id[MAX_ID_LEN];           // 绑定床位ID
    int record_count;                  // 关联医疗记录数

    //扩展字段：手机号 + 身份证
    char phone[15];                    // 手机号
    char id_card[20];                  // 身份证号
    char doctor_id[MAX_ID_LEN];        // 挂号医生ID
    char dept_id[MAX_ID_LEN];          // 挂号科室ID
    int register_status;               // 0-未挂号 1-待就诊 2-就诊中 3-已完成
    char register_time[25];            // 挂号时间
    char pin[7];                         // 6位访问密码(空串=未设置)
    char register_record_id[MAX_ID_LEN]; // 关联的挂号记录ID（精确退款用）

} Patient;

// ==================== 预约挂号 结构体 ====================
typedef struct {
    char id[MAX_ID_LEN];           // 排班ID
    char doctor_id[MAX_ID_LEN];    // 医生ID
    char dept_id[MAX_ID_LEN];      // 科室ID
    char date[11];                 // 日期 YYYY-MM-DD
    char time_slot[20];            // 时间段
    int max_patients;              // 总号源
    int current_patients;          // 已预约数
    int is_available;              // 是否可预约
} DoctorSchedule;

typedef struct {
    char id[MAX_ID_LEN];           // 预约ID
    char patient_id[MAX_ID_LEN];   // 患者ID
    char schedule_id[MAX_ID_LEN];  // 排班ID
    char status[20];              // 状态：已预约/已完成/已取消
    char create_time[MAX_TIME_LEN]; // 预约时间
    long long cost;               // 支付金额（分）
} Appointment;

// 医生结构体
typedef struct {
    char id[MAX_ID_LEN];               // 医生ID
    char name[MAX_NAME_LEN];           // 姓名
    char dept_id[MAX_ID_LEN];          // 所属科室ID
    char specialty[MAX_SPECIALTY_LEN];      // 擅长领域
    char account[MAX_NAME_LEN];        // 登录账号
    char password[MAX_PWD_LEN];        // 登录密码

    //医生每日挂号限额
    int max_register;                  // 每日最大挂号量
    int current_register;              // 当前已挂号量
    char register_date[11];            // 挂号日期（用于每日重置，格式YYYY-MM-DD）

} Doctor;

// 科室结构体
typedef struct {
    char id[MAX_ID_LEN];               // 科室ID
    char name[MAX_NAME_LEN];           // 科室名称
    int doctor_count;                  // 医生数
} Department;

// 床位结构体
typedef struct {
    char id[MAX_ID_LEN];               // 床位ID
    RoomType room_type;                // 病房类型 (枚举)
    char dept_id[MAX_ID_LEN];          // 所属科室ID
    BedStatus status;                  // 状态 (枚举)
    char patient_id[MAX_ID_LEN];       // 占用患者ID
    char admit_time[MAX_TIME_LEN];     // 入院时间
} Bed;

// 药品结构体
typedef struct {
    char id[MAX_ID_LEN];               // 药品ID
    char general_name[MAX_NAME_LEN];   // 通用名
    char trade_name[MAX_NAME_LEN];     // 商品名
    char alias[MAX_NAME_LEN];          // 别名
    float price;                       // 单价
    int stock;                         // 库存
    int warning_threshold;             // 预警阈值
    char dept_id[MAX_ID_LEN];          // 所属科室ID
} Drug;

// 医疗记录结构体
typedef struct {
    char id[MAX_ID_LEN];               // 记录ID
    char patient_id[MAX_ID_LEN];       // 关联患者ID
    char doctor_id[MAX_ID_LEN];        // 负责医生ID
    RecordType type;                   // 记录类型 (枚举)
    long long cost;                    // 费用（分）
    char detail[MAX_DETAIL_LEN];       // 详情
    char create_time[MAX_TIME_LEN];    // 创建时间
    int cancelled;                     // 是否已取消（退款）
} MedicalRecord;

// ==================== 2. 通用链表节点定义 ====================
typedef struct ListNode ListNode;
struct ListNode {
    void* data;                        // 通用数据指针
    int data_size;                     // 数据大小
    char id[MAX_ID_LEN];               // 节点ID (用于快速查找)
    ListNode* next;
};

typedef struct {
    ListNode* head;
    int length;
} LinkList;

// ==================== 3. 通用函数声明 ====================
// --- 链表操作 (his_link.c实现) ---
LinkList* InitList();
int InsertNode(LinkList* list, int index, void* data, int data_size, const char* id);
int DeleteNode(LinkList* list, const char* id);
ListNode* FindNode(LinkList* list, const char* id);
void TraverseList(LinkList* list, void (*print_func)(void*));
void FreeList(LinkList* list);

// --- 工具函数 (his_tool.c实现) ---
void ClearInputBuffer();
void readString(char* buf, int size);        // 统一字符串输入（fgets+清换行+溢出处理）
int inputLine(char* buf, size_t size);       // 安全行输入：fgets+溢出清理，返回1成功
int getConfirm(void);                        // 统一 y/n 确认输入

int getValidChoice(int min, int max);   // 统一菜单输入校验
void GenerateID(char* id, char type);
int generateUniqueID(char* out_id, char prefix, LinkList* list);  // 安全生成唯一ID，0成功/-1失败
int ValidateNumber(const char* str);
int ValidatePhone(const char* phone);           // 手机号格式校验
int ValidateIDCard(const char* id_card);         // 身份证号格式校验
int ValidateNoPipe(const char* str);             // 禁止字段分隔符"|"
void HisGetSystemTime(char* time_str);
int SaveDataToFile(LinkList* list, const char* filename, void (*format_func)(void*, char*));
int LoadDataFromFile(LinkList* list, const char* filename, void (*parse_func)(char*, void*));
void PrintSeparator();                                              // 打印菜单分隔线
void passwordObfuscate(char* pwd);                                  // 密码混淆（nibble-swap）
void waitForEnter(void);                                            // 等待回车继续

// ==================== 功能函数 ====================
// --- 模块入口函数 (各模块实现) ---
void patientModule();
void dept_bedModule();
void drugModule();
void doctorSubMenu();
void scheduleSubMenu();
void globalStatsSubMenu();
void backupAllData();

/* GUI 模块需要额外入口（原是 his_main.c static 函数，GUI 模式需要访问） */
extern int verifyAdminPassword(const char* username, const char* password);
extern void initGlobalLists(void);
extern void loadAllHisData(void);
extern void saveAllHisData(void);
extern void freeGlobalLists(void);

// --- 模块间共用函数 ---
extern void printDeptInfo(void* data);
extern void printPatientInfo(void* data);
extern void printDoctorInfo(void* data);
extern void printBedInfo(void* data);
extern void printDrugInfo(void* data);

extern void savePatientData(void);
extern void saveDeptData(void);
extern void saveDoctorData(void);
extern void saveBedData(void);
extern void saveRecordData(void);
extern void saveDrugData(void);

/* 由 main 统一加载，各模块只负责各自文件；空文件可正常加载为空链表 */
extern void loadDeptData(void);
extern void loadDoctorData(void);
extern void loadBedData(void);
extern void loadPatientData(void);
extern void loadRecordData(void);
extern void loadDrugData(void);
extern void saveScheduleData(void);
extern void loadScheduleData(void);
extern void saveAppointmentData(void);
extern void loadAppointmentData(void);

/* 患者只读查询入口，在其他模块中也需要引用所以放在 patient */
extern void patientViewOnlyModule(Patient* p);

// ==================== 患者与医疗记录管理（跨模块调用）====================
extern int isPhoneUsedByOther(const char* phone, const char* exclude_id);
extern int isIDCardUsedByOther(const char* id_card, const char* exclude_id);
extern int getAgeFromIDCard(const char* id_card);
extern int verifyPatientPin(Patient* p);
extern void inputAndViewRecords(void);       // 管理员查看患者记录
extern void inputAndAddRecord(void);         // 管理员新增医疗记录
extern void inputAndModifyRecord(void);      // 管理员修改医疗记录
extern void inputAndDeleteRecord(void);      // 管理员删除医疗记录

// 独立输入验证函数（跨模块共用，his_appointment 等模块需要）
extern int inputName(char* out_name, size_t cap);
extern int inputAge(int* out_age);
extern int inputGender(char* out_gender, size_t cap);
extern int inputInsuranceRatio(float* out_ratio);
extern int inputBalance(long long* out_balance);
extern int inputPhone(char* out_phone, size_t cap, const char* exclude_id);
extern int inputIDCard(char* out_idcard, size_t cap, const char* exclude_id, int current_age);
extern void inputPin(char* out_pin);

// 健壮的字段分割函数：与 strtok 不同，连续分隔符不会跳过，正确产生空字符串字段
extern char* next_token(char** str);

// ==================== 患者菜单相关函数 (his_main.c 使用) ====================
extern Patient* patientLogin(void);               // 患者登录（ID/创建）
extern void patientSelfService(void);             // 患者自助服务主菜单
extern void normalRegistration(Patient* p);       // 普通挂号，患者菜单选项1
extern void appointmentRegistration(Patient* p);  // 预约挂号，患者菜单选项2
extern void viewMyRegistration(Patient* p);       // 查看挂号记录，患者菜单选项3
extern void cancelMyRegistration(Patient* p);     // 取消我的挂号/预约，患者菜单选项4
extern void patientRecharge(Patient* p);          // 自助充值，患者菜单选项6
extern void appointmentCheckIn(Patient* p);       // 预约签到（转为挂号），患者菜单选项7

// ==================== 医生菜单相关函数 (his_main.c 使用) ====================
extern void queryPatientByDoctor(void);     // 医生查看患者，医生菜单选项1
extern void medicalRecordModule(void);      // 医生管理医疗记录，医生菜单选项2
extern void queryMyAppointment(void);       // 医生查看预约，医生菜单选项3

// ==================== 全局链表变量 (各模块共用) ====================
extern LinkList* patient_list;
extern LinkList* doctor_list;
extern LinkList* dept_list;
extern LinkList* bed_list;
extern LinkList* drug_list;
extern LinkList* record_list;
extern LinkList* schedule_list;    // 医生排班链表
extern LinkList* appointment_list; // 预约记录链表

// ==================== 当前登录医生会话 ====================
extern char g_current_doctor_id[MAX_ID_LEN];
extern char g_current_doctor_name[MAX_NAME_LEN];

#endif
