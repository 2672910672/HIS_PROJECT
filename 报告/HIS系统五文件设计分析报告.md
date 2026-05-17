# HIS医院信息系统 — 五核心文件设计分析报告

## 概述

HIS（Hospital Information System）是一个基于 C 语言的控制台医院信息系统，采用**模块化分层设计**。本文分析其五个核心源文件：`his_config.h`、`his.h`、`his_main.c`、`his_tool.c`、`his_link.c`，涵盖配置定义、数据结构、主控流程、工具函数、底层链表五个层面。

---

## 一、his_config.h — 全局配置常量层

**职责**：定义系统中所有宏观常量、枚举类型、文件路径、业务默认值，是整个系统的"配置中心"。

### 设计特点

1. **单一配置入口**：所有魔数集中在 config 头文件中，其他文件只需 `#include "his_config.h"` 即可引用，避免 magic number 散落各处。
2. **枚举代替整数标记**：用 `BedStatus`、`InpatientStatus`、`RecordType`、`RoomType`、`RegStatus` 五个枚举替代裸露的 `int` 常量，提升代码可读性。预留的 `UserRole` 枚举因未使用已清理。

### 关键代码

```c
// 枚举示例：医疗记录类型
typedef enum {
    RECORD_REGISTER = 1,   // 挂号
    RECORD_DIAGNOSIS = 2,  // 诊断
    RECORD_EXAM = 3,       // 检查
    RECORD_INHOSP = 4,     // 住院
    RECORD_PRESCR = 5      // 药品处方
} RecordType;

// ID前缀宏（用于生成全局唯一ID）
#define ID_PREFIX_PATIENT       'P'
#define ID_PREFIX_DOCTOR        'D'
#define ID_PREFIX_DEPT          'K'
#define ID_PREFIX_BED           'B'
#define ID_PREFIX_DRUG          'M'
#define ID_PREFIX_RECORD        'R'
#define ID_PREFIX_APPOINTMENT   'A'
#define ID_PREFIX_SCHEDULE      'S'

// 业务默认值
#define REGISTRATION_FEE    10.0f
#define APPOINTMENT_FEE     20.0f
#define DEFAULT_INSURANCE   0.7f
```

### 设计评价

将所有配置收敛一处，修改业务参数（如挂号费）时无需深入业务代码。枚举类型的定义也为后续的 switch-case 分发提供了类型安全保障。

---

## 二、his.h — 数据结构与接口声明层

**职责**：作为系统的"头文件枢纽"，定义全部数据结构体、通用链表节点、函数声明和全局变量 extern 声明。

### 设计特点

1. **统一头文件**：所有模块只需 `#include "his.h"`，由 his.h 再引入 his_config.h，避免了多头文件相互包含的混乱。
2. **数据模型完整**：定义了 8 个核心业务结构体：`Patient`、`Doctor`、`Department`、`Bed`、`Drug`、`MedicalRecord`、`DoctorSchedule`、`Appointment`，覆盖了医院信息系统的全部主要实体。
3. **通用链表抽象**：定义 `ListNode`（通用数据指针 + ID）和 `LinkList`（头指针 + 长度），用 `void* data` 实现泛型。

### 关键代码

```c
// 通用链表节点（支持任意数据类型）
typedef struct ListNode {
    void* data;                        // 通用数据指针
    int data_size;                     // 数据大小
    char id[MAX_ID_LEN];              // 节点ID（用于快速查找）
    struct ListNode* next;
} ListNode;

typedef struct {
    ListNode* head;
    int length;
} LinkList;

// 患者结构体（最复杂的业务实体）
typedef struct {
    char id[MAX_ID_LEN];
    char name[MAX_NAME_LEN];
    int age;
    char gender[10];
    float insurance_ratio;             // 医保报销比例
    float balance;                     // 账户余额
    InpatientStatus is_inpatient;      // 是否住院（枚举）
    char bed_id[MAX_ID_LEN];           // 绑定床位ID
    int record_count;                  // 关联医疗记录数
    char phone[15];                    // 手机号
    char id_card[20];                  // 身份证号
    char doctor_id[MAX_ID_LEN];        // 挂号医生ID
    char dept_id[MAX_ID_LEN];          // 挂号科室ID
    int register_status;               // 0-未挂号 1-待就诊 2-就诊中 3-已完成
    char register_time[25];            // 挂号时间
    int pin;                           // 6位访问密码
} Patient;
```

