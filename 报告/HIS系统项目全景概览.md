# HIS 医院信息系统 — 项目全景概览

## 一、项目结构一览

```
HIS_System_Project/
├── his_config.h          配置常量（长度/枚举/文件路径/默认值）
├── his.h                全局头文件（结构体定义/函数声明/extern 变量）
│
├── his_main.c           程序入口 / 三种角色登录认证 / 主菜单路由
├── his_tool.c           通用工具函数（输入校验/ID生成/文件I/O/密码混淆）
├── his_link.c           通用单向链表（Init/Insert/Delete/Find/Traverse/Free）
│
├── dept_bed.c           科室管理 + 床位管理（CRUD + 床位统计）
├── doctor.c             医生管理（CRUD + 账号密码 + 持久化）
├── schedule.c           排班管理（CRUD + 持久化）
├── drug.c               药品管理（CRUD + 库存管理 + 门诊发药）
├── patient.c            患者管理（CRUD + 充值 + 只读查看 + PIN校验）
├── record.c             医疗记录管理（CRUD + 医生工作站）
├── registration.c       普通挂号 + 预约挂号
├── admin_tools.c        全局查询统计 + 管理员报表 + 数据备份
│
└── data/                数据文件（启动时加载，运行中实时保存）
    ├── patient.txt / doctor.txt / dept.txt
    ├── bed.txt / drug.txt / record.txt
    ├── schedule.txt / appointment.txt
    └── admin.dat
```

**共 13 个源文件，约 5500 行 C 代码。**

---

## 二、模块依赖关系

```
                    ┌─────────────────────┐
                    │    his_config.h      │  ← 所有文件引用
                    │  常量/枚举/路径       │
                    └─────────┬───────────┘
                              │
                    ┌─────────▼───────────┐
                    │       his.h         │  ← 所有 .c 文件引用
                    │  结构体/函数声明     │
                    └─────────┬───────────┘
                              │
         ┌────────────────────┼───────────────────────────┐
         ▼                    ▼                           ▼
 ┌───────────────┐   ┌───────────────┐   ┌───────────────────────┐
 │  his_tool.c   │   │  his_link.c   │   │     his_main.c        │
 │  输入/校验    │   │  通用链表     │   │  程序入口/主菜单      │
 │  文件I/O      │   │  7个链表实例  │   │  三种角色登录          │
 │  ID生成/混淆  │   └───────┬───────┘   └───────────┬───────────┘
 └───────┬───────┘           │                       │
         │                   │           ┌───────────┼───────────┬───────────┐
         │    全局链表变量跨模块共享       ▼           ▼           ▼           ▼
         │                   │     ┌──────────┐ ┌────────┐ ┌────────┐ ┌──────────┐
         │                   │     │dept_bed  │ │ doctor │ │ drug   │ │patient   │
         │                   │     │科室/床位  │ │ 医生   │ │ 药品   │ │ 患者     │
         │                   │     └──────────┘ └────────┘ └────────┘ └────┬─────┘
         │                   │                                              │
         └───────┬───────────┘                        ┌────────────────────┘
                 │                                    ▼
         ┌───────▼───────┐                    ┌──────────────┐
         │ 全局链表:      │                    │  record.c    │
         │ patient_list   │                    │  医疗记录    │
         │ doctor_list    │                    │  医生工作站  │
         │ dept_list      │                    └──────────────┘
         │ bed_list       │                    ┌──────────────┐
         │ drug_list      │                    │ registration │
         │ record_list    │                    │  挂号/预约   │
         │ schedule_list  │                    └──────────────┘
         │ appointment_li │                    ┌──────────────┐
         └────────────────┘                    │ admin_tools  │
                                               │ 统计/备份    │
                                               └──────────────┘
```

---

## 三、全局链表：模块间的数据桥梁

```
所有模块通过 8 个全局 extern 链表访问数据：

  patient_list  — 患者实体（patient.c 读写，其它模块只读引用）
  doctor_list   — 医生实体（doctor.c 读写）
  dept_list     — 科室实体（dept_bed.c 读写）
  bed_list      — 床位实体（dept_bed.c 读写）
  drug_list     — 药品实体（drug.c 读写）
  record_list   — 记录实体（record.c 读写）
  schedule_list — 排班实体（schedule.c 读写）
  appointment   — 预约实体（registration.c 读写）

┌─────────────┐      ┌─────────────┐      ┌─────────────┐
│ 模块A       │      │ 模块B       │      │ 模块C       │
│ (写自己数据) │      │ (读他人数据) │      │ (读/写引用)  │
│             │      │             │      │             │
│  dept_list ─┼──────┤→ dept_list  │      │  drug_list  │
│  bed_list   │      │→ doctor_list│      │  patient... │
│             │      │→ patient_list      │  doctor...  │
└─────────────┘      └─────────────┘      └─────────────┘
```

