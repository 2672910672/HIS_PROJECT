# HIS 医院信息系统 — 项目分析报告

## 一、项目概述

**HIS** (Hospital Information System) 是一个基于 C99 标准的控制台医院信息管理系统，支持管理员、医生、患者三种角色，涵盖科室管理、医生管理、床位管理、患者管理、药品管理、挂号预约、医疗记录等核心业务流程。

- **语言标准**：C99
- **编译工具**：MSVC (Visual Studio 2022 / v143)
- **编译标志**：`/source-charset:utf-8`（源文件 UTF-8，执行字符集系统默认 CP936）
- **数据存储**：文本文件 (`data/*.txt`)，以 `|` 分隔字段
- **字符编码**：源文件 UTF-8，执行字符集 CP936

---

## 二、系统架构

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────┐
│                     his_main.c                          │
│                   (主程序/菜单调度/角色登录)               │
└──────────────────────┬──────────────────────────────────┘
                       │
        ┌──────────────┼──────────────┬──────────────────┐
        ▼              ▼              ▼                  ▼
┌──────────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────┐
│ dept_bed.c   │ │ doctor.c │ │ drug.c   │ │  admin_tools.c   │
│ 科室/床位管理  │ │ 医生管理  │ │ 药品管理  │ │ 全局统计/备份     │
└──────────────┘ └──────────┘ └──────────┘ └──────────────────┘
        │                                                   
        ├──────────────────────────────────────────────────┐
        ▼                                                  ▼
┌──────────────────────┐                     ┌──────────────────────┐
│    patient.c         │  ───→ record.c      │   registration.c    │
│ 患者管理/充值/只读查看  │      医疗记录管理    │  普通挂号/预约挂号    │
└──────────────────────┘                     └──────────────────────┘
        │                                             │
        ▼                                             ▼
┌──────────────────────┐                     ┌──────────────────────┐
│    schedule.c        │                     │   his_tool.c         │
│ 排班管理              │                     │ 工具函数/校验/文件I/O │
└──────────────────────┘                     └──────────────────────┘
                                                       │
                                                       ▼
                                               ┌──────────────────────┐
                                               │   his_link.c         │
                                               │ 通用链表(7个实例)     │
                                               └──────────────────────┘
```

### 2.2 模块依赖关系

```
his_config.h  ←  his.h  ←  (所有 .c 文件)
                          │
               ┌──────────┼──────────┐
               ▼          ▼          ▼
          his_tool.c  his_link.c  his_main.c
               │          │
               └──────────┘
               (全局链表变量跨模块共享)
```

### 2.3 全局链表架构

所有数据在内存中以 **单向通用链表** 形式存在，程序启动时从文件加载，退出时保存回文件：

```
全局变量 (his_main.c 中定义，extern 共享):
  ┌──────────────┬──────────────┬──────────────┐
  │ patient_list │ doctor_list  │  dept_list    │
  ├──────────────┼──────────────┼──────────────┤
  │  bed_list    │  drug_list   │ record_list   │
  ├──────────────┼──────────────┼──────────────┤
  │ schedule_list│appointment_li│              │
  └──────────────┴──────────────┴──────────────┘
```

**链表节点结构**：

```
LinkList
  ├── head: ListNode*
  └── length: int

ListNode
  ├── data: void*       ← malloc + memcpy 存储实体副本
  ├── data_size: int
  ├── id: char[MAX_ID_LEN]  ← 用于 FindNode 快速查找
  └── next: ListNode*
```

---

## 三、数据结构关系图

```
┌──────────────┐     ┌──────────────────┐     ┌──────────────┐
│  Department   │     │     Doctor       │     │     Bed      │
├──────────────┤     ├──────────────────┤     ├──────────────┤
│ id (K*****)  │◄────│ dept_id          │     │ id (B*****)  │
│ name         │     │ name             │◄────│ dept_id      │
│ doctor_count │     │ specialty        │     │ room_type    │
└──────────────┘     │ account          │     │ status       │
                     │ password(混淆)    │     │ patient_id   │
                     │ max_register     │     │ admit_time   │
                     │ current_register │     └──────┬───────┘
                     └──────┬───────────┘            │
                            │                        │
                     ┌──────▼───────────┐     ┌──────▼───────┐
                     │    Patient       │     │              │
                     ├──────────────────┤     │  (多对一)     │
                     │ id (P*****)      │     │              │
                     │ name             │     │              │
                     │ doctor_id        │─────┘              │
                     │ dept_id          │────────────────────┘
                     │ balance          │
                     │ register_status  │
                     │ is_inpatient     │
                     │ pin (访问密码)    │
                     │ phone/id_card    │
                     └──────┬───────────┘
                            │
                     ┌──────▼───────────┐     ┌──────────────┐
                     │  MedicalRecord   │     │   Drug       │
                     ├──────────────────┤     ├──────────────┤
                     │ patient_id       │     │ id (M*****)  │
                     │ doctor_id        │     │ general_name │
                     │ type (枚举)       │     │ trade_name   │
                     │ cost             │     │ price/stock  │
                     │ detail           │     │ warning_thr  │
                     │ create_time      │     │ dept_id      │
                     └──────────────────┘     └──────────────┘

┌──────────────┐     ┌──────────────────┐
│ DoctorSchedule│    │  Appointment     │
├──────────────┤     ├──────────────────┤
│ id (S*****)  │     │ id (A*****)      │
│ doctor_id    │◄────│ schedule_id      │
│ dept_id      │     │ patient_id       │
│ date         │     │ status(预约/取消) │
│ time_slot    │     │ create_time      │
│ max_patients │     │ cost             │
│ current_ptnts│     └──────────────────┘
│ is_available │
└──────────────┘
```

**关联关系**：

- `Doctor.dept_id` → `Department.id`（多对一：一个科室有多名医生）
- `Bed.dept_id` → `Department.id`（多对一：一个科室有多个床位）
- `Patient.doctor_id` → `Doctor.id`（多对一：一个医生有多个患者）
- `Patient.dept_id` → `Department.id`（多对一）
- `MedicalRecord.patient_id` → `Patient.id`（多对一：一个患者有多条记录）
- `MedicalRecord.doctor_id` → `Doctor.id`（多对一）
- `DoctorSchedule.doctor_id` → `Doctor.id`（多对一）
- `Appointment.schedule_id` → `DoctorSchedule.id`（多对一）

---

## 四、模块详解

### 4.1 `his_main.c` — 主程序入口 / 菜单调度

**职责**：初始化和释放全局链表、加载和保存全部数据、角色登录验证、菜单路由。

| 函数                  | 说明                              |
| ------------------- | ------------------------------- |
| `initGlobalLists()` | 初始化所有 8 个链表                     |
| `freeGlobalLists()` | 释放所有链表内存                        |
| `loadAllHisData()`  | 调用各模块的 load 函数加载数据              |
| `saveAllHisData()`  | 退出前保存全部数据                       |
| `adminLogin()`      | 管理员登录（硬编码账号 admin/123456，5 次锁定） |
| `doctorLogin()`     | 医生登录（密码混淆比对 + 明文兼容迁移，登录后设置会话）   |
| `adminMenu()`       | 管理员菜单 → 患者/科室医生床位/药品/修改密码       |
| `doctorMenu()`      | 医生菜单 → 查看患者/医疗记录/预约/排班/个人信息/改密  |
| `patientMenu()`     | 患者菜单 → 挂号/预约/查看/取消/充值           |

### 4.2 登录流程详解

#### 管理员登录流程

```
┌─────────────────┐
│   adminLogin()  │
└────────┬────────┘
         │
         ▼