### 头文件组织

```
his_config.h  (全局常量)
      ↓
   his.h      (结构体 + 函数声明 + 全局变量 extern)
      ↓
his_main.c / his_tool.c / his_link.c / 各模块 .c 文件
```

---

## 三、his_link.c — 通用链表底层实现

**职责**：提供泛型单向链表的全部操作：初始化、增删节点、查找、遍历、释放。

### 设计特点

1. **void* 泛型**：数据区用 `void*` 配合 `data_size`，支持存储任意结构体类型，同一个链表操作函数可管理 Patient、Doctor、Drug 等任意实体。
2. **ID 快速查找**：每个节点存储 `id` 字段，`FindNode()` 直接比对节点 ID 而非遍历数据区，O(n) 查找但在小型系统中足够高效。
3. **头插/尾插统一接口**：`InsertNode(list, index, ...)` 中 `index=0` 表示头插，`index=-1` 表示尾插，`index=N` 表示指定位置插入。

### 关键代码

```c
// 初始化链表
LinkList* InitList() {
    LinkList* list = (LinkList*)malloc(sizeof(LinkList));
    if (!list) return NULL;
    list->head = NULL;
    list->length = 0;
    return list;
}

// 插入节点（通用：index=0头插，-1尾插）
int InsertNode(LinkList* list, int index, void* data, int data_size, const char* id) {
    if (!list || !data || !id) return -1;
    ListNode* new_node = (ListNode*)malloc(sizeof(ListNode));
    if (!new_node) return -1;
    new_node->data = malloc(data_size);
    if (!new_node->data) { free(new_node); return -1; }
    memcpy(new_node->data, data, data_size);
    new_node->data_size = data_size;
    strncpy(new_node->id, id, MAX_ID_LEN - 1);
    new_node->id[MAX_ID_LEN - 1] = '\0';
    // ... 链表插入逻辑
    list->length++;
    return 0;
}

// 按ID查找
ListNode* FindNode(LinkList* list, const char* id) {
    ListNode* p = list->head;
    while (p != NULL) {
        if (strcmp(p->id, id) == 0) return p;
        p = p->next;
    }
    return NULL;
}

// 释放整个链表
void FreeList(LinkList* list) {
    if (!list) return;
    ListNode* p = list->head, * tmp;
    while (p) {
        tmp = p;
        p = p->next;
        free(tmp->data);  // 先释放数据区
        free(tmp);         // 再释放节点
    }
    free(list);
}
```

### 设计评价

此层是 HIS 系统的"基础设施"，上层业务模块不需要重复实现链表操作。`void*` 泛型的设计思路类似于 C 标准库中的 `qsort`，在纯 C 环境下达到了较好的代码复用效果。

---

## 四、his_tool.c — 通用工具函数层

**职责**：提供不依赖具体业务逻辑的通用函数：输入校验、ID 生成、密码混淆、数据持久化、时间获取等。

### 设计特点

1. **函数指针实现策略模式**：`SaveDataToFile` 和 `LoadDataFromFile` 接受函数指针参数 —— 各业务模块只需提供自己的 `format_func`（将结构体格式化为一行文本）和 `parse_func`（将一行文本解析回结构体），即可复用同一套文件读写逻辑。
2. **统一输入校验**：`getValidChoice(min, max)` 替代裸 `scanf`，使用 `fgets + 手工校验` 的方式彻底消除缓冲区残留问题。
3. **安全字符串拷贝宏**：`HIS_STRNCPY` 封装了 `strncpy` 的陷阱，保证目标缓冲区始终以 `\0` 结尾。
4. **密码混淆**：`passwordObfuscate` 使用 nibble-swap（高4位与低4位交换）实现简单密码混淆，而非明文存储。