**写者原则**：每个链表的主要写入由其归属模块完成，其它模块仅查询或通过函数接口读写。

---

## 四、跨模块函数调用关系

```
his_main.c (主控)
├── loadAdminConfig()  [self]
├── adminLogin()       [self]     → strcmp(ADMIN_USERNAME, s_admin_password)
├── doctorLogin()      [self]     → 遍历 doctor_list 比对账号密码
├── patientMenu()      [self]
│   ├── normalRegistration()     → registration.c
│   ├── appointmentRegistration() → registration.c
│   ├── patientRecharge()        → patient.c
│   ├── patientViewOnlyModule()  → patient.c
│   ├── viewMyRegistration()     → registration.c
│   └── cancelMyRegistration()   → registration.c
├── adminMenu()        [self]
│   ├── patientModule()          → patient.c
│   │     ├── inputAndViewRecords()  → record.c
│   │     ├── inputAndAddRecord()    → record.c
│   │     ├── inputAndModifyRecord() → record.c
│   │     └── inputAndDeleteRecord() → record.c
│   ├── dept_bedModule()         → dept_bed.c
│   │     ├── doctorSubMenu()        → doctor.c
│   │     └── scheduleSubMenu()      → schedule.c
│   ├── drugModule()             → drug.c
│   │     ├── globalStatsSubMenu()   → admin_tools.c
│   │     └── backupAllData()       → admin_tools.c
│   └── changeAdminPassword()   [self]
└── doctorMenu()      [self]
    ├── queryPatientByDoctor()  → record.c
    ├── medicalRecordModule()   → record.c
    ├── queryMyAppointment()    → record.c
    ├── viewMySchedule()        [self]
    ├── viewMyProfile()         [self]
    └── changeDoctorPassword()  [self]
```

**跨模块调用接口**：所有跨模块调用通过 `his.h` 中的 `extern` 函数声明完成，模块间不直接引用对方内部函数。

---

## 五、公用打印函数

这些 `printXxxInfo` 函数在各模块中定义，但被多处调用：

| 函数                 | 定义在        | 被调用方                                                                |
| ------------------ | ---------- | ------------------------------------------------------------------- |
| `printPatientInfo` | patient.c  | record.c（医生查看患者）、admin_tools.c（全局查询）                                |
| `printDeptInfo`    | dept_bed.c | doctor.c（选择科室）、drug.c（选择科室）、schedule.c、registration.c、admin_tools.c |
| `printDoctorInfo`  | doctor.c   | record.c（医生工作站）、admin_tools.c（全局查询）                                 |
| `printBedInfo`     | dept_bed.c | admin_tools.c（全局查询）                                                 |
| `printDrugInfo`    | drug.c     | admin_tools.c（全局查询）                                                 |

**模式特点**：接受 `void*` 参数，与 `TraverseList` 的 `void (*print_func)(void*)` 签名兼容。

---

## 六、多表联动的核心业务操作

### 挂号 (normalRegistration)

```
涉及: patient + doctor + record
patient: balance -= pay, doctor_id=, dept_id=, register_status=PENDING, record_count++
doctor:  current_register++
record:  新增 RECORD_REGISTER 类型记录
```

### 发药 (issuePrescription)

```
涉及: drug + patient + record
drug:    stock -= quantity
patient: balance -= patient_pay, record_count++
record:  新增 RECORD_PRESCR 类型记录（含费用明细）
```

### 住院 (modifyBedStatus → admit)

```
涉及: bed + patient
bed:     status=OCCUPIED, patient_id=, admit_time=
patient: is_inpatient=IN, bed_id=
```

### 取消预约 (cancelMyRegistration → 预约)

```
涉及: appointment + schedule + patient
appointment: status="已取消"
schedule:    current_patients-- (恢复号源)
patient:     balance += cost (退费)
```

---

## 七、代码架构模式总结

### 1. 模块的"三件套"模式

每个业务模块遵循相同的结构：

```
xxx.c
├── 静态函数声明（模块内部函数前向声明）
├── 业务 CRUD 函数（add/modify/delete/query）
├── 子菜单函数（用户交互循环）
├── 数据持久化（saveXxxData / loadXxxData + format/parse 函数）
└── 对外入口函数（xxxModule / xxxSubMenu）
```