┌─────────────────────┐
│ fail_count >= 5 ?   │──── YES ──→ [锁定] 返回 0
└────────┬────────────┘
         │ NO
         ▼
┌─────────────────────┐
│  输入 username      │
│  输入 password      │
└────────┬────────────┘
         │
         ▼
┌─────────────────────────────────┐
│ strcmp(username, "admin") == 0 │
│ &&                              │── YES ──→ fail_count=0, 返回 1
│ strcmp(password, "123456") == 0 │
└────────┬────────────────────────┘
         │ NO
         ▼
┌─────────────────────┐
│ fail_count++        │
│ 提示第 n/5 次尝试    │──→ 返回 0
└─────────────────────┘
```

**特点**：管理员账号硬编码在 `his_config.h`；密码支持运行时修改（`changeAdminPassword()`），持久化存储在 `data/admin.dat`；`fail_count` 为 `static` 变量，锁定后需要重启程序。

#### 医生登录流程

```
┌─────────────────────┐
│   doctorLogin()     │
└────────┬────────────┘
         │
         ▼
┌─────────────────────┐
│ fail_count >= 5 ?   │──YES──→ [锁定] 返回 0
└────────┬────────────┘
         │
         ▼
┌─────────────────────┐
│  doctor_list 为空?   │──YES──→ [提示无数据] 返回 0
└────────┬────────────┘
         │
         ▼
┌─────────────────────┐
│ 输入 username       │
│ 输入 password       │
│ passwordObfuscate() │
│   ↓                 │
│ password变成混淆版本 │
└────────┬────────────┘
         │
         ▼
  ┌──────────────────┐
  │ 遍历 doctor_list  │
  └────────┬─────────┘
           │
   ┌───────┴───────┐
   │ d->account == │ YES
   │ username ?    │──────┐
   └───────┬───────┘      │
           │ NO           ▼
           │     ┌─────────────────┐
           │     │ d->password ==  │ YES──→ 登录成功
           │     │ password(混淆)? │
           │     └────────┬────────┘
           │              │ NO
           │              ▼
           │     ┌─────────────────────────┐
           │     │ 明文兼容迁移路径          │
           │     │ check = d->password     │
           │     │ obfuscate(check)        │
           │     │ check == password(混淆)?│──YES─→ 迁移+登录成功
           │     └────────┬────────────────┘
           │              │ NO
           │              ▼
           │        继续下一个节点
           │
           ▼
    遍历结束，无匹配
    fail_count++
    提示登录失败
```

**登录成功后设置会话**：

```c
g_current_doctor_id   = d->id;    // 供各模块跨函数使用
g_current_doctor_name = d->name;  // 菜单标题显示
// 医生菜单退出时自动清空这两个全局变量
```

**会话跟踪意义**：医生登录后的所有操作（查看患者、创建记录、修改状态）不再需要手动输入医生ID，直接从全局变量读取，避免身份冒用。

**关键代码 — 明文兼容迁移**：

```c
// 兼容旧数据：若密码仍为明文存储，尝试明文比对并迁移
if (d && strcmp(d->account, username) == 0) {
    char check[MAX_PWD_LEN];
    HIS_STRNCPY(check, d->password, MAX_PWD_LEN);
    passwordObfuscate(check);  // nibble-swap(d->password)
    if (strcmp(check, password) == 0) {
        // d->password 是明文 → 替换为混淆版本
        HIS_STRNCPY(d->password, password, MAX_PWD_LEN);
        saveDoctorData();      // 立即保存迁移结果
        // 登录成功
    }
}
```

**为什么需要这个迁移**：最初版本在 `loadDoctorData()` 中通过 `isprint` 判断密码是否明文，但 nibble-swap 后部分字符仍可打印（如 `'3'`→`'3'`、`'R'`→`'%'`），导致重启后反复混淆↔还原。修复后迁移移至登录时按需处理。

---

### 4.3 `dept_bed.c` — 科室 / 床位管理

**职责**：科室 CRUD、床位 CRUD（含住院出院）、统计查询。医生管理和排班管理已分别拆分至 `doctor.c` 和 `schedule.c`。

> 医生管理（`addDoctor`、`modifyDoctor`、`deleteDoctor`、`queryDoctor`）现已拆分至独立的 `doctor.c` 模块。详情请参考 `doctor.c`。

```c
while (1) {
    printf("请输入医生姓名: ");
    if (!fgets(d->name, MAX_NAME_LEN, stdin)) { ClearInputBuffer(); continue; }
    nl = strchr(d->name, '\n');
    if (nl) *nl = '\0';          // 正常读到\n
    else {
        ClearInputBuffer();      // 输入超长，清空残留
        // GBK截断保护：末尾字节若为GBK首字节则移除
        size_t _len = strlen(d->name);
        if (_len > 0 && (unsigned char)d->name[_len - 1] >= 0x81)
            d->name[_len - 1] = '\0';
    }
    if (!ValidateNoPipe(d->name)) { printf("[错误] 姓名不能包含分隔符'|'。\n"); continue; }
    if (strlen(d->name) == 0) { printf("[错误] 姓名不能为空！\n"); continue; }
    break;
}
```

**关键代码 — 账号唯一性检查**：

```c
// 遍历检查账号是否已存在
ListNode* p = doctor_list->head;
while (p) {
    Doctor* existing = (Doctor*)p->data;
    if (strcmp(existing->account, d.account) == 0) {
        printf("\n[错误] 登录账号 '%s' 已被使用！\n", d.account);
        return;
    }
    p = p->next;
}
```

**修改医生时密码可选保留**：

```c
printf("请输入登录密码（直接回车则不更改）: ");
if (fgets(pwd_buf, MAX_PWD_LEN, stdin)) {
    nl = strchr(pwd_buf, '\n');
    if (nl) *nl = '\0';
    else ClearInputBuffer();
    if (strlen(pwd_buf) > 0) {
        HIS_STRNCPY(d->password, pwd_buf, MAX_PWD_LEN);
        passwordObfuscate(d->password);
    } // 空输入 → 保留旧密码 (d->password 不变)
}
```

**修改医生时账号唯一性（排除自身）**：

```c
ListNode* p = doctor_list->head;
while (p) {
    Doctor* existing = (Doctor*)p->data;
    if (existing != d && strcmp(existing->account, d->account) == 0) {
        // 冲突 → 恢复旧账号和旧密码
        HIS_STRNCPY(d->account, old_account, MAX_NAME_LEN);
        HIS_STRNCPY(d->password, old_pwd, MAX_PWD_LEN);
        return;
    }
    p = p->next;
}
```

#### 床位管理 — 住院/出院流程

```
┌──────────────────┐
│ modifyBedStatus()│
└───────┬──────────┘
        │
        ▼
