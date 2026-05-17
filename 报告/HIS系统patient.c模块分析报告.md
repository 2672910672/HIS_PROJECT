# HIS医院信息系统 — patient.c 模块分析报告

## 概述

`patient.c` 在重构前是 HIS 系统中**规模最大**的模块文件（原2080行），现已按功能拆分为三个文件：`patient.c`（约700行）、`registration.c`（约520行）、`record.c`（约460行），总代码量不变。拆分后职责分明，各有清晰的编译单元边界，集成了**患者管理**、**医疗记录**、**排班与预约**、**医生工作站**、**挂号注册**五大子系统。

---

## 一、模块定位

### 职责范围

- 患者 CRUD（管理员视角）
- 医疗记录 CRUD（管理员 + 医生视角）
- 排班与预约的文件持久化
- 普通挂号与预约挂号（患者自助）
- 医生查看患者、管理诊断/处方记录
- 患者自助查看医疗记录与充值

### 对外接口（在 his.h 中声明）

```c
// 患者管理入口（管理员菜单）
void patientModule();

// 医生菜单功能
void queryPatientByDoctor();     // 医生查看患者
void medicalRecordModule();      // 医生医疗记录管理
void queryMyAppointment();      // 医生查看预约

// 患者自助功能
void normalRegistration();       // 普通挂号
void appointmentRegistration();  // 预约挂号
void viewMyRegistration();       // 查看我的挂号
void cancelMyRegistration();     // 取消挂号/预约
void patientRecharge();          // 自助充值
void patientViewOnlyModule();    // 患者只读查看医疗记录
```

---

## 二、文件拆分架构

### patient.c（约700行）— 患者信息管理

```
patient.c
├── 打印函数 (printPatientInfo)
├── 文件 I/O (患者 data/patient.txt)
│   ├── savePatientData / formatPatient / loadPatientData / parsePatient
├── 辅助校验函数（跨模块共享）
│   ├── isPhoneUsedByOther       — 手机号唯一性（供 registration 调用）
│   ├── isIDCardUsedByOther      — 身份证唯一性（供 registration 调用）
│   ├── getAgeFromIDCard         — 从身份证提取年龄（供 registration 调用）
│   └── verifyPatientPin         — 患者6位密码验证（供 registration 调用）
├── 患者查询 (queryPatientSubMenu — 5种查询方式)
├── 患者管理入口 (patientModule — 管理员增删改查)
│   ├── inputPatientBasicInfo / modifyPatientInfo
│   ├── inputAndModifyPatient / inputAndDeletePatient
│   └── case 5: 委托至 record.c 完成
└── 患者自助
    └── patientRecharge / patientViewOnlyModule
```

### record.c（约460行）— 医疗记录 + 医生工作站

```
record.c
├── 文件 I/O (医疗记录 data/record.txt)
│   ├── saveRecordData / formatRecord / loadRecordData / parseRecord
├── 医生端校验 (validateDoctorPatientAccess — 三重校验)
├── 医生菜单
│   ├── queryPatientByDoctor     — 查看我的患者
│   ├── medicalRecordModule      — 诊断/处方/改状态
│   └── queryMyAppointment      — 查看我的预约
└── 医疗记录 CRUD（管理员视角）
    ├── inputAndViewRecords / inputAndAddRecord
    ├── inputAndModifyRecord / inputAndDeleteRecord
    └── displayPatientRecords / inputRecordInfo（内部辅助）
```

### registration.c（约500行）— 挂号 + 预约

```
registration.c
├── 文件 I/O (预约 data/appointment.txt)
│   ├── saveAppointmentData / formatAppointment / loadAppointmentData / parseAppointment
├── 内部辅助
│   ├── checkAndResetDoctorDaily     — 医生号源跨日重置
│   ├── inputAndFindOrCreatePatient  — 查找或自动创建患者
│   └── selectDeptAndDoctor          — 选择科室+医生
├── 挂号（患者自助）
│   ├── normalRegistration            — 普通挂号（7步流程）
│   └── appointmentRegistration       — 预约挂号（6步流程）
└── 挂号查询与取消
    ├── viewMyRegistration            — 查看我的挂号/预约
    └── cancelMyRegistration          — 取消挂号/预约（含退费）
```

---

## 三、关键设计分析

### 1. 文件持久化的"四元组"模式

每个业务实体在 patient.c 中遵循相同的持久化模式：

```c
// 1. 保存：调用通用工具函数
void savePatientData(void) {
    SaveDataToFile(patient_list, FILE_PATIENT, formatPatient);
}

// 2. 格式化为一行（用 | 分隔）
static void formatPatient(void* data, char* line) {
    sprintf(line, "%s|%s|%d|%s|%.2f|%.2f|%d|...", ...);
}

// 3. 加载
void loadPatientData(void) {
    LoadDataFromFile(patient_list, FILE_PATIENT, parsePatient);
}

// 4. 解析行文本（用 strtok 按 | 拆分）
static void parsePatient(char* line, void* data) {
    char* token = strtok(line, "|");
    HIS_STRNCPY(p->id, token, sizeof(p->id));
    // ... 每个字段逐一解析
}
```

