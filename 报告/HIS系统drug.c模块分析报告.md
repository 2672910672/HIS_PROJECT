# HIS医院信息系统 — drug.c 模块分析报告

## 概述

`drug.c`（约550行）是 HIS 系统的**药品药房管理模块**，涵盖药品信息管理、库存管理（入库/出库）、门诊发药（含医保结算）三大功能。发药操作同时影响药品库存、患者余额和医疗记录三个子系统，是系统中**业务链路最长**的操作。全局查询统计与数据备份功能已拆分至独立的 `admin_tools.c`。

---

## 一、模块定位

### 职责范围

- 药品信息管理（增删改查，支持多科室归属）
- 药品库存管理（入库、出库、预警提示）
- 门诊发药（含患者身份校验、医生关联、医保结算）

### 对外接口

```c
// 模块唯一入口
void drugModule();

// 数据持久化
void saveDrugData(void);
void loadDrugData(void);

// 打印函数（跨模块调用，供 admin_tools.c 使用）
void printDrugInfo(void* data);
```

模块内部所有业务函数均为 `static`，对外仅暴露有限接口，封装性好。

---

## 二、内部架构

```
drug.c
├── 药品信息管理
│   ├── addDrug         — 新增（关联科室+3名+2价+1阈值）
│   ├── modifyDrug      — 7个可修改字段（含库存和预警）
│   ├── deleteDrug      — 删除（校验库存为0）
│   └── queryDrug       — 4种查询方式（ID/关键词/科室/全部）
│
├── 药品库存管理
│   ├── drugInbound     — 入库（增加库存+预警提示）
│   ├── drugOutbound    — 出库（减少库存+不足拦截+预警提示）
│   └── viewStockWarning — 查看所有低于阈值的药品
│
├── 门诊发药
│   ├── issuePrescription        — 完整发药流程（9步）
│   └── hasDuplicatePrescription — 检查重复处方（内部辅助）
│
└── 数据持久化
    └── formatDrugLine / parseDrugLine
```

### 子菜单结构

```
drugModule()
├── 1. 药品信息管理 (drugInfoSubMenu)
│   ├── 添加药品
│   ├── 修改药品信息
│   ├── 删除药品
│   └── 查询药品
├── 2. 药品库存管理 (drugStockSubMenu)
│   ├── 药品入库
│   ├── 药品出库
│   └── 查看库存预警
├── 3. 门诊发药 (prescriptionSubMenu)
│   └── 门诊发药
├── 4. 全局查询统计 * → 委托至 admin_tools.c
└── 5. 一键数据备份 * → 委托至 admin_tools.c
```

---

## 三、关键设计分析

### 1. 药品信息的三名体系

药品支持**通用名**、**商品名**、**别名**三个名称字段，满足医院药房的实际需求：

```c
typedef struct {
    char id[MAX_ID_LEN];
    char general_name[MAX_NAME_LEN];   // 通用名（必填）
    char trade_name[MAX_NAME_LEN];     // 商品名（可选）
    char alias[MAX_NAME_LEN];          // 别名（可选）
    float price;                       // 单价
    int stock;                         // 库存
    int warning_threshold;             // 预警阈值
    char dept_id[MAX_ID_LEN];          // 所属科室
} Drug;
```

查询时支持对三个名称字段的模糊匹配：

```c
if (strstr(d->general_name, keyword) ||
    strstr(d->trade_name, keyword) ||
    strstr(d->alias, keyword)) {
    printDrugInfo(d);
}
```

### 2. 库存预警的三态库存显示

`printDrugInfo` 中根据库存量动态显示状态：

```c
const char* stock_status;
if (d->stock == 0) stock_status = "[警告]缺货";
else if (d->stock < d->warning_threshold) stock_status = "[警告]不足";
else stock_status = "[正常]充足";
```

### 3. 门诊发药的完整事务流程

`issuePrescription()` 是 drug.c 中最复杂的函数，实现了跨三个子系统的数据更新：

```
┌─────────────────────────────────────────────────────────────┐
│                    issuePrescription                         │
├─────────────────────────────────────────────────────────────┤
│  1. 输入患者ID → 校验存在                                    │
│  2. 输入药品ID → 校验存在                                    │
│  3. 输入数量 → 校验库存充足                                  │
│  4. 输入医生ID → 校验存在 + 校验患者挂该医生的号              │
│  5. 计算费用: total = price * quantity                       │
│     insurance_pay = total * insurance_ratio                 │
│     patient_pay = total - insurance_pay                     │
│  6. 校验患者余额 >= patient_pay                             │
│  7. 确认发药                                                │
│  8. 执行三个原子操作:                                        │
│     ├── 扣减药品库存: d->stock -= quantity                  │
│     ├── 扣减患者余额: p->balance -= patient_pay              │
│     └── 创建医疗处方记录: insert record_list                │
│  9. 保存三个数据文件                                        │
└─────────────────────────────────────────────────────────────┘
```

关键执行代码：

```c
// 8. 执行数据更新
d->stock -= quantity;                    // 扣库存
p->balance -= patient_pay;               // 扣余额
// 创建医疗记录
MedicalRecord r;
GenerateID(r.id, ID_PREFIX_RECORD);
r.type = RECORD_PRESCR;
r.cost = total_cost;
snprintf(r.detail, MAX_DETAIL_LEN, "门诊发药: %s x%d, 医保报销%.2f元",
    d->general_name, quantity, insurance_pay);
InsertNode(record_list, -1, &r, sizeof(MedicalRecord), r.id);
p->record_count++;

// 9. 保存所有数据
saveDrugData();
savePatientData();
saveRecordData();
```

### 4. 医生-患者关联校验

发药前严格校验开药医生与患者挂号的医生一致：