┌──────────────────┐
│ 输入床位ID        │
│ FindNode 查找床位  │
└───────┬──────────┘
        │
        ▼
   ┌────────┐
   │ status │
   │ = ?    │
   └───┬────┘
       │
  ┌────┴────┐
  │ BED_FREE│ ────────────────── 办理住院 ──────────────────┐
  └─────────┘                                               │
       │                                                    │
       ▼                                                    │
  ┌──────────────────┐                                      │
  │ 输入患者ID         │                                      │
  │ FindNode 查找患者  │                                      │
  └───────┬──────────┘                                      │
          │                                                  │
          ▼                                                  │
  ┌──────────────────┐                                      │
  │ is_inpatient ==  │──YES──→ [错误] 患者已住院              │
  │ PATIENT_IN ?     │                                      │
  └───────┬──────────┘                                      │
          │ NO                                               │
          ▼                                                  │
  ┌──────────────────┐                                      │
  │ bed->status =    │                                      │
  │   BED_OCCUPIED   │                                      │
  │ bed->patient_id  │                                      │
  │ GetSystemTime()  │                                      │
  │ patient->is_inp  │                                      │
  │   atient = IN    │                                      │
  │ patient->bed_id  │                                      │
  │ saveBedData()    │                                      │
  │ savePatientData()│                                      │
  └──────────────────┘                                      │
                                                             │
  ┌──────────────┐                                           │
  │BED_OCCUPIED  │ ───────────────── 办理出院 ──────────────┘
  └──────────────┘
       │
       ▼
  ┌──────────────────┐
  │ 确认? (y/n)       │──NO──→ 取消
  └───────┬──────────┘
          │ YES
          ▼
  ┌──────────────────┐
  │ patient->is_inp  │
  │   atient = OUT   │
  │ patient->bed_id  │ = "-1"
  │ bed->status =    │
  │   BED_FREE       │
  │ bed->patient_id  │ = "-1"
  │ bed->admit_time  │ = ""
  │ saveBedData()    │
  │ savePatientData()│
  └──────────────────┘
```

---

### 4.4 `patient.c` — 患者管理 / 挂号 / 预约 / 医疗记录

**职责**：患者 CRUD、普通挂号、预约挂号、医疗记录管理、医生端功能、患者自助查看/充值。

#### 普通挂号完整流程

```
┌──────────────────────┐
│ normalRegistration() │
└───────┬──────────────┘
        │
        ▼
┌──────────────────────────────────┐
│ inputAndFindOrCreatePatient()    │
│                                  │
│  ┌────────────────────────────┐  │
│  │ 输入ID或手机号              │  │
│  │ 先按ID找 → 找到则返回       │  │
│  │ 再按手机号找 → 找到则返回   │  │
│  │ 未找到 → 询问是否创建新患者  │  │
│  │ 是 → 输入信息 → InsertNode  │  │
│  └────────────────────────────┘  │
└───────┬──────────────────────────┘
        │
        ▼
┌──────────────────────┐
│ 已挂号?               │──YES──→ [提示] 先取消再挂号
│ register_status != 0 │
└───────┬──────────────┘
        │ NO
        ▼
┌──────────────────────┐
│ selectDeptAndDoctor() │
│                      │
│  ┌────────────────┐  │
│  │ 显示科室列表     │  │
│  │ 用户选编号       │  │
│  │ 显示该科室医生   │  │
│  │ 检查号源是否满   │  │
│  │ 用户选医生       │  │
│  └────────────────┘  │
└───────┬──────────────┘
        │
        ▼
┌──────────────────────┐
│ 显示挂号费用          │
│ 医保报销后实际支付:   │
│ pay=cost*(1-ratio)   │
│ 确认? (y/n)          │──NO──→ 取消
└───────┬──────────────┘
        │ YES
        ▼
┌──────────────────────┐
│ 余额 >= pay ?         │──NO──→ [错误] 余额不足，请充值
└───────┬──────────────┘
        │ YES
        ▼
┌──────────────────────────────────────┐
│ 执行挂号                              │
│                                      │
│ p->balance -= pay                    │
│ p->doctor_id = doctor_id             │
│ p->dept_id = dept_id                 │
│ p->register_status = REG_STATUS_PENDING│
│ GetSystemTime(p->register_time)      │
│ d->current_register++                │
│ savePatientData()                    │
│ saveDoctorData()                     │
│                                      │
│ 创建 MedicalRecord (RECORD_REGISTER)  │
│ InsertNode → saveRecordData()        │
└──────────────────────────────────────┘
```

**关键代码 — 选择科室和医生**：

```c
static int selectDeptAndDoctor(char* out_dept_id, char* out_doctor_id) {
    // 显示科室列表（编号方式）
    int index = 1;
    ListNode* dn = dept_list->head;
    while (dn) {
        Department* dept = (Department*)dn->data;
        printf("  %d. %s (ID: %s)\n", index, dept->name, dept->id);
        index++; dn = dn->next;
    }
    int dept_choice = getValidChoice(0, dept_list->length);
    if (dept_choice == 0) return -1;

    // 显示该科室下医生（含剩余号源）
    int doc_count = 0;
    ListNode* docn = doctor_list->head;
    while (docn) {
        Doctor* d = (Doctor*)docn->data;
        if (strcmp(d->dept_id, selected_dept->id) == 0) {
            doc_count++;
            printf("  %d. %s | 擅长: %s | 剩余号源: %d/%d\n",
                doc_count, d->name, d->specialty,
                d->max_register - d->current_register,
                d->max_register);
        }
        docn = docn->next;
    }
    // ...用户选择医生→检查号源→返回
}
```

**关键代码 — 患者查找或自动创建**：

```c
static Patient* inputAndFindOrCreatePatient(void) {
    printf("请输入患者ID或手机号: ");
    fgets(id_or_phone, MAX_ID_LEN, stdin);
    // 1. 先按ID查找
    ListNode* node = FindNode(patient_list, id_or_phone);
    if (node) return (Patient*)node->data; // 找到即返回

    // 2. 再按手机号遍历查找
    node = patient_list->head;
    while (node) {
        Patient* p = (Patient*)node->data;
        if (strcmp(p->phone, id_or_phone) == 0) return p;
        node = node->next;
    }

    // 3. 未找到→创建新患者
    printf("是否创建新患者？(y/n): ");
    if (buf[0] != 'y' && buf[0] != 'Y') return NULL;
    // ...输入个人信息、生成ID、InsertNode...
}
```

#### 预约挂号流程

```
┌───────────────────────────┐
│ appointmentRegistration() │
└──────────┬────────────────┘
           │
           ▼