### 2. 数据持久化的"四元组"模式

```
saveXxxData()  → SaveDataToFile(list, FILE, formatXxx)
formatXxx()    → void formatXxx(void* data, char* line)
loadXxxData()  → LoadDataFromFile(list, FILE, parseXxx)
parseXxx()     → void parseXxx(char* line, void* data)
```

### 3. 子菜单的"循环-显示-选择-分发"模式

```c
void xxxSubMenu() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("菜单标题\n");
        PrintSeparator();
        printf("  1. 功能A\n");
        printf("  2. 功能B\n");
        printf("  0. 返回\n");
        choice = getValidChoice(0, N);
        switch (choice) {
        case 1: funcA(); break;
        case 2: funcB(); break;
        case 0: return;
        }
    }
}
```

整个项目中有约 10 个这样的子菜单，结构完全一致。

---

## 八、文件行数及贡献占比

| 文件             | 行数        | 占比    | 职责      |
| -------------- | --------- | ----- | ------- |
| his_config.h   | ~90       | 1.6%  | 配置常量    |
| his.h          | ~230      | 4.2%  | 全局声明    |
| his_main.c     | ~500      | 9.1%  | 主控/三种角色 |
| dept_bed.c     | ~530      | 9.7%  | 科室+床位   |
| doctor.c       | ~390      | 7.1%  | 医生管理    |
| schedule.c     | ~430      | 7.9%  | 排班管理    |
| drug.c         | ~550      | 10.1% | 药品+发药   |
| admin_tools.c  | ~220      | 4.0%  | 统计+备份   |
| patient.c      | ~630      | 11.5% | 患者管理    |
| record.c       | ~520      | 9.5%  | 医疗记录    |
| registration.c | ~500      | 9.1%  | 挂号+预约   |
| his_tool.c     | ~190      | 3.5%  | 工具函数    |
| his_link.c     | ~110      | 2.0%  | 通用链表    |
| **合计**         | **~5470** | 100%  |         |

---

## 九、项目演变记录

```
重构前（单文件 + 大模块）:
  his_main.c (500) + dept_bed.c (1340) + patient.c (2080) + drug.c (920)
  = 约 4840 行 in 4 个功能文件

重构后（按职责拆分）:
  his_main.c (500) + dept_bed.c(530)+doctor.c(390)   ← dept_bed.c 拆分
  + schedule.c(430)                                   ← 排班独立
  + drug.c(550)                                       ← 药品精简
  + admin_tools.c(220)                                ← 统计/备份独立
  + patient.c(630) + record.c(520) + registration.c(500) ← patient.c 拆分
  + his_tool.c(190) + his_link.c(110)
  = 13 个文件，约 5470 行

关键优化:
  /utf-8 → /source-charset:utf-8  修复终端乱码
  LoadDataFromFile 统一 4 处 loadXxxData    减少 ~50 行重复代码
  删除 GetListLength / ValidateID           消除死代码
  删除 UserRole 枚举 / DEFAULT_BALANCE      清理未使用配置
  inputDrugInfo 循环校验                    修复输入安全漏洞
  modifySchedule 用 readString 代替 fgets   统一输入方式
  deleteXxx 输入 0 视为取消                  改进用户体验
  addSchedule 校验号源 <= 医生上限            新增业务校验
```

---

## 十、文件速查索引

当你想找某个功能时，从这里开始：

| 想做什么    | 找哪个文件          | 入口函数                                      |
| ------- | -------------- | ----------------------------------------- |
| 启动流程/菜单 | his_main.c     | `main()`                                  |
| 管理科室/床位 | dept_bed.c     | `dept_bedModule()`                        |
| 管理医生    | doctor.c       | `doctorSubMenu()`                         |
| 管理排班    | schedule.c     | `scheduleSubMenu()`                       |
| 管理药品/发药 | drug.c         | `drugModule()`                            |
| 管理患者    | patient.c      | `patientModule()`                         |
| 管理医疗记录  | record.c       | `medicalRecordModule()`                   |
| 挂号/预约   | registration.c | `appointSubMenu()`                        |
| 统计报表/备份 | admin_tools.c  | `globalStatsSubMenu()`, `backupAllData()` |
| 数据结构定义  | his.h          | 8 个 struct + 链表节点                         |
| 配置常量    | his_config.h   | 长度/枚举/文件路径                                |
| 链表操作    | his_link.c     | `InitList` ~ `FreeList`                   |
| 工具函数    | his_tool.c     | `readString` ~ `ValidateIDCard`           |