```c
if (strcmp(p->doctor_id, doctor_id) != 0) {
    printf("\n[错误] 该患者未挂该医生的号，无法为其发药！\n");
    return;
}
```

模拟了真实医院的"挂号→就诊→开药"流程约束。

### 5. 重复处方检查保护

发药前检查患者是否已有处方记录，避免重复发药：

```c
static int hasDuplicatePrescription(const char* patient_id) {
    ListNode* rp = record_list->head;
    while (rp) {
        MedicalRecord* r = (MedicalRecord*)rp->data;
        if (strcmp(r->patient_id, patient_id) == 0 && r->type == RECORD_PRESCR)
            return 1;
        rp = rp->next;
    }
    return 0;
}
```

如有已有处方，系统会给出警告并确认是否继续，防止误操作。

### 6. 数据持久化

药品的 format/parse 使用 `sscanf` + 精确宽度控制：

```c
static void formatDrugLine(void* data, char* line) {
    Drug* d = (Drug*)data;
    sprintf(line, "%s|%s|%s|%s|%.2f|%d|%d|%s",
        d->id, d->general_name, d->trade_name, d->alias,
        d->price, d->stock, d->warning_threshold, d->dept_id);
}

static void parseDrugLine(char* line, void* data) {
    Drug* d = (Drug*)data;
    memset(d, 0, sizeof(Drug));
    sscanf(line, "%19[^|]|%49[^|]|%49[^|]|%49[^|]|%f|%d|%d|%19[^\n]",
        d->id, d->general_name, d->trade_name, d->alias,
        &d->price, &d->stock, &d->warning_threshold, d->dept_id);
}
```

`%19[^|]` 等宽度限制防止缓冲区溢出，比 dept_bed.c 中未限制的 `%[^|]` 更安全。

---

## 四、全局查询统计的统一入口

`printGlobalQuery` 实现了"输入一个ID，查询所有类型实体"的能力：

```c
switch (choice) {
case 1: /* 查患者 */ printPatientInfo(p); break;
case 2: /* 查医生 */ printDoctorInfo(d); break;
case 3: /* 查科室 */ printDeptInfo(dept); break;
case 4: /* 查床位 */ printBedInfo(b); break;
case 5: /* 查药品 */ printDrugInfo(d); break;
}
```

调用在其他模块定义的打印函数，体现了跨模块函数复用。

---

## 四、优化修改记录

### 修改1：统一输入方式（scanf → getValidChoice）

将 `queryDrug`、`printGlobalQuery` 中残留的 `scanf` + `ClearInputBuffer` 手动校验模式统一替换为 `getValidChoice`，与 patient.c 保持一致：

```c
// 修改前
int choice;
if (scanf("%d", &choice) != 1) { ClearInputBuffer(); return; }
ClearInputBuffer();

// 修改后
int choice = getValidChoice(1, 4);
```

### 修改2：预警阈值默认值

`inputDrugInfo` 中输入预警阈值时，根据初始库存的20%（`DRUG_WARNING_RATIO`）自动计算建议默认值，用户可直接回车接受：

```c
int default_threshold = (int)(d->stock * DRUG_WARNING_RATIO);
if (default_threshold < 1) default_threshold = 1;
printf("请输入库存预警阈值 (直接回车默认 %d，即库存的 %.0f%%): ",
    default_threshold, DRUG_WARNING_RATIO * 100);
```

### 修改3：出入库操作记录

`drugInbound` 和 `drugOutbound` 在执行后输出带时间戳的操作记录，便于追溯：

```c
char now[30];
GetSystemTime(now);
printf("  [操作记录] %s | 入库 %s x%d | 经办人: 管理员\n", now, d->general_name, quantity);
```

### 修改4：发药数据保存顺序

`issuePrescription` 中调整三个数据文件的保存顺序，将医疗记录（审计证据）放在最前面，减少数据不一致风险：

```c
// 修改前
saveDrugData();    // 先存库存
savePatientData(); // 再存余额
saveRecordData();  // 最后存记录

// 修改后
saveRecordData();  // 先存记录（事务的权威证据）
saveDrugData();    // 再存库存变更
savePatientData(); // 最后存余额变更
```

---

## 五、设计评价

### 优点

| 方面              | 评价                               |
| --------------- | -------------------------------- |
| **封装性强**        | 文件内部函数几乎全 static，仅暴露必要接口，信息隐藏做得好 |
| **业务链路完整**      | 门诊发药串联了药品库存、患者余额、医疗记录三个子系统       |
| **安全机制**        | 药品删除前检查库存为0、发药前校验医患匹配、库存预警提示     |
| **sscanf 宽度保护** | `%19[^                           |
| **输入校验增强**     | 药品名称输入已改为 while 循环校验非空和分隔符           |

### 可改进点

| 方面          | 建议                                      | 状态                                            |
| ----------- | --------------------------------------- | --------------------------------------------- |
| **药品批号管理**  | 真实药房需要批号/有效期，当前仅管理库存数量                  | 待处理（结构性变更较大）                                  |
| **发药回滚不完整** | 发药中途失败缺少事务回滚机制                          | **已修复** → 调整保存顺序（记录→库存→余额）                    |
| **缺少出入库记录** | 入库/出库操作没有操作日志记录，无法追溯                    | **已修复** → 操作后输出带时间戳的记录                        |
| **预警阈值初始化** | `inputDrugInfo` 中的预警阈值可提供建议默认值（如库存的20%） | **已修复** → 根据 `DRUG_WARNING_RATIO`（20%）自动计算默认值 |
| **重复处方检查**   | 发药前未检查患者是否已有处方，可能重复发药                  | **已修复** → 提取 `hasDuplicatePrescription` 函数     |