┌───────────────────────────┐
│ inputAndFindOrCreatePatient│
└──────────┬────────────────┘
           │
           ▼
┌───────────────────────────┐
│ selectDeptAndDoctor()     │
│ → 得到 dept_id, doctor_id │
└──────────┬────────────────┘
           │
           ▼
┌──────────────────────────────────────┐
│ 查找医生的可用排班                     │
│ 遍历 schedule_list                   │
│ 条件: s->doctor_id == doctor_id      │
│      && s->is_available              │
│      && s->current_patients < max    │
│ 显示可选排班列表                       │
└──────────┬───────────────────────────┘
           │
           ▼
┌───────────────────────────────┐
│ 用户选择排班编号                │
│ 确认预约、支付费用              │
│ 余额 >= pay ? ──NO──→ 错误    │
└──────────┬────────────────────┘
           │ YES
           ▼
┌───────────────────────────────┐
│ 执行预约                       │
│ p->balance -= pay             │
│ 创建 Appointment 记录          │
│   status="已预约"              │
│   schedule_id = 选中排班ID     │
│ selected_schedule->           │
│   current_patients++          │
│ saveAppointmentData()         │
│ saveScheduleData()            │
│ savePatientData()             │
└───────────────────────────────┘
```

#### 医疗记录管理（医生端）

```
┌────────────────────┐
│ medicalRecordModule│
└──────┬─────────────┘
       │
       ├── 1. 查看患者医疗记录
       │      → displayPatientRecords(patient_id)
       │      → 遍历 record_list，筛选 patient_id 匹配
       │      → 关联显示医生姓名
       │
       ├── 2. 新增诊断记录
       │      → 输入患者ID → FindNode 校验存在
       │      → 校验患者已挂号 (register_status != NONE)
       │      → 输入医生ID → FindNode 校验
       │      → 校验患者挂的是该医生的号
       │      → 输入费用、详情 → InsertNode → save
       │
       ├── 3. 新增处方记录
       │      → 与诊断记录类似，type=RECORD_PRESCR
       │
       ├── 4. 修改就诊状态
       │      → 输入患者ID → FindNode
       │      → register_status: 待就诊→就诊中→已完成
       │
       └── 0. 返回
```

---

### 4.5 `drug.c` — 药品管理 / 库存 / 发药 / 统计

**职责**：药品 CRUD、入库出库、库存预警、门诊发药（含医保抵扣）。全局查询统计与数据备份已拆分至 `admin_tools.c`。

#### 门诊发药完整流程

```
┌────────────────────┐
│ issuePrescription()│
└──────┬─────────────┘
       │
       ▼
┌──────────────────────┐
│ 输入患者ID            │
│ FindNode 校验患者      │──NO──→ [错误] 患者不存在
└──────┬───────────────┘
       │ YES
       ▼
┌──────────────────────┐
│ 输入药品ID            │
│ FindNode 校验药品      │──NO──→ [错误] 药品不存在
└──────┬───────────────┘
       │ YES
       ▼
┌──────────────────────┐
│ 输入发药数量           │
│ stock >= quantity ?   │──NO──→ [失败] 库存不足
└──────┬───────────────┘
       │ YES
       ▼
┌──────────────────────┐
│ 输入医生ID            │
│ FindNode 校验医生      │──NO──→ [错误] 医生不存在
│ p->doctor_id == doc ? │──NO──→ [错误] 患者未挂该医生号
└──────┬───────────────┘
       │ YES
       ▼
┌──────────────────────────────────────┐
│ 计算费用                              │
│ total_cost = d->price * quantity     │
│ insurance_pay = total_cost * ratio   │
│ patient_pay = total_cost - insurance │
│                                      │
│ 显示费用明细                          │
│ 患者当前余额: p->balance              │
│ balance >= patient_pay ? ──NO──→ 余额不足
└──────┬───────────────────────────────┘
       │ YES
       ▼
┌──────────────────────┐
│ 确认? (y/n) ──NO──→ 取消
└──────┬───────────────┘
       │ YES
       ▼
┌──────────────────────────────────────────┐
│ 执行发药                                  │
│ 1. d->stock -= quantity (扣库存)          │
│ 2. p->balance -= patient_pay (扣余额)      │
│ 3. 创建 MedicalRecord                     │
│      type = RECORD_PRESCR                 │
│      detail = "门诊发药: 阿莫西林 x2, ..."  │
│      InsertNode → saveRecordData()        │
│ 4. p->record_count++                      │
│ 5. saveDrugData() + savePatientData()     │
│ 6. 如果 stock < warning → 预警提示          │
└──────────────────────────────────────────┘
```

**关键代码 — 医保计算与执行**：

```c
// 费用计算
float total_cost = d->price * quantity;
float insurance_pay = total_cost * p->insurance_ratio;
float patient_pay = total_cost - insurance_pay;

// 余额校验
if (p->balance < patient_pay) {
    printf("[失败] 患者余额不足！需自付 %.2f 元，当前余额 %.2f 元\n",
        patient_pay, p->balance);
    return;
}

// 执行发药
d->stock -= quantity;
p->balance -= patient_pay;

// 生成医疗记录
MedicalRecord r;
memset(&r, 0, sizeof(MedicalRecord));
GenerateID(r.id, ID_PREFIX_RECORD);
HIS_STRNCPY(r.patient_id, patient_id, MAX_ID_LEN);
HIS_STRNCPY(r.doctor_id, doctor_id, MAX_ID_LEN);
r.type = RECORD_PRESCR;
r.cost = total_cost;
snprintf(r.detail, MAX_DETAIL_LEN, "门诊发药: %s x%d, 医保报销%.2f元",
    d->general_name, quantity, insurance_pay);