此模式在 patient.c 中重复了 **3 次**（Patient、MedicalRecord、Appointment），DoctorSchedule 的持久化现已由独立的 `schedule.c` 管理。体现了函数指针策略模式在纯 C 中的典型应用。

### 2. 患者身份验证的双通道机制

`verifyPatientPin` 实现了基于6位数字密码的访问控制：

```c
static int verifyPatientPin(Patient* p) {
    if (p->pin == 0) return 1;  // 未设置密码，跳过验证
    for (int tries = 0; tries < 3; tries++) {
        printf("请输入6位访问密码 (%d次尝试): ", 3 - tries);
        fgets(buf, sizeof(buf), stdin);
        if (atoi(buf) == p->pin) return 1;
    }
    printf("[错误] 密码验证失败已达上限，操作取消。\n");
    return 0;
}
```

设计要点：

- pin=0 表示未设置，向后兼容旧数据
- 3次尝试锁定，防止暴力破解
- 用在 `patientViewOnlyModule`、`viewMyRegistration`、`cancelMyRegistration`、`patientRecharge` 中

### 3. 身份查找与自动创建

`inputAndFindOrCreatePatient()` 实现了挂号场景下的便捷流程：

```c
static Patient* inputAndFindOrCreatePatient(void) {
    // 1. 先按 ID 查找
    ListNode* node = FindNode(patient_list, id_or_phone);
    if (node) return (Patient*)node->data;

    // 2. 再按手机号查找
    node = patient_list->head;
    while (node) {
        Patient* p = (Patient*)node->data;
        if (strcmp(p->phone, id_or_phone) == 0) return p;
    }

    // 3. 未找到 → 询问是否创建新患者
    // 4. 自动提取手机号、校验身份证、生成ID...
}
```

这种"输入即查找、找不到即创建"的设计减少了患者操作步骤，适合门诊自助挂号场景。

### 4. 挂号的完整事务流程

`normalRegistration()` 包含了一条完整的挂号链路：

```c
void normalRegistration(void) {
    // 1. 查找或创建患者
    // 2. 检查是否已有未完成的挂号
    // 3. 选择科室 → 选择医生（含号源检查）
    // 4. 计算费用（原价 - 医保报销）
    // 5. 确认挂号
    // 6. 检查余额
    // 7. 扣费 + 更新医生号源 + 创建挂号医疗记录
}
```

其中余额不足时退出、医生号源跨日自动重置（`checkAndResetDoctorDaily`）等细节体现了对医院实际业务流程的模拟。

### 5. 医生号源跨日重置机制

```c
static void checkAndResetDoctorDaily(Doctor* d) {
    char today[32];
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    snprintf(today, sizeof(today), "%04d-%02d-%02d",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    if (strcmp(d->register_date, today) != 0) {
        d->current_register = 0;     // 清零
        HIS_STRNCPY(d->register_date, today, sizeof(d->register_date)); // 更新日期
    }
}
```

在每次展示医生列表和挂号前调用，确保跨日号源自动恢复。

### 6. 医生端医疗记录管理的权限校验

`medicalRecordModule` 中的诊断和处方记录创建做了严格的三重校验：

```c
// 1. 患者存在
ListNode* pn = FindNode(patient_list, r.patient_id);
if (!pn) { printf("[错误] 患者不存在！\n"); break; }

// 2. 患者已挂号
if (((Patient*)pn->data)->register_status == REG_STATUS_NONE) { ... }

// 3. 患者挂的是当前医生的号
if (strcmp(((Patient*)pn->data)->doctor_id, r.doctor_id) != 0) { ... }
```

### 7. 删除患者的前置检查

`inputAndDeletePatient` 在删除前做了完整的关联检查（医疗记录、预约、住院、挂号），并采用"先备份 ID、再删除节点"的安全写法：

```c
// 安全复制姓名（在删除节点前）
char patient_name[MAX_NAME_LEN];
strncpy(patient_name, p->name, MAX_NAME_LEN - 1);
patient_name[MAX_NAME_LEN - 1] = '\0';

if (DeleteNode(patient_list, id) == 0) {
    printf("\n[成功] 患者 %s 已删除！\n", patient_name);  // 使用备份的姓名
}
```

---

## 四、优化修改记录

### 修改1：抽取公共校验函数 `validateDoctorPatientAccess`

`medicalRecordModule` 中 case 2（诊断）和 case 3（处方）的校验逻辑完全相同，包括患者存在性、挂号状态、医生匹配三重校验。抽取为公共函数后消除重复代码：

```c
static Patient* validateDoctorPatientAccess(const char* patient_id, const char* doctor_id) {
    ListNode* pn = FindNode(patient_list, patient_id);
    if (!pn) { printf("[错误] 患者不存在！\n"); return NULL; }
    Patient* p = (Patient*)pn->data;
    if (p->register_status == REG_STATUS_NONE) {
        printf("[提示] 该患者未挂号，请先完成挂号再创建医疗记录。\n");
        return NULL;
    }
    if (!FindNode(doctor_list, doctor_id)) {
        printf("[错误] 医生ID不存在！\n");
        return NULL;
    }
    if (strcmp(p->doctor_id, doctor_id) != 0) {
        printf("[提示] 该患者未挂此医生的号，无法创建医疗记录。\n");
        return NULL;
    }
    return p;
}
```

