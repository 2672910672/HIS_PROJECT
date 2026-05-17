# HIS医院信息系统 — dept_bed.c 模块分析报告

## 概述

`dept_bed.c`（约530行）是 HIS 系统的**基础数据管理模块**，负责**科室**和**床位**两大核心实体的增删改查（CRUD）与关联维护。同时提供床位使用率统计功能。医生管理和排班管理已拆分至独立的 `doctor.c` 和 `schedule.c`。

---

## 一、模块定位

### 职责范围

- 科室信息管理（增删改查 + 医生数自动维护）
- 床位管理（含入住/出院流程、状态切换）
- 床位统计查询（按科室 + 全院视角）

### 对外接口

```c
// 模块入口
void dept_bedModule();  // 管理员菜单 → 二级入口（含医生/排班子菜单跳转）

// 数据持久化
void saveDeptData(void);
void loadDeptData(void);
void saveBedData(void);
void loadBedData(void);

// 打印函数（供其他模块使用）
void printDeptInfo(void* data);
void printBedInfo(void* data);
```

---

## 二、内部架构

```
dept_bed.c
├── 科室管理
│   ├── addDept         — 新增（自动生成K开头ID）
│   ├── modifyDept      — 修改名称
│   ├── deleteDept      — 删除（校验医生数和床位）
│   └── queryDept       — ID查询 / 全部列出
│
├── 床位管理
│   ├── addBed          — 新增（关联科室+病房类型选择）
│   ├── modifyBedStatus — 入住/出院状态切换
│   ├── deleteBed       — 删除（校验占用状态）
│   └── queryBed        — 4种查询方式
│
├── 统计功能
│   └── statsSubMenu    — 按科室/全院床位使用率统计
│
└── 数据持久化（2套 format/parse）
    ├── formatDeptLine / parseDeptLine
    └── formatBedLine / parseBedLine
```

---

## 三、关键设计分析

### 1. 科室-床位的双层关联模型

科室与床位之间通过 `dept_id` 字段建立外键关联，并在关键操作中维护引用完整性：

```
Department (科室)
  ├── doctor_count（医生数，由 doctor.c 中的 addDoctor/deleteDoctor 自动增减）
  └── Bed（床位，dept_id 关联，查询时显示科室名称）
```

#### 删除保护

```c
// 删除科室前：检查医生数
if (d->doctor_count > 0) {
    printf("[错误] 该科室有 %d 名医生，无法删除！\n", d->doctor_count);
    return;
}
// 删除科室前：检查床位存在
ListNode* p = bed_list->head;
while (p) {
    Bed* b = (Bed*)p->data;
    if (strcmp(b->dept_id, id) == 0) { has_bed = 1; break; }
}
```

### 2. 床位管理的"入住/出院"状态机

`modifyBedStatus` 实现了床位状态的有限状态机：

```
入住流程: BED_FREE + 患者未住院 → BED_OCCUPIED + 患者标记住院
出院流程: BED_OCCUPIED          → BED_FREE + 患者标记出院 + 清空记录
```

关键代码：

```c
if (b->status == BED_FREE) {
    // 办理住院
    Patient* p = (Patient*)patient_node->data;
    if (p->is_inpatient == PATIENT_IN) {
        printf("[错误] 患者已处于住院状态！\n");
        return;
    }
    b->status = BED_OCCUPIED;
    HIS_STRNCPY(b->patient_id, patient_id, MAX_ID_LEN);
    GetSystemTime(b->admit_time);
    p->is_inpatient = PATIENT_IN;
    HIS_STRNCPY(p->bed_id, bed_id, MAX_ID_LEN);
}
else {
    // 办理出院
    b->status = BED_FREE;
    HIS_STRNCPY(b->patient_id, "-1", MAX_ID_LEN);
    b->admit_time[0] = '\0';
    p->is_inpatient = PATIENT_OUT;
}
```

### 3. 床位统计的两种视角

```c
static void calculateBedStats(const char* dept_id, int* total, int* occupied) {
    ListNode* p = bed_list->head;
    while (p) {
        Bed* b = (Bed*)p->data;
        // dept_id == NULL 表示全院统计
        if (dept_id == NULL || strcmp(b->dept_id, dept_id) == 0) {
            (*total)++;
            if (b->status == BED_OCCUPIED) (*occupied)++;
        }
        p = p->next;
    }
}

// 统计报表输出
static void printBedStats(const char* title, int total, int occupied) {
    double occupancy_rate = (total > 0) ? ((double)occupied / total) * 100 : 0;
    printf("  总床位: %d\n", total);
    printf("  已占用: %d\n", occupied);
    printf("  空闲:   %d\n", free_beds);
    printf("  占用率: %.1f%%\n", occupancy_rate);
}
```

---

## 四、优化修改记录

### 修改1：parseBedLine 类型转换安全修复

`parseBedLine` 中直接对枚举指针进行 `int*` 强制转换，属于 C 语言的未指定行为。改用中间 int 变量后赋值：

```c
// 修改前
sscanf(line, "%[^|]|%d|...", b->id, (int*)&b->room_type, ..., (int*)&b->status);

// 修改后
int room_type, status;
sscanf(line, "%[^|]|%d|...", b->id, &room_type, ..., &status);
b->room_type = (RoomType)room_type;
b->status = (BedStatus)status;
```

### 修改2：parseDeptLine 缓冲区溢出修复

`parseDeptLine` 使用 `%[^|]` 无宽度限制，超长输入可导致缓冲区溢出。添加宽度控制：

```c
// 修改前
sscanf(line, "%[^|]|%[^|]|%d", d->id, d->name, &d->doctor_count);

// 修改后（%19[^|] 和 %49[^|] 限制最大长度）
sscanf(line, "%19[^|]|%49[^|]|%d", d->id, d->name, &d->doctor_count);
```

---

## 五、数据持久化文件格式

| 实体  | 文件路径              | 字段                                                               | 解析方式             |
| --- | ----------------- | ---------------------------------------------------------------- | ---------------- |
| 科室  | `data/dept.txt`   | `id\|name\|doctor_count`                                         | sscanf %[^\|] 格式 |
| 床位  | `data/bed.txt`    | `id\|room_type\|dept_id\|status\|patient_id\|admit_time`         | sscanf %[^\n]    |

医生数据现已存储在 `data/doctor.txt`（由 `doctor.c` 管理），排班数据存储在 `data/schedule.txt`（由 `schedule.c` 管理）。

---

## 六、设计评价

### 优点

| 方面         | 评价                          |
| ---------- | --------------------------- |
| **关联完整性**  | 科室和床位的增删操作都有完整的外键约束检查，避免孤儿数据 |
| **床位状态机**  | 入住/出院流程清晰，双向同步患者和床位状态       |
| **load 函数简化** | 手写的 fopen→fgets→parse→InsertNode 已全部替换为一行 LoadDataFromFile 调用 |

### 可改进点

| 方面                      | 建议                                                                           | 状态                             |
| ----------------------- | ---------------------------------------------------------------------------- | ------------------------------ |
| **parseBedLine 类型转换**   | `(int*)&b->room_type` 和 `(int*)&b->status` 的强制转换在 C 中属于未指定行为，可用中间 int 变量     | **已修复** → 使用中间 int 变量后赋值       |
| **parseDeptLine 缓冲区溢出** | `sscanf` 未限制字符串长度，超长输入可能溢出                                                   | **已修复** → 添加 `%19[^            |