GetSystemTime(r.create_time);
InsertNode(record_list, -1, &r, sizeof(MedicalRecord), r.id);
p->record_count++;

saveDrugData();
savePatientData();
saveRecordData();
```

#### 入库出库流程

```
入库 (drugInbound):                           出库 (drugOutbound):
─────────────────                             ──────────────────
输入药品ID → FindNode                         输入药品ID → FindNode
输入入库数量 (+正整数)                          输入出库数量 (+正整数)
d->stock += quantity                          stock >= quantity ? ──NO──→ 库存不足
[预警] stock < threshold ? → 警告               d->stock -= quantity
saveDrugData()                                [预警] stock < threshold ? → 警告
                                              saveDrugData()
```

---

### 4.6 `his_tool.c` — 工具函数库

#### 密码混淆流程 (Nibble-Swap)

```
原始密码:  "abc123"
                    ┌─────────────┐
ASCII:  a(0x61)     │ 高4位 ↔ 低4位 │
        0110 0001   │  → 0001 0110 │ → 0x16 (不可打印)
                    └─────────────┘
         b(0x62)    │  → 0010 0110 │ → 0x26 (不可打印)
         c(0x63)    │  → 0011 0110 │ → 0x36 ('6' 可打印!)
         1(0x31)    │  → 0001 0011 │ → 0x13 (不可打印)
         2(0x32)    │  → 0010 0011 │ → 0x23 ('#' 可打印!)
         3(0x33)    │  → 0011 0011 │ → 0x33 ('3' 可打印!)

混淆后:  0x16 0x26 0x36 0x13 0x23 0x33
```

**nibble-swap 自逆性证明**：`f(f(x)) = x`

```
对字节 x = (高4位H, 低4位L)
f(x) = (L, H)        ← 高低4位交换
f(f(x)) = f(L, H) = (H, L) = x    ← 两次操作还原
```

```c
void passwordObfuscate(char* pwd) {
    if (!pwd) return;
    for (int i = 0; pwd[i]; i++) {
        pwd[i] = ((pwd[i] << 4) | ((unsigned char)pwd[i] >> 4));
    }
}
```

**实际调用场景**：

```
addDoctor: 输入明文密码 → obfuscate → 存文件 (混淆状态)
doctorLogin: 输入密码 → obfuscate → 比对文件中已混淆的密码
明文迁移: 将文件中明文密码 obfuscate 后比对 → 匹配则覆盖为混淆版
```

#### 文件写入流程 (原子保存)

```
┌──────────────┐
│SaveDataToFile│
└──────┬───────┘
       │
       ▼
┌──────────────────┐
│ 构造临时文件名     │
│ snprintf("%s.tmp")│
└──────┬───────────┘
       │
       ▼
┌──────────────────┐
│ fopen(tmp, "w")  │──失败→ 返回-1
└──────┬───────────┘
       │
       ▼
┌───────────────────────────────┐
│ 遍历链表, format_func 逐行写入 │
│ while (p) {                   │
│   format_func(p->data, line); │
│   fprintf(fp, "%s\n", line);  │
│   p = p->next;                │
│ }                             │
└──────┬────────────────────────┘
       │
       ▼
┌──────────────────┐
│ fclose(fp)       │
│ remove(filename) │
│ rename(tmp, file)│──失败→ 返回-1
└──────────────────┘

             ★ 关键: rename 是原子操作
             如果 rename 前程序崩溃 → tmp 文件残留, 原文件完好
```

#### 文件加载流程

```
┌────────────────┐
│LoadDataFromFile│
└──────┬─────────┘
       │
       ▼
┌──────────────────┐
│ fopen(file, "r") │──失败→ 返回-1 (文件不存在是正常情况)
└──────┬───────────┘
       │
       ▼
┌──────────────────────────────────────┐
│ while (fgets(line, sizeof, fp)) {    │
│   line[strcspn(line, "\n")] = '\0';  │
│   // 去除 \r (Windows兼容)            │
│   if (line[len-1] == '\r')           │
│       line[len-1] = '\0';            │
│   // 跳过空行/无效行                  │
│   if (空行 或 |开头 或 全部|) continue │
│                                      │
│   data = malloc(MAX_DATA_SIZE);      │
│   parse_func(line, data);            │
│   // ID有效性检查                     │
│   if (strlen(parsed_id) < 4) {       │
│       free(data); continue;          │
│   }                                  │
│   InsertNode(list, -1, data, ..., id)│
│   free(data);                        │
│ }                                    │
└──────┬───────────────────────────────┘
       │
       ▼
┌──────────────────┐
│ fclose(fp);      │
│ return 0;        │
└──────────────────┘
```

#### 菜单输入处理流程

```
┌──────────────────────────────────┐
│ getValidChoice(min, max)         │
│                                  │
│ while (1) {                      │
│   fgets(buf, 64, stdin);         │ ← 读整行(含\n)
│   buf[strcspn(buf,"\n")]='\0';  │ ← 去除\n
│                                  │
│   // 校验: 是否空?               │
│   if (len==0) → 重新输入          │
│                                  │
│   // 校验: 是否全数字?            │
│   for (每个字符)                  │
│     if (!数字) → 重新输入          │
│                                  │
│   // 校验: 是否在[min,max]范围?   │
│   choice = atoi(buf);            │
│   if (range_ok) → return choice; │
│   else → 重新输入                 │
│ }                                │
└──────────────────────────────────┘
```

对比传统 `scanf("%d", &choice)` 的问题：

```
scanf 方式:                    getValidChoice 方式:
┌──────────────────┐          ┌──────────────────────┐
│ 输入 "abc\n"     │          │ 输入 "abc\n"          │
│ scanf 返回 0     │          │ fgets 读到 "abc\n"    │
│ 'a','b','c' 残留 │          │ 检测到非数字 → 重新输入│
│ 下次 scanf 又失败│          │ 缓冲区已清空，不影响   │
│ ...死循环        │          │ 下一轮输入正常         │
└──────────────────┘          └──────────────────────┘
```

#### ClearInputBuffer 与 fgets 溢出检测

```c
// 清空 stdin 缓冲区
void ClearInputBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}
```

使用场景：

```
fgets(buf, size, stdin)
  → strchr(buf, '\n') != NULL ?    输入正常, 替换\n为\0
  → strchr(buf, '\n') == NULL ?    输入超长
    → ClearInputBuffer() 清空残留
    → 检查末尾是否截断中文字符