原 case 2/3 中各约12行的校验代码缩减为一行：

```c
if (!validateDoctorPatientAccess(r.patient_id, r.doctor_id)) break;
```

### 修改2：就诊状态流转硬约束

状态 `REG_STATUS_NONE → PENDING → IN_PROGRESS → DONE` 的流转现在由代码硬性保证：

```c
if (st == 1) {
    if (p->register_status != REG_STATUS_PENDING) {
        printf("[提示] 仅待就诊状态可转为就诊中。\n");
        break;
    }
    p->register_status = REG_STATUS_IN_PROGRESS;
}
else if (st == 2) {
    if (p->register_status != REG_STATUS_IN_PROGRESS) {
        printf("[提示] 仅就诊中状态可转为已完成。\n");
        break;
    }
    p->register_status = REG_STATUS_DONE;
}
```

### 修改3：取消挂号时对已存在诊断/处方的联动提醒

`cancelMyRegistration` 中取消现场挂号前，增加对已有诊断/处方记录的检查并给出提示：

```c
ListNode* rp = record_list->head;
while (rp) {
    MedicalRecord* r = (MedicalRecord*)rp->data;
    if (strcmp(r->patient_id, p->id) == 0 &&
        (r->type == RECORD_DIAGNOSIS || r->type == RECORD_PRESCR)) {
        printf("[警告] 该患者已有诊断/处方记录，取消挂号不会自动删除。\n");
        break;
    }
    rp = rp->next;
}
```

---

### 修改4：按功能拆分为三个独立编译单元

原2080行的 `patient.c` 按功能域拆分为三个文件：

| 文件               | 行数   | 职责                           |
| ---------------- | ---- | ---------------------------- |
| `patient.c`      | ~700 | 患者 CRUD + 患者自助充值/查看          |
| `registration.c` | ~520 | 挂号 + 预约 + 排班文件 I/O + 查看/取消挂号 |
| `record.c`       | ~460 | 医疗记录 CRUD + 医生工作站            |

拆分原则：

- **按功能域切割**：每个编译单元只负责单一业务领域
- **最小接口暴露**：仅 `isPhoneUsedByOther`、`isIDCardUsedByOther`、`getAgeFromIDCard`、`verifyPatientPin` 四个函数从 `static` 改为非静态并加入 `his.h`
- **零功能变更**：所有业务逻辑、菜单流程、数据持久化行为均保持不变
- **菜单透明**：`patientModule` 中的医疗记录子菜单通过函数调用委托至 `record.c`，对用户完全透明

---

## 五、数据持久化文件格式

| 实体   | 文件路径                   | 字段数 | 关键字段                                                               |
| ---- | ---------------------- | --- | ------------------------------------------------------------------ |
| 患者   | `data/patient.txt`     | 16  | `id\|name\|age\|gender\|...\|pin`                                  |
| 医疗记录 | `data/record.txt`      | 7   | `id\|patient_id\|doctor_id\|type\|cost\|detail\|create_time`       |
| 排班   | `data/schedule.txt`    | 8   | `id\|doctor_id\|dept_id\|date\|time_slot\|max\|current\|available` |
| 预约   | `data/appointment.txt` | 6   | `id\|patient_id\|schedule_id\|status\|create_time\|cost`           |

---

## 六、设计评价

### 优点

| 方面         | 评价                                        |
| ---------- | ----------------------------------------- |
| **业务覆盖全面** | 患者、医疗记录、挂号、预约、医生工作站五大域，功能完整               |
| **输入校验严格** | 身份证加权校验、手机号格式、年龄范围、唯一性检查                  |
| **事务完整性**  | 挂号涉及扣费、号源扣除、记录创建，保证数据一致                   |
| **向后兼容**   | parsePatient 对缺少 register_time/pin 的旧数据兼容 |
| **权限控制**   | 医生只能操作挂自己号的患者，患者访问需密码                     |

### 可改进点

| 方面         | 建议                                                                  | 状态                                          |
| ---------- | ------------------------------------------------------------------- | ------------------------------------------- |
| **文件体积**   | 原2080行，已拆分为 `patient.c`、`registration.c`、`record.c` 三个编译单元          | **已修复** → 每个文件约460~700行                     |
| **重复代码**   | `medicalRecordModule` 中 case 2（诊断）和 case 3（处方）的校验逻辑几乎相同，可抽取公共函数     | **已修复** → 抽取为 `validateDoctorPatientAccess` |
| **取消挂号回滚** | `cancelMyRegistration` 中退费逻辑可靠，但缺少对已产生的诊断/处方记录的联动处理                 | **已修复** → 取消前检测并给出联动提醒                      |
| **就诊状态流转** | 状态 `REG_STATUS_NONE → PENDING → IN_PROGRESS → DONE` 缺少硬约束，代码靠业务逻辑保证 | **已修复** → 添加了状态跳转校验                         |