### 关键代码

```c
// 函数指针实现的数据持久化（策略模式）
int SaveDataToFile(LinkList* list, const char* filename,
                   void (*format_func)(void*, char*)) {
    // ... 先写入 .tmp 文件，再 rename 覆盖原文件
    // 防止写入中途崩溃导致数据丢失
}

int LoadDataFromFile(LinkList* list, const char* filename,
                     void (*parse_func)(char*, void*)) {
    // ... 逐行读取，跳过空行和仅含分隔符的脏行
    // 对 Windows \r\n 换行符做兼容处理
}

// nibble-swap 密码混淆
void passwordObfuscate(char* pwd) {
    if (!pwd) return;
    for (int i = 0; pwd[i]; i++) {
        pwd[i] = ((pwd[i] << 4) | ((unsigned char)pwd[i] >> 4));
    }
}

// 统一菜单输入校验
int getValidChoice(int min, int max) {
    char buf[64];
    while (1) {
        if (!fgets(buf, sizeof(buf), stdin)) { /* 异常处理 */ }
        buf[strcspn(buf, "\n")] = '\0';
        // 校验是否全为数字、范围是否正确
        if (choice >= min && choice <= max) return choice;
    }
}

// 身份证18位加权校验（GB 11643-1999标准）
int ValidateIDCard(const char* id_card) {
    static const int weights[17] = {7,9,10,5,8,4,2,1,6,3,7,9,10,5,8,4,2};
    static const char check_chars[11] = "10X98765432";
    // 加权求和 → mod 11 → 比对校验位
}
```

---

## 五、his_main.c — 主控流程与角色菜单层

**职责**：程序入口，全局生命周期管理，三层角色（管理员/医生/患者）的菜单分发与控制。

### 设计特点

1. **三层角色权限模型**：
   
   - **管理员**：管理患者、科室/医生/床位、药品药房、修改密码
   - **医生**：查看患者、管理医疗记录、查看预约排班、个人信息、修改密码
   - **患者**：挂号 / 预约挂号、查看/取消挂号、查看医疗记录、自助充值

2. **数据全生命周期管理**：`initGlobalLists() → loadAllHisData() → [菜单循环] → saveAllHisData() → freeGlobalLists()`，启动时加载、退出时保存、全程内存操作。

3. **登录安全机制**：管理员和医生均有**连续5次失败锁定**机制（`static int fail_count`）。

4. **医生密码迁移兼容**：支持旧数据的明文密码自动迁移为混淆密码。

### 关键代码

```c
// 主函数 —— 程序入口
int main(void) {
    initGlobalLists();     // 初始化8个全局链表
    loadAllHisData();      // 从文件加载所有数据到内存

    while (1) {
        printMainMenu();   // 主菜单：管理员/医生/患者
        choice = getValidChoice(0, 3);

        switch (choice) {
        case 1:
            if (adminLogin()) adminMenu();   // 管理员登录 + 菜单
            break;
        case 2:
            if (doctorLogin()) doctorMenu(); // 医生登录 + 菜单
            break;
        case 3:
            patientMenu();                   // 患者菜单（无需登录）
            break;
        case 0:
            saveAllHisData();   // 退出前保存
            freeGlobalLists();  // 释放内存
            exit(0);
        }
    }
}

// 数据加载（启动时调用一次）
static void loadAllHisData(void) {
    loadAdminConfig();
    loadDeptData();
    loadDoctorData();
    loadBedData();
    loadPatientData();
    loadRecordData();
    loadDrugData();
    loadScheduleData();
    loadAppointmentData();
}

// 管理员登录（含5次锁定保护）
static int adminLogin() {
    static int fail_count = 0;
    if (fail_count >= 5) {
        printf("[锁定] 登录尝试次数过多！\n");
        return 0;
    }
    // 验证用户名密码...
    if (strcmp(username, ADMIN_USERNAME) == 0 &&
        strcmp(password, s_admin_password) == 0) {
        fail_count = 0;
        return 1;
    } else {
        fail_count++;
        return 0;
    }
}
```