```

---

### 4.7 `his_link.c` — 通用链表实现

#### InsertNode 内部流程

```
┌──────────────────────┐
│ InsertNode(list, idx)│
└──────┬───────────────┘
       │
       ▼
┌──────────────────────────────┐
│ 参数校验: list/data/id 非空   │
│ index 在 [-1, length] 范围    │
└──────┬───────────────────────┘
       │
       ▼
┌──────────────────────────────┐
│ malloc ListNode              │
│ malloc data_size (深拷贝)     │
│ memcpy 复制数据到新内存        │
│ strncpy 复制 id              │
└──────┬───────────────────────┘
       │
       ▼
 ┌──────────────────────┐
 │ index = 0 或空链表?   │──YES──→ 头插法: new->next = head; head = new
 └─────────┬────────────┘
           │ NO
 ┌─────────┴────────────┐
 │ index = -1 (尾插)?    │──YES──→ 遍历到末尾插入
 └─────────┬────────────┘
           │ NO
 ┌─────────┴────────────┐
 │ 指定位置插入           │
 │ 遍历到 index-1 位置   │
 │ new->next = p->next  │
 │ p->next = new        │
 └──────────────────────┘
           │
           ▼
┌──────────────────────┐
│ list->length++       │
│ return 0             │
└──────────────────────┘
```

**深拷贝示意图**：

```
InsertNode(list, -1, &d, sizeof(Doctor), d.id)

链表节点:                             Doctor d (栈上):
┌──────────────┐                     ┌──────────────┐
│ ListNode     │                     │ id="D26051.."│
│  ┌────────┐  │    memcpy           │ name="张三"  │
│  │ data ──┼──┼────────────────────→│ specialty..  │
│  │ size   │  │  sizeof(Doctor)     │ account..    │
│  │ id     │  │                     │ password..   │
│  │ next ──┼──┼────→ ...           │ max_reg..    │
│  └────────┘  │                     └──────────────┘
│ data指向     │
│ malloc新内存  │
└──────────────┘
```

---

## 五、校验逻辑汇总

| 校验类型         | 位置                                           | 规则                                                |
| ------------ | -------------------------------------------- | ------------------------------------------------- |
| **手机号**      | `ValidatePhone()` + `isPhoneUsedByOther()`   | 11 位、全数字、以 `1` 开头 + 全院唯一                          |
| **身份证**      | `ValidateIDCard()` + `isIDCardUsedByOther()` | 18 位、前 17 位数字、末位数字/X、加权校验和 (GB 11643-1999) + 全院唯一 |
| **身份证年龄一致性** | `getAgeFromIDCard()`                         | 从身份证提取出生日期计算年龄，与录入年龄比对，不一致则拦截                     |
| **姓名/科室名**   | `inputDoctorInfo()` / `inputDeptInfo()`      | 非空、无 `                                            |
| **医生账号**     | `addDoctor()` / `modifyDoctor()`             | 非空、无 `                                            |
| **医生密码**     | `addDoctor()`                                | 非空                                                |
| **医生特长**     | `inputDoctorInfo()`                          | 无 `                                               |
| **菜单输入**     | `getValidChoice()`                           | 数字、范围内、fgets 整行读取                                 |
| **性别**       | `inputPatientBasicInfo()`                    | 只能是 `男` 或 `女`                                     |
| **年龄**       | `inputPatientBasicInfo()`                    | 0~150 整数                                          |
| **医保比例**     | `inputPatientBasicInfo()`                    | 0.0~1.0 浮点数                                       |
| **余额/充值**    | `patientRecharge()`                          | 单次 ≤10 万、总额 ≤50 万                                 |
| **ID 唯一性**   | `GenerateID()` + `FindNode()`                | 时间戳+序号，重试最多 10 次                                  |
| **号源限制**     | `selectDeptAndDoctor()`                      | current_register < max_register                   |
| **库存预警**     | `drugInbound/Outbound/issuePrescription()`   | stock < warning_threshold 时警告                     |
| **发药前校验**    | `issuePrescription()`                        | 患者存在、药品存在、库存够、医生匹配、余额足                            |

---

## 六、特色代码与关键设计

### 6.1 身份证校验 (GB 11643-1999)

```c
int ValidateIDCard(const char* id_card) {
    if (!id_card || strlen(id_card) != 18) return 0;
    // 前17位必须是数字
    for (size_t i = 0; i < 17; i++)
        if (id_card[i] < '0' || id_card[i] > '9') return 0;
    // 末位数字或X/x
    char last = id_card[17];
    if (!(last >= '0' && last <= '9') && last != 'X' && last != 'x') return 0;

    // 加权求和 (GB 11643-1999)
    static const int weights[17] = { 7,9,10,5,8,4,2,1,6,3,7,9,10,5,8,4,2 };
    static const char check_chars[11] = "10X98765432";
    int sum = 0;
    for (size_t i = 0; i < 17; i++)
        sum += (id_card[i] - '0') * weights[i];

    char expected_check = check_chars[sum % 11];
    char actual_last = (last >= 'a' && last <= 'z') ? last - 'a' + 'A' : last;
    return actual_last == expected_check;
}
```

加权系数与校验字符表来自国家标准 GB 11643-1999。

### 6.2 ID 生成策略

```c
void GenerateID(char* id, char type) {
    static int seq = 1;
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    snprintf(id, MAX_ID_LEN, "%c%02d%02d%02d%03d",
        type, tm->tm_year % 100, tm->tm_mon + 1, tm->tm_mday, seq++);
}
```

格式：`[前缀][年2位][月2位][日2位][序号3位]`
示例：`P260511001`（患者，2026-05-11，第 1 号）

**前缀规则**：

- `P` = 患者, `D` = 医生, `K` = 科室
- `B` = 床位, `M` = 药品, `R` = 记录
- `A` = 预约, `S` = 排班

### 6.3 安全字符串拷贝宏

```c
#define HIS_STRNCPY(dst, src, cap) do { \
    char* _his_d = (dst); \
    size_t _his_c = (size_t)(cap); \
    const char* _his_s = (src); \
    if (_his_d && _his_c > 0U) { \
        strncpy(_his_d, (_his_s) ? (_his_s) : "", _his_c - 1U); \
        _his_d[_his_c - 1U] = '\0'; \
    } \
} while (0)
```

保证目标缓冲区始终以 `\0` 结尾（`strncpy` 在源字符串 >= 目标大小时不自动添加终止符）。

### 6.4 预约与挂号的双轨制

系统同时支持两种就医模式：

```
普通挂号                              预约挂号
  │                                     │
  ▼                                     ▼
现场选择科室医生                      选择科室医生
  │                                     │
  ▼                                     ▼
直接就诊 (register_status = PENDING)  选择排班时间段
  │                                  创建 Appointment (status="已预约")
  ▼                                     │
创建 MedicalRecord (RECORD_REGISTER)    ▼
  │                                  创建 MedicalRecord (RECORD_REGISTER)
  ▼                                     │
医生可以查看挂自己号的患者             医生通过 queryMyAppointment 查看预约
```

### 6.5 患者 PIN 码保护

```c
static int verifyPatientPin(Patient* p) {
    if (p->pin == 0) return 1;  // 未设置密码，跳过验证
    for (int tries = 0; tries < 3; tries++) {
        printf("请输入6位访问密码 (%d次尝试): ", 3 - tries);
        fgets(buf, sizeof(buf), stdin);
        if (atoi(buf) == p->pin) return 1;
        printf("[错误] 密码错误！\n");
    }
    printf("[错误] 密码验证失败已达上限，操作取消。\n");
    return 0;
}
```

用于患者查看自己的信息、充值、取消挂号的场景，保护患者隐私。

### 6.6 两种文件解析模式对比

**模式一**：`sscanf` + `%[^|]`

```c
sscanf(line, "%[^|]|%[^|]|%d", d->id, d->name, &d->doctor_count);
// 用于: dept/doctor/bed/drug (字段较少，固定)
```

| 优点        | 缺点                          |
| --------- | --------------------------- |
| 代码简洁，一行解析 | 不能处理字段内的 `                  |
| 无需修改原字符串  | GBK trail byte = 0x7C 时解析错位 |
| 类型安全      | 不支持可选字段                     |

**模式二**：`strtok` 逐字段

```c
char* token = strtok(line, "|");
if (!token) return;
HIS_STRNCPY(p->id, token, sizeof(p->id));
token = strtok(NULL, "|");
// ...逐字段解析
// 用于: patient/record/schedule/appointment (字段多，有可选字段)
```

| 优点               | 缺点              |
| ---------------- | --------------- |
| 灵活，支持可选字段        | `strtok` 修改原字符串 |
| 可处理字段内的 `        | `（需要其他转义）       |
| 向后兼容（新增字段不影响旧数据） | 需逐字段手动赋值        |

---

## 七、数据持久化

### 7.1 文件格式概览

| 数据文件              | 字段数 | 示例          |
| ----------------- | --- | ----------- |
| `patient.txt`     | 16  | `P260511001 |
| `doctor.txt`      | 8   | `D260511001 |
| `dept.txt`        | 3   | `K001       |
| `bed.txt`         | 6   | `B001       |
| `drug.txt`        | 8   | `M001       |
| `record.txt`      | 7   | `R001       |
| `schedule.txt`    | 8   | `S001       |
| `appointment.txt` | 6   | `A001       |

### 7.2 数据生命周期

```
┌──────────────────────────────────────────────────────┐
│ 程序启动                                               │
│                                                        │
│  initGlobalLists()                                     │
│    ↓                                                   │
│  loadAllHisData()                                      │
│    ├── loadDeptData()   ── fopen ── fgets ── 逐行解析   │
│    ├── loadDoctorData() ── fopen ── fgets ── 逐行解析   │
│    ├── loadBedData()    ── fopen ── fgets ── 逐行解析   │
│    ├── loadPatientData()── fopen ── fgets ── 逐行解析   │
│    ├── loadRecordData() ── fopen ── fgets ── 逐行解析   │
│    ├── loadDrugData()   ── fopen ── fgets ── 逐行解析   │
│    ├── loadScheduleData()                              │
│    └── loadAppointmentData()                           │
│                                                        │
│  ┌──────────────────────────────────────────────────┐  │
│  │                运行期间                           │  │
│  │  所有操作在内存链表中进行                          │  │
│  │  每次修改立即 saveXxxData() 写回文件               │  │
│  └──────────────────────────────────────────────────┘  │
│                                                        │
│  程序退出 (exit(0))                                     │
│    ↓                                                   │
│  saveAllHisData()                                      │
│    ├── savePatientData() ── SaveDataToFile             │
│    ├── saveRecordData()  ── SaveDataToFile             │
│    └── ... (共8个)                                      │
│    ↓                                                   │
│  freeGlobalLists()                                     │
│    └── FreeList → free(节点data) → free(节点) → free(链表)│
└──────────────────────────────────────────────────────┘
```

---

## 八、安全与防护措施

| 防护类型     | 实现方式                                                   |
| -------- | ------------------------------------------------------ |
| 缓冲区溢出    | 所有 `fgets` 后检查 `strchr('\n')`，超长则 `ClearInputBuffer()` |
| GBK 字符截断 | 截断后若末尾字节 ≥0x81，自动移除不完整的中文字符                            |
| 分隔符注入    | `ValidateNoPipe()` 禁止输入中的 `                            |
| 密码保护     | nibble-swap 混淆存储，非明文                                   |
| 患者隐私     | 可选 6 位 PIN 码保护查看，3 次错误锁定                               |
| 登录锁定     | 同一角色 5 次失败后锁定（static 计数器，重启恢复）                         |
| 数据完整性    | 写 tmp 文件 + rename 原子操作                                 |
| 空值防护     | `HIS_STRNCPY` 宏对 NULL src 有保护                          |
| 死循环防护    | 所有循环输入均有 `break` 条件                                    |
| 整数溢出     | `GenerateID` 序号 static 递增，无溢出检查（可改进）                   |

---

## 九、已修复 Bug 与改进方向

### 9.1 已修复 Bug