### 菜单结构

```
主菜单
├── 1. 管理员登录
│   └── 管理员菜单
│       ├── 患者与医疗记录管理
│       ├── 科室/医生/床位管理
│       ├── 药品药房管理
│       └── 修改管理员密码
├── 2. 医生登录
│   └── 医生工作站
│       ├── 查看我的患者
│       ├── 管理医疗记录
│       ├── 查看患者预约信息
│       ├── 查看我的排班
│       ├── 查看个人信息
│       └── 修改密码
└── 3. 患者操作
    └── 患者菜单
        ├── 普通挂号 / 预约挂号
        ├── 查看/取消挂号记录
        ├── 查看医疗记录
        └── 自助充值
```

---

## 六、总体架构评价

### 优点

| 维度         | 评价                                                   |
| ---------- | ---------------------------------------------------- |
| **分层清晰**   | Config → Header → LinkList → Tool → Main，依赖关系单向，职责分明 |
| **代码复用度高** | 通用链表 + 函数指针持久化，各模块只需实现 format/parse 函数               |
| **输入安全**   | 统一使用 `fgets + 手工校验`，无 scanf 缓冲区漏洞                    |
| **数据完整**   | 写文件采用 `write-then-rename` 策略，防止崩溃导致数据损坏              |
| **跨平台兼容**  | 显式处理 Windows `\r\n` 换行符                              |

### 可改进点

| 方面   | 建议                             |
| ---- | ------------------------------ |
| 错误处理 | 多数函数返回 -1 表示失败，调用方有时忽略返回值      |
| 线程安全 | 全局链表无锁，不适合多线程场景                |
| 密码安全 | nibble-swap 属于编码而非加密，仅防"看一眼"级别 |
| 文件格式 | 基于 `                           |
| 链表效率 | O(n) 查找，数据量大时可考虑哈希索引           |

---

## 七、数据文件持久化流程

```
内存 (全局链表)                   磁盘 (data/*.txt)
  ┌──────────┐     SaveDataToFile     ┌──────────┐
  │ patient  │  ──────────────────→  │ patient  │
  │_list     │  ←──────────────────  │ .txt     │
  ├──────────┤    LoadDataFromFile    ├──────────┤
  │ doctor   │  (通过函数指针         │ doctor   │
  │_list     │   format_func /        │ .txt     │
  ├──────────┤   parse_func 适配      ├──────────┤
  │ ...      │   各结构体)            │ ...      │
  └──────────┘                       └──────────┘
```

每个业务实体有 8 个数据文件（patient.txt, doctor.txt, dept.txt, bed.txt, drug.txt, record.txt, schedule.txt, appointment.txt），外加管理员密码文件 `admin.dat`。

---

## 总结

这五个文件构成了 HIS 系统的核心骨架：

- **his_config.h** — 系统的"宪法"，定义所有规则和边界
- **his.h** — 系统的"字典"，定义所有数据结构和对外接口
- **his_link.c** — 系统的"地基"，提供内存数据管理能力
- **his_tool.c** — 系统的"工具箱"，提供跨模块通用能力
- **his_main.c** — 系统的"大脑"，承担主控流程和角色调度

整体设计体现了在纯 C 环境下通过**函数指针实现策略模式**、**void* 实现泛型容器**、**分层解耦**等经典嵌入式/桌面端 C 语言设计思想。