| #   | 问题             | 根因                               | 修复                                                               |
| --- | -------------- | -------------------------------- | ---------------------------------------------------------------- |
| 1   | 账号无唯一性         | addDoctor/modifyDoctor 未检查       | 遍历 doctor_list 查重（排除自身）                                          |
| 2   | 空账号/密码         | fgets 直接接受空输入                    | 非空校验循环                                                           |
| 3   | 修改医生强制重输密码     | 无保留旧密码机制                         | 回车保留旧密码                                                          |
| 4   | fgets 溢出跳输入    | fgets 未消费 \n → 后续跳过              | strchr 检测 + ClearInputBuffer                                     |
| 5   | 密码迁移误判         | isprint 不可靠（nibble-swap 后部分仍可打印） | 登录时明文兼容比对                                                        |
| 6   | fgets 未检查返回值   | 直接使用结果                           | 返回 NULL 时清缓冲重试                                                   |
| 7   | 科室中文乱码         | load 函数未去 \r                     | 统一增加 \r 去除                                                       |
| 8   | GBK 截断显示 ?     | fgets 截断中文字符                     | 截断后移除残留 GBK 首字节 (≥0x81)                                          |
| 9   | dept.txt 编辑器乱码 | 0xC6 触发 UTF-8 误检                 | 不影响程序运行，可不修                                                      |
| 10  | 医生无会话跟踪        | 每次操作需手动输入医生ID                    | 引入全局 `g_current_doctor_id` / `g_current_doctor_name`，登录时设置，返回时清除 |
| 11  | 患者查询能力弱        | 仅有ID精确查询                         | 新增 `queryPatientSubMenu()` → 按姓名模糊/科室/状态多维度查询                    |
| 12  | 修改功能不够精细       | modify 直接重输所有字段                  | 全部改为选择性字段子菜单（patient/doctor/drug）                                |
| 13  | 手机号重复录入        | 无唯一性检查                           | 新增 `isPhoneUsedByOther()`，创建/修改时校验                               |
| 14  | 年龄接受非数字        | `atoi("abc")` 返回 0 绕过范围校验        | 增加 `ValidateNumber()` + `strcspn` 去换行后再转                         |
| 15  | 身份证号重复 + 年龄不匹配 | 无校验                              | 新增 `isIDCardUsedByOther()` + `getAgeFromIDCard()`，录入/修改时校验       |
| 16  | 删除医疗记录不更新计数    | 只删记录不更新 Patient.record_count     | 删除时同步更新 record_count                                             |
| 17  | 预约显示过期排班       | 未过滤历史日期                          | 排班列表增加 `strcmp(date, today) >= 0` 过滤                             |
| 18  | 取消挂号/预约不退费     | 原逻辑只清状态不退钱                       | 取消时查找关联记录并退还余额，恢复号源                                              |

### 9.2 可改进方向

- `sscanf` + `%[^|]` 不能处理 GBK trail byte = `0x7C` 的场景（建议所有解析统一改为 `strtok`）
- 密码存储只有混淆没有哈希（Nibble-swap 不是加密，仅防明文存储）
- 所有全局链表是外部变量，模块间耦合较紧
- `GenerateID` 的 `static int seq` 不是线程安全（单线程无问题）
- 缺少事务回滚机制（部分多表操作可能部分成功）
- 医疗记录类型枚举和打印用硬编码字符串，不符合 DRY
- `dept_bed.c` 仍混用 `scanf` 和 `fgets` 两种输入方式（历史遗留），建议统一为 `fgets`
- 排班管理在 `dept_bed.c` 中但排班 I/O 在 `patient.c` 中，模块边界可进一步梳理

---

## 十、架构图总结

### 分层架构

```
┌─────────────────────────────────────────────────────────┐
│                    表示层 (CLI 菜单)                      │
│  his_main.c: printMainMenu / adminMenu / doctorMenu...  │
├─────────────────────────────────────────────────────────┤
│                     控制层 (逻辑调度)                     │
│  his_main.c: adminLogin / doctorLogin / 菜单 switch     │
├─────────────────────────────────────────────────────────┤
│                     业务层 (CRUD + 校验)                 │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐  │
│  │dept_bed  │ │ patient  │ │  drug    │ │appoint...│  │
│  │ .c       │ │ .c       │ │ .c       │ │ (patient │  │
│  │科室/医生  │ │患者/记录  │ │药品/库存  │ │  .c内)   │  │
│  │ /床位    │ │ /挂号    │ │ /发药    │ │ 预约     │  │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘  │
├─────────────────────────────────────────────────────────┤
│                     数据层 (持久化)                      │
│  his_tool.c: SaveDataToFile / LoadDataFromFile          │
│  各模块: saveXxxData / loadXxxData / format / parse    │
├─────────────────────────────────────────────────────────┤
│                     基础设施层                           │
│  his_link.c: 通用链表                                   │
│  his_tool.c: 校验/ID生成/密码混淆/缓冲区清理             │
│  his_config.h: 常量/枚举/文件路径                       │
└─────────────────────────────────────────────────────────┘
```

### 三层菜单导航

```
main()
  ├── 管理员登录
  │     └── adminMenu
  │           ├── 患者与医疗记录管理 → patientModule()
  │           ├── 科室/医生/床位管理  → dept_bedModule()
  │           │     ├── 科室信息管理
  │           │     ├── 医生信息管理
  │           │     ├── 床位管理 (含住院/出院)
  │           │     ├── 床位统计查询
  │           │     └── 排班管理 (含新增/查看/删除/修改)
  │           ├── 药品药房管理       → drugModule()
  │           │     ├── 药品信息管理
  │           │     ├── 药品库存管理
  │           │     ├── 门诊发药
  │           │     ├── 全局查询统计
  │           │     └── 一键数据备份
  │           └── 修改管理员密码
  │
  ├── 医生登录
  │     └── doctorMenu (会话自动跟踪医生身份)
  │           ├── 查看我的患者
  │           ├── 管理医疗记录 (新增诊断/处方 + 修改就诊状态)
  │           ├── 查看患者预约信息
  │           ├── 查看我的排班
  │           ├── 查看个人信息
  │           └── 修改密码
  │
  └── 患者操作
        └── patientMenu
              ├── 普通挂号
              ├── 预约挂号
              ├── 查看我的挂号记录
              ├── 取消我的挂号/预约 (含退费)
              ├── 查看我的医疗记录 (PIN码保护)
              └── 自助充值
```

---

## 附录：关键文件清单

| 文件             | 行数  | 职责                          |
| -------------- | --- | --------------------------- |
| `his_main.c`   | ~500 | 主程序入口、菜单路由、登录验证、会话跟踪        |
| `dept_bed.c`   | ~530 | 科室/床位 CRUD + 统计             |
| `doctor.c`     | ~390 | 医生 CRUD + 持久化               |
| `drug.c`       | ~550 | 药品 CRUD + 库存 + 发药           |
| `patient.c`    | ~630 | 患者 CRUD + 充值 + 只读查看        |
| `record.c`     | ~520 | 医疗记录 CRUD + 医生工作站          |
| `registration.c` | ~500 | 普通挂号 + 预约挂号                 |
| `schedule.c`   | ~430 | 排班管理（CRUD + 持久化）           |
| `admin_tools.c` | ~220 | 全局查询统计 + 数据备份              |
| `his_tool.c`   | ~190 | 工具函数、校验、文件 I/O、密码混淆         |
| `his_link.c`   | ~110 | 通用链表实现                      |
| `his.h`        | ~230 | 结构体定义、函数声明、宏                |
| `his_config.h` | ~90  | 配置常量、枚举、文件路径                |
| **合计**         | **~5470** | **(原 5452 行，功能不变，结构更清晰)** |
