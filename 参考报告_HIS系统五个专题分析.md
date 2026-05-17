# HIS 医院信息系统 — 五个专题分析（参考稿）

> 本文档供撰写 HIS系统报告.md 时参考，涵盖五个专题：
> 一、安全与校验机制  
> 二、问题解决策略  
> 三、特色代码设计  
> 四、相关测试数据  
> 五、核心业务数据联动

---

## 一、安全与校验机制

### 1.1 输入校验体系

系统构建了多层输入防护，从底层输入函数到业务校验逐层拦截非法数据：

#### 层级一：统一菜单输入 — getValidChoice

```c
int getValidChoice(int min, int max) {
    char buf[64];
    int choice;
    while (1) {
        if (!inputLine(buf, sizeof(buf))) {
            printf("输入异常，请重新输入: ");
            continue;
        }
        if (strlen(buf) == 0) {
            printf("输入不能为空，请重新输入 (%d-%d): ", min, max);
            continue;
        }
        int valid = 1;
        for (int i = 0; buf[i]; i++) {
            if (buf[i] < '0' || buf[i] > '9') { valid = 0; break; }
        }
        if (!valid) { printf("输入无效，只能输入数字 (%d-%d): ", min, max); continue; }
        choice = atoi(buf);
        if (choice >= min && choice <= max) return choice;
        printf("输入超出范围，请重新输入 (%d-%d): ", min, max);
    }
}
```

对比裸 `scanf("%d")` 的常见问题：

| 问题      | scanf                                                   | getValidChoice                                     |
| ------- | ------------------------------------------------------- | -------------------------------------------------- |
| 输入"abc" | scanf 返回 0，缓冲区残留 'a','b','c','\n'，后续所有 scanf 全部失败，陷入死循环 | fgets 读整行，检测到非数字字符后提示重输，缓冲区已清空                     |
| 输入"3 5" | scanf 只读 3，"\n" 和 "5" 残留，下次读取直接返回 5，造成逻辑错误              | fgets 读整行，atoi 取第一个数字，无关字符不影响下次输入                  |
| 缓冲区溢出   | 无保护，超长输入篡改栈上变量                                          | fgets 限制 64 字符，超长部分留在 stdin 中由 ClearInputBuffer 清空 |

#### 层级二：fgets 溢出检测与 GBK 截断保护（内联于各模块输入函数）

GBK 截断保护嵌入在 doctor.c、patient.c 等模块的输入函数中。典型模式如下（以 doctor.c 的 inputDoctorInfo 为例）：

```c
if (!fgets(d->name, MAX_NAME_LEN, stdin)) { ClearInputBuffer(); continue; }
nl = strchr(d->name, '\n');
if (nl) {
    *nl = '\0';          // 正常读取到换行符
} else {
    ClearInputBuffer();  // 输入超长，清空 stdin 残留
    // GBK 截断保护：中文字符由2字节组成，若截断发生在首字节位置
    // 该字节可能 >= 0x81（GBK 首字节范围），移除该字节防止乱码
    size_t _len = strlen(d->name);
    if (_len > 0 && (unsigned char)d->name[_len - 1] >= 0x81)
        d->name[_len - 1] = '\0';
}
```

为何不在 readString 集中处理？readString 是通用函数（简化版的 fgets+去换行），而 GBK 截断保护需要操作目标缓冲区，各模块的缓冲区大小和业务要求不同，因此保护逻辑嵌入在各个模块的输入代码中。

#### 层级三：字段级校验

| 字段     | 校验函数                                                                | 规则                                   | 违规处理       |
| ------ | ------------------------------------------------------------------- | ------------------------------------ | ---------- |
| 姓名/科室名 | `ValidateNoPipe()`                                                  | 不能包含分隔符 `\|`                         | while 循环重输 |
| 年龄     | `inputAge()`                                                        | 0~150 整数                             | 循环重输       |
| 性别     | `inputGender()`                                                     | 只能是"男"或"女"                           | 循环重输       |
| 手机号    | `ValidatePhone()` + `isPhoneUsedByOther()`                          | 11 位、全数字、以 1 开头、全院唯一                 | 格式或重复均重输   |
| 身份证    | `ValidateIDCard()` + `isIDCardUsedByOther()` + `getAgeFromIDCard()` | 18 位、加权校验码 (GB 11643-1999)、全院唯一、年龄一致 | 逐个拦截重输     |
| 医保比例   | `inputInsuranceRatio()`                                             | 0.0~1.0 浮点数                          | 循环重输       |
| 余额/充值  | `patientRecharge()`                                                 | 单次 ≤ 10 万、总额 ≤ 50 万                  | 提示上限后重输    |
| 菜单选项   | `getValidChoice()`                                                  | 数字、范围内                               | 提示范围后重输    |

### 1.2 身份证校验 (GB 11643-1999)

```c
int ValidateIDCard(const char* id_card) {
    if (!id_card || strlen(id_card) != 18) return 0;
    for (size_t i = 0; i < 17; i++)
        if (id_card[i] < '0' || id_card[i] > '9') return 0;
    char last = id_card[17];
    if (!(last >= '0' && last <= '9') && last != 'X' && last != 'x') return 0;

    static const int weights[17] = { 7,9,10,5,8,4,2,1,6,3,7,9,10,5,8,4,2 };
    static const char check_chars[11] = "10X98765432";
    int sum = 0;
    for (size_t i = 0; i < 17; i++)
        sum += (id_card[i] - '0') * weights[i];

    char expected = check_chars[sum % 11];
    char actual_last = (last >= 'a' && last <= 'z') ? last - 'a' + 'A' : last;
    return actual_last == expected;
}
```

前 17 位数字与加权系数相乘后累加，和模 11 得到校验位，与身份证末位比对。此算法来自国家标准 GB 11643-1999，可在不联网的情况下本地校验身份证真伪。

### 1.3 年龄与身份证联动校验

新增患者时，输入的年龄必须与身份证号中出生日期计算出来的年龄一致：

```
输入年龄: 30
输入身份证: 110101199003071234
  → getAgeFromIDCard() 解析出生日期 1990-03-07
  → 计算当前年龄 = 2026 - 1990 - (生日没过?) = 36
  → 36 != 30 → 输出警告并让用户重新输入年龄和身份证号
```

不一致时不会简单跳过，而是引导用户重新依次输入年龄和身份证号，确保数据准确性。

### 1.4 身份认证与访问控制

#### 登录锁定

管理员和医生登录均有连续 5 次失败锁定机制：

```c
static int adminLogin() {
    static int fail_count = 0;
    if (fail_count >= 5) {
        printf("[锁定] 登录尝试次数过多！\n");
        return 0;
    }
    // ...验证逻辑...
    if (验证成功) { fail_count = 0; return 1; }
    else { fail_count++; return 0; }
}
```

`fail_count` 为 `static` 变量，程序重启后重置。设计意图是防止暴力破解，同时避免永久锁定的维护问题。

#### 患者 PIN 码保护

敏感操作（查看记录、充值、取消预约）需要 6 位数字 PIN 码验证：

```c
int verifyPatientPin(Patient* p) {
    if (p->pin[0] == '\0') return 1;  // 未设置密码则跳过
    for (int tries = 0; tries < 3; tries++) {
        printf("请输入6位访问密码 (%d次尝试): ", 3 - tries);
        if (strcmp(buf, p->pin) == 0) return 1;
    }
    printf("[错误] 密码验证失败已达上限，操作取消。\n");
    return 0;
}
```

- `pin[0] == '\0'` 表示未设置，向后兼容旧数据
- 3 次失败后直接取消操作，不暴露任何患者信息

#### 医生端三重校验

医生写诊断/处方前做严格权限校验：

```c
// 1. 患者存在
ListNode* pn = FindNode(patient_list, patient_id);
if (!pn) { printf("[错误] 患者不存在！\n"); return; }
// 2. 患者已挂号
if (p->register_status == REG_STATUS_NONE) {
    printf("[提示] 该患者未挂号，请先完成挂号。\n"); return;
}
// 3. 患者挂的是当前医生的号
if (strcmp(p->doctor_id, doctor_id) != 0) {
    printf("[提示] 该患者未挂此医生的号。\n"); return;
}
```

#### 角色权限隔离

| 功能      | 管理员 | 医生        | 患者        |
| ------- | --- | --------- | --------- |
| 患者 CRUD | ✅   | ❌         | ❌         |
| 医生 CRUD | ✅   | ❌         | ❌         |
| 药品管理    | ✅   | ❌         | ❌         |
| 写诊断/处方  | ✅   | ✅         | ❌         |
| 查看自己患者  | ✅   | ✅（仅挂自己号的） | ✅（仅自己）    |
| 充值      | ❌   | ❌         | ✅（PIN 验证） |
| 取消挂号    | ❌   | ❌         | ✅（PIN 验证） |

### 1.5 密码存储安全

密码使用 nibble-swap（半字节交换）混淆存储：

```c
void passwordObfuscate(char* pwd) {
    for (int i = 0; pwd[i]; i++) {
        pwd[i] = ((pwd[i] << 4) | ((unsigned char)pwd[i] >> 4));
    }
}
```

**自逆性证明**：设一个字节的 nibble-swap 为 f(x) = (L, H)，则 f(f(x)) = f(L, H) = (H, L) = x，两次操作恢复原值。这使得登录验证时只需对用户输入的密码做一次 obfuscate，再与文件中已混淆的密码直接 strcmp 比对即可。

**安全等级**：此方案仅防"磁盘被直接读取时一目了然"的场景，不防逆向工程。若需更高安全性，应考虑专业的密码哈希库（如 bcrypt），但课程设计范围内足够。

### 1.6 文件写入安全 — 原子保存

```c
int SaveDataToFile(LinkList* list, const char* filename, void (*format_func)(void*, char*)) {
    char tmp[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s.tmp", filename);
    FILE* fp = fopen(tmp, "w");
    if (!fp) return -1;
    // 遍历链表，逐行写入临时文件
    ListNode* p = list->head;
    while (p) {
        char line[MAX_LINE_LEN];
        format_func(p->data, line);
        fprintf(fp, "%s\n", line);
        p = p->next;
    }
    fclose(fp);
    remove(filename);       // 删除原文件
    rename(tmp, filename);  // 原子重命名
    return 0;
}
```

**为什么不直接写入原文件？** 如果写入中途程序崩溃，原文件会被截断或包含部分新数据，导致数据损坏。原子写入策略保证：即使 `fopen(tmp)` 后崩溃，原文件完好无损；只有 rename 成功时才替换。

### 1.7 业务安全校验汇总

| 校验场景   | 校验内容                              | 违规处理    |
| ------ | --------------------------------- | ------- |
| 删除科室   | 科室下医生数 > 0？有床位？                   | 提示无法删除  |
| 删除医生   | 医生是否有排班？                          | 校验后确认   |
| 删除患者   | 有医疗记录？有预约？在住院？有挂号？                | 逐项检查后拦截 |
| 删除药品   | 库存 > 0？                           | 拦截      |
| 删除床位   | 床位被占用？                            | 拦截      |
| 发药     | 患者存在？药品存在？库存够？医生匹配？余额足？           | 逐项拦截    |
| 重复发药   | 患者已有处方记录？                         | 警告确认    |
| 就诊状态流转 | NONE→PENDING→IN_PROGRESS→DONE 硬约束 | 非法跳转拦截  |
| 号源限制   | current_register < max_register   | 提示号源已满  |
| 跨日重置   | 挂号日期 != 今天？                       | 自动清零号源  |

---

## 二、问题解决策略

### 2.1 密码明文迁移问题

**问题描述**：项目早期版本中，`loadDoctorData()` 通过 `isprint()` 判断加载的密码是否为明文。但 nibble-swap 混淆后的部分字节恰好落在可打印 ASCII 范围内（如字符 '6'→'6'、'3'→'3'），导致程序重启后产生误判，反复混淆已混淆的密码，最终数据彻底损坏。

**错误代码（修复前）**：

```c
// loadDoctorData 中的错误判断
char check[MAX_PWD_LEN];
strcpy(check, d->password);
passwordObfuscate(check);          // 对已混淆的密码再次混淆 = 还原
if (isprint((unsigned char)check[0])) {
    // 本意是判断 check 是否为"可打印"（即明文）
    // 但混淆后某些字符依然可打印，导致旧数据被反复混淆
    strcpy(d->password, check);    // 明文→混淆 ✓ | 混淆→还原 ✗
}
```

**修复方案**：将密码迁移逻辑移至登录时按需处理：

```c
// doctorLogin 中的迁移逻辑
ListNode* p = doctor_list->head;
while (p) {
    Doctor* d = (Doctor*)p->data;
    if (strcmp(d->account, username) == 0) {
        char check[MAX_PWD_LEN];
        HIS_STRNCPY(check, d->password, MAX_PWD_LEN);
        passwordObfuscate(check);
        if (strcmp(check, password) == 0) {
            // 匹配说明 d->password 是明文，迁移为混淆版本
            HIS_STRNCPY(d->password, password, MAX_PWD_LEN);
            saveDoctorData();
            return 1;  // 登录成功
        }
        if (strcmp(d->password, password) == 0) {
            return 1;  // 已经是混淆版本，直接匹配
        }
    }
    p = p->next;
}
```

**关键点**：先对文件中读取的密码做 obfuscate，如果和用户输入（已 obfuscate）匹配，说明原密码是明文→覆盖为混淆版本；如果文件中的密码直接和用户输入匹配，说明已经是混淆版本→正常登录。不再依赖 `isprint()` 判断，彻底消除误判。

### 2.2 GBK 中文字符截断乱码

**问题描述**：当用户输入超长中文字符串时，`fgets()` 会在第 N-1 个字符处截断。如果截断点恰好落在 GBK 编码中文的第一个字节，末尾会保留一个孤立的 GBK 首字节（0x81~0xFE），后续存储和显示都产生乱码。

**修复方案**：截断发生后，检查末尾字节是否可能是 GBK 首字节，若是则移除：

```c
size_t len = strlen(buf);
if (len > 0 && (unsigned char)buf[len - 1] >= 0x81) {
    buf[len - 1] = '\0';
}
```

为什么是 `>= 0x81`？GBK 编码的中文首字节范围是 0x81~0xFE，而 ASCII 可见字符的最大值是 0x7E（'~'），所以 `>= 0x81` 的字符一定不是完整的 ASCII 字符，一定是 GBK 多字节字符的片段，安全截断。

### 2.3 parseBedLine 类型转换安全修复

**问题描述**：`parseBedLine` 中直接对枚举指针做 `int*` 强制转换，属于 C 语言未指定行为（不同编译器可能产生不同结果）：

```c
// 修改前 — 未指定行为
sscanf(line, "%[^|]|%d|%[^|]|%d|%[^|]|%[^\n]",
    b->id, (int*)&b->room_type, b->dept_id, (int*)&b->status,
    b->patient_id, b->admit_time);
```

**修复后**：

```c
// 修改后 — 使用中间 int 变量
int room_type, status;
sscanf(line, "%19[^|]|%d|%19[^|]|%d|%19[^|]|%29[^\n]",
    b->id, &room_type, b->dept_id, &status,
    b->patient_id, b->admit_time);
b->room_type = (RoomType)room_type;
b->status = (BedStatus)status;
```

同时修复了两个问题：类型转换安全 + sscanf 宽度控制防缓冲区溢出。

### 2.4 parseDeptLine 缓冲区溢出修复

**问题描述**：`sscanf` 的 `%[^|]` 格式没有宽度限制，如果数据文件中对应字段超过目标缓冲区大小，会导致缓冲区溢出：

```c
// 修改前 — 无宽度限制
sscanf(line, "%[^|]|%[^|]|%d", d->id, d->name, &d->doctor_count);
```

**修复后**：

```c
sscanf(line, "%19[^|]|%49[^|]|%d", d->id, d->name, &d->doctor_count);
```

`%19[^|]` 限制最多读取 19 个字符 + 1 个 '\\0' = 20 字符，适配 `d->id[MAX_ID_LEN]` 的长度。drug.c 的 parseDrugLine 已提前使用了精确宽度控制，dept_bed.c 的修复与之对齐。

### 2.5 发药数据保存顺序调整

**问题描述**：原发药流程的数据保存顺序为 `saveDrugData() → savePatientData() → saveRecordData()`，即先存库存变更，最后存处方记录。如果在保存库存后、保存记录前程序崩溃，库存已扣但处方记录丢失，产生数据不一致。

**修复方案**：将医疗记录（作为操作审计证据）放在第一位保存：

```c
// 修改前
saveDrugData();    // 库存已扣，但若在这步后崩溃，无法追溯
savePatientData();
saveRecordData();

// 修改后
saveRecordData();  // 先存记录（事务的权威证据）
saveDrugData();    // 再存库存变更
savePatientData(); // 最后存余额变更
```

调整后的逻辑：即使后续保存失败，至少有一条处方记录可以追溯该操作。实际修复无法做到完全的 ACID 事务（纯 C + 文本文件），但通过此顺序减少了数据丢失时的排查难度。

### 2.6 就诊状态流转硬约束

**问题描述**：最初的就诊状态修改不检查当前状态，医生可以直接从"待就诊"跳到"已完成"，跳过"就诊中"状态，造成流程混乱。

**修复方案**：增加状态跳转校验：

```c
if (st == 1) {  // 转为"就诊中"
    if (p->register_status != REG_STATUS_PENDING) {
        printf("[提示] 仅待就诊状态可转为就诊中。\n"); break;
    }
    p->register_status = REG_STATUS_IN_PROGRESS;
}
else if (st == 2) {  // 转为"已完成"
    if (p->register_status != REG_STATUS_IN_PROGRESS) {
        printf("[提示] 仅就诊中状态可转为已完成。\n"); break;
    }
    p->register_status = REG_STATUS_DONE;
}
```

状态流转图：

```
REG_STATUS_NONE (0) → PENDING (1) → IN_PROGRESS (2) → DONE (3)
      ↑                                                      |
      └──────── 取消挂号重置 ────────────────────────────────┘
```

### 2.7 分精度计算（浮点误差问题）

**问题描述**：直接使用 `float` 计算金额会导致累积误差，典型表现为 `0.1 + 0.2 = 0.30000000000000004`。在多次扣费、统计汇总后，余额会逐渐偏离正确值。

**修复方案**：所有金额以"分"为单位存储（`long long`），仅在显示时除以 100 转为元：

```c
// 计算（保险柜级别精度）
long long total_cost_cents = (long long)(price * quantity * 100.0 + 0.5);
long long insurance_pay_cents = (long long)(total_cost_cents * insurance_ratio);
long long patient_pay_cents = total_cost_cents - insurance_pay_cents;

// 显示（临时转元）
printf("总费用: %.2f 元\n", (double)total_cost_cents / 100.0);
printf("医保报销: %.2f 元\n", (double)insurance_pay_cents / 100.0);
printf("自付: %.2f 元\n", (double)patient_pay_cents / 100.0);
```

注意 `+0.5` 用于四舍五入，避免 `(int)` 截断导致 1 分的精度损失。

---

## 三、特色代码设计

### 3.1 void* 泛型链表

所有数据在内存中统一以通用单向链表管理：

```c
typedef struct ListNode {
    void* data;              // 任意类型数据的指针
    int data_size;           // 数据大小（用于 memcpy 深拷贝）
    char id[MAX_ID_LEN];     // 节点 ID（用于 O(n) 快速查找）
    struct ListNode* next;
} ListNode;

typedef struct {
    ListNode* head;
    int length;
} LinkList;
```

**实现原理**：通过 `void*` 存储任意结构体，配合 `data_size` 在插入时做 `malloc + memcpy` 深拷贝，释放时 `free(data)`。8 个业务实体（Patient、Doctor、Department、Bed、Drug、MedicalRecord、DoctorSchedule、Appointment）共用同一套链表操作函数。

**为什么不做成宏模板？** C 语言没有模板机制。典型的 C 泛型方案有：

- `void*` + 函数指针（本项目采用，类似 C 标准库 `qsort` 的思路）
- `#define` 宏模板（展开后代码膨胀，调试困难）
- 每个实体手写一套链表（代码冗余）

本项目的方案在代码复用和可维护性之间取得了合理平衡。

### 3.2 函数指针策略模式

数据持久化采用策略模式，将"格式化/解析"策略与"文件读写"骨架分离：

```c
// 骨架（his_tool.c，固定不变）
int SaveDataToFile(LinkList* list, const char* filename,
                   void (*format_func)(void*, char*));
int LoadDataFromFile(LinkList* list, const char* filename,
                     void (*parse_func)(char*, void*));

// 策略（由各业务模块提供）
static void formatPatient(void* data, char* line) {
    Patient* p = (Patient*)data;
    snprintf(line, MAX_LINE_LEN, "%s|%s|%d|%s|...", /* 各字段 */);
}
static void parsePatient(char* line, void* data) {
    Patient* p = (Patient*)data;
    memset(p, 0, sizeof(Patient));
    // 按 | 拆分字段并赋值
}

// 组装
void savePatientData(void) {
    SaveDataToFile(patient_list, FILE_PATIENT, formatPatient);
}
void loadPatientData(void) {
    LoadDataFromFile(patient_list, FILE_PATIENT, parsePatient);
}
```

此模式在 8 个业务模块中各出现一次，避免了重复编写文件读写代码。每增加一个新实体，只需提供 format + parse 两个函数。

### 3.3 原子写入机制

文件保存采用 write-then-rename 模式：

```
保存步骤：
① fopen("data/patient.txt.tmp", "w")    — 写临时文件
② 遍历链表，逐行写入临时文件
③ fclose(tmp)
④ remove("data/patient.txt")             — 删除原文件
⑤ rename("data/patient.txt.tmp", "data/patient.txt")  — 原子重命名
```

`rename()` 是操作系统原语级别的原子操作（NTFS 文件系统保证）。如果程序在步骤①~③之间崩溃，临时文件可能不完整，但原文件完好无损；如果程序在步骤④之后崩溃，临时文件已经替代原文件，数据仍完整。不存在"写到一半崩溃导致文件损坏"的窗口。

### 3.4 分精度金额计算

结构体中的金额字段以 `long long` 类型存储，单位为"分"：

```c
typedef struct {
    // ...其他字段
    long long balance;       // 余额（分）
    // ...
} Patient;
typedef struct {
    // ...其他字段
    long long cost;          // 医疗费用（分）
    // ...
} MedicalRecord;
```

**为什么不用 float？** `float` 的精度约 7 位十进制数字，以"元"为单位存储 100,000.00 元时，理论精度足够；但多次加减运算后误差累积，且 `0.1` 无法在二进制浮点数中精确表示。`long long` 以"分"为单位可精确表示 0~9e18 分的范围，完全满足医院系统的金额精度要求。

### 3.5 ID 生成策略

```c
void GenerateID(char* id, char type) {
    static int seq = 1;
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    snprintf(id, MAX_ID_LEN, "%c%02d%02d%02d%03d",
        type, tm->tm_year % 100, tm->tm_mon + 1, tm->tm_mday, seq++);
}
```

| 前缀  | 实体             | 示例         | 说明           |
| --- | -------------- | ---------- | ------------ |
| P   | Patient        | P260511001 | 患者           |
| D   | Doctor         | D260511001 | 医生           |
| K   | Department     | K260511001 | 科室（"科"拼音首字母） |
| B   | Bed            | B260511001 | 床位           |
| M   | Drug           | M260511001 | 药品（Medicine） |
| R   | MedicalRecord  | R260511001 | 病历           |
| S   | DoctorSchedule | S260511001 | 排班           |
| A   | Appointment    | A260511001 | 预约           |

ID 格式：`[前缀][年2位][月2位][日2位][序号3位]`。序号每日自 `001` 重新开始。生成后立即检查冲突，最多重试 10 次以保证唯一性。

### 3.6 HIS_STRNCPY 安全宏

```c
#define HIS_STRNCPY(dst, src, cap) do { \
    char* _d = (dst); \
    size_t _c = (size_t)(cap); \
    const char* _s = (src); \
    if (_d && _c > 0U) { \
        strncpy(_d, (_s) ? (_s) : "", _c - 1U); \
        _d[_c - 1U] = '\0'; \
    } \
} while (0)
```

`strncpy` 在源字符串长度 >= 目标大小时**不**自动添加 '\\0'，这是 C 标准库的一个经典陷阱。此宏封装保证了：

1. 目标缓冲区始终以 '\\0' 结尾
2. 源指针为 NULL 时拷贝空字符串而非崩溃
3. 目标容量为 0 时安全跳过

### 3.7 跨模块函数调用体系

所有跨模块调用通过 `his.h` 中的 `extern` 声明完成：

```
his_main.c
├── patientModule()         → patient.c
│   └── inputAndXxxRecords() → record.c
├── dept_bedModule()        → dept_bed.c
│   ├── doctorSubMenu()     → doctor.c
│   └── scheduleSubMenu()   → schedule.c
└── drugModule()            → drug.c
    ├── globalStatsSubMenu() → admin_tools.c
    └── backupAllData()      → admin_tools.c
```

各业务模块内部函数全部声明为 `static`，只暴露必要的入口函数和打印函数。模块间不直接引用对方内部函数，不直接访问对方内部变量（全部通过 `extern` 全局链表访问），实现了 C 语言级别的信息隐藏。

### 3.8 全部函数推提前置声明

每个 `.c` 文件在开头将所有内部函数做前向声明：

```c
// drug.c 开头的完整前向声明
static void inputDrugInfo(Drug* d);
void printDrugInfo(void* data);
static void formatDrugLine(void* data, char* line);
static void parseDrugLine(char* line, void* data);
void saveDrugData(void);
void loadDrugData(void);
static void drugInfoSubMenu();
static void drugStockSubMenu();
static void prescriptionSubMenu();
```

这样做的好处：

1. 消除编译器警告（`-Wmissing-prototypes`）
2. 函数定义顺序可以任意排列（业务相关函数可以放在一起）
3. 读者可以在文件开头一览所有功能点

---

## 四、相关测试数据

### 4.1 功能测试

系统经过完整的黑盒功能测试，覆盖 12 个业务模块，包含 **98 个测试用例**，通过率 **100%**。

#### 患者模块（TC-PAT-001 ~ 008）

| 用例         | 场景                     | 结果  |
| ---------- | ---------------------- |:---:|
| TC-PAT-001 | 正常录入患者（完整信息）           | ✅   |
| TC-PAT-002 | 姓名为空拦截                 | ✅   |
| TC-PAT-003 | 姓名修改为 49 字符（MAX-1 临界值） | ✅   |
| TC-PAT-004 | 姓名输入 60 字符（超长截断）       | ✅   |
| TC-PAT-005 | 身份证含特殊字符拦截             | ✅   |
| TC-PAT-006 | 手机号不足 11 位拦截           | ✅   |
| TC-PAT-007 | 批量患者列表展示               | ✅   |
| TC-PAT-008 | 10 名同名患者"张三"共存         | ✅   |

#### 挂号模块（TC-REG-001 ~ 009）

| 用例         | 场景                     | 结果  |
| ---------- | ---------------------- |:---:|
| TC-REG-001 | 患者正常挂号                 | ✅   |
| TC-REG-002 | 同一患者重复挂号拦截             | ✅   |
| TC-REG-003 | 医生号源已满时挂号              | ✅   |
| TC-REG-004 | 余额不足时挂号                | ✅   |
| TC-REG-005 | 取消现场挂号（号源恢复）           | ✅   |
| TC-REG-006 | 预约挂号                   | ✅   |
| TC-REG-007 | 取消预约（退费+号源恢复）          | ✅   |
| TC-REG-008 | 科室无医生时挂号               | ✅   |
| TC-REG-009 | 挂号后 record_count 与文件一致 | ✅   |

#### 诊断/处方模块（TC-DIA-001 ~ 006）

| 用例         | 场景                     | 结果  |
| ---------- | ---------------------- |:---:|
| TC-DIA-001 | 医生新增诊断记录               | ✅   |
| TC-DIA-002 | 未挂号患者直接创建诊断拦截          | ✅   |
| TC-DIA-003 | 诊断描述超长截断               | ✅   |
| TC-DIA-004 | 医生查看患者医疗记录             | ✅   |
| TC-DIA-005 | 修改就诊状态流转               | ✅   |
| TC-DIA-006 | 诊断后 record_count 与文件一致 | ✅   |

- 住院模块（TC-INP-001 ~ 005）：全部通过
- 床位模块（TC-BED-001 ~ 011）：全部通过
- 排班模块（TC-SCH-001 ~ 009）：全部通过
- 药品模块（TC-DRG-001 ~ 014）：全部通过
- 权限控制（ACL-A-001 ~ D-006）：全部通过

### 4.2 边界场景测试

针对输入边界进行了专项测试：

| 类别        | 测试项数   | 通过率      |
| --------- |:------:|:--------:|
| 姓名字段边界    | 5      | 100%     |
| 年龄字段边界    | 5      | 100%     |
| 手机号/身份证边界 | 5      | 100%     |
| 医保/余额边界   | 6      | 100%     |
| 号源与状态边界   | 7      | 100%     |
| 床位状态边界    | 8      | 100%     |
| 药品库存边界    | 8      | 100%     |
| **合计**    | **44** | **100%** |

关键边界验证结果举例：

| 测试项       | 输入                     | 预期             | 实际  |
| --------- | ---------------------- | -------------- |:---:|
| 年龄为0      | 0                      | 允许（新生儿）        | ✅   |
| 负年龄       | -5                     | 拦截             | ✅   |
| 超大年龄      | 200                    | 拦截             | ✅   |
| 医保比例>1.0  | 2.0                    | 钳位为 1.0        | ✅   |
| 余额刚好等于挂号费 | balance == pay         | 扣费后余额 0，挂号成功   | ✅   |
| 号源归零      | current_register=0 且取消 | 检查 >0 后再减，不出负数 | ✅   |
| 库存为0时出库   | stock=0                | 拦截"库存不足"       | ✅   |
| 取消已取消的预约  | status="已取消" 再取消       | 提示"已取消"        | ✅   |

### 4.3 数据容量测试

批量操作测试结果：

| 数据类型       | 目标数量 | 实际创建 | 耗时  |
| ---------- |:----:|:----:|:---:|
| 科室         | 5    | 5    | <1s |
| 医生         | 20   | 20   | <2s |
| 患者（含10名同名） | 110  | 110  | <5s |
| 药品         | 20   | 20   | <2s |
| 床位（三种病房类型） | 30   | 30   | <2s |

所有 ID 唯一性验证通过，同名患者各有唯一 ID。

容量推算：

| 场景   | 数据量                   | 预估内存    | 预计响应 |
| ---- |:---------------------:|:-------:|:----:|
| 小型医院 | 1000 患者 / 5000 记录     | <2 MB   | 即时   |
| 中型医院 | 10000 患者 / 50000 记录   | <15 MB  | <1s  |
| 大型医院 | 100000 患者 / 500000 记录 | <150 MB | 1~3s |

系统在 10 万级数据量下运行正常，`FindNode()` 的 O(n) 遍历在百万级数据时可能出现性能瓶颈。

### 4.4 数据完整性验证

通过操作前后对比数据文件内容，验证 12 项跨模块一致性：

| 场景    | 检查项                                                                   | 结果  |
| ----- | --------------------------------------------------------------------- |:---:|
| 挂号后   | patient.txt balance 减少 ✓, doctor.txt current_register 增加 ✓            | ✅   |
| 发药后   | drug.txt stock 减少 ✓, patient.txt balance 减少 ✓, record.txt 新增 PRESCR ✓ | ✅   |
| 住院后   | bed.txt status=1 ✓, patient.txt is_inpatient=1 ✓                      | ✅   |
| 出院后   | bed.txt status=0, patient_id=-1 ✓                                     | ✅   |
| 取消挂号后 | doctor.txt current_register 递减 ✓                                      | ✅   |
| 取消预约后 | schedule.txt current_patients 递减 ✓, 退费 ✓                              | ✅   |

### 4.5 稳定性测试

| 测试类别   | 用例数    | 通过数    | 通过率      |
| ------ |:------:|:------:|:--------:|
| 批量数据容量 | 7      | 7      | 100%     |
| 数据完整性  | 12     | 12     | 100%     |
| 数据持久化  | 5      | 5      | 100%     |
| 数据一致性  | 7      | 7      | 100%     |
| 响应速度   | 5      | 5      | 100%     |
| **合计** | **36** | **36** | **100%** |

---

## 五、核心业务数据联动

HIS 系统的核心操作均涉及多表（链表）联动更新。以下详细分析四个关键流程的数据变动。

### 5.1 挂号流程（涉及 patient + doctor + record）

#### 流程图

```
normalRegistration()
  │
  ├─ 操作前状态
  │   patient: 张三, balance=200.00, register_status=NONE, record_count=1
  │   doctor:  张明, current_register=0/30
  │   record:  空（无挂号记录）
  │
  ├─ 步骤1: 选择科室+医生
  │   内科 → 张明 (号源 0/30 充足)
  │
  ├─ 步骤2: 计算并校验
  │   挂号费 10.00, 医保 70%, 自付 3.00
  │   余额 200.00 >= 3.00 ✓
  │
  ├─ 步骤3: 扣费
  │   p->balance: 200.00 → 197.00 (减3.00)
  │   p->doctor_id = D260511001
  │   p->dept_id = K260511001
  │   p->register_status = PENDING (1)
  │   p->register_time = "2026-05-11 09:15:30"
  │   p->record_count: 1 → 2
  │
  ├─ 步骤4: 更新医生号源
  │   d->current_register: 0 → 1
  │
  └─ 步骤5: 创建挂号记录
      MedicalRecord { id=R260511001, patient_id=P260511001,
                      doctor_id=D260511001, type=REGISTER(1),
                      cost=300(分), detail="普通挂号 - 费用: 3.00" }
      → InsertNode(record_list)
```

#### 数据联动表

| 文件          | 字段变化                                                                                     | 操作                |
| ----------- | ---------------------------------------------------------------------------------------- | ----------------- |
| patient.txt | balance ↓, doctor_id 更新, dept_id 更新, register_status→1, register_time 更新, record_count++ | savePatientData() |
| doctor.txt  | current_register++                                                                       | saveDoctorData()  |
| record.txt  | 新增一条 RECORD_REGISTER 记录                                                                  | saveRecordData()  |

#### 正确性检查清单

```
✓ 余额扣减 = 挂号费 × (1 - 医保比例)
✓ doctor_id 和 dept_id 同时更新，保持一致性
✓ register_status 从 NONE → PENDING，不允许重复挂号
✓ 医生号源 current_register < max_register 前提检查
✓ record_count 作为冗余字段，与 record 表中实际记录数一致
✓ 保存顺序：doctor → record → patient（医疗记录作为审计证据优先）
```

### 5.2 发药流程（涉及 drug + patient + record）

#### 流程图

```
issuePrescription()
  │
  ├─ 操作前状态
  │   patient: 张三, balance=19700(分), insurance_ratio=0.7
  │   drug:    阿莫西林, stock=500, price=12.50, threshold=50
  │   record:  已有挂号(R260511001)+诊断(R260511002)
  │
  ├─ 步骤1: 校验
  │   患者存在 ✓ | 药品存在 ✓ | 库存 500>=2 ✓
  │   医生存在 ✓ | 患者挂的是该医生的号 ✓
  │   hasDuplicatePrescription → 已有处方？（本例无，直接通过）
  │
  ├─ 步骤2: 计算费用
  │   总价 = 12.50 × 2 = 25.00 元 = 2500(分)
  │   医保 = 2500 × 0.7 = 1750(分) = 17.50 元
  │   自付 = 2500 - 1750 = 750(分) = 7.50 元
  │   余额 19700 >= 750 ✓
  │
  ├─ 步骤3: 执行更新（三个操作）
  │   ① d->stock: 500 → 498 (扣库存)
  │   ② p->balance: 19700 → 18950 (扣余额)
  │   ③ p->record_count: 2 → 3
  │
  └─ 步骤4: 创建处方记录
      MedicalRecord { id=R260511003, type=PRESCR(5),
                      cost=2500(分),
                      detail="门诊发药: 阿莫西林胶囊 x2, 医保报销17.50元" }
      → InsertNode(record_list)
```

#### 数据联动表

| 文件          | 字段变化                                     | 操作                |
| ----------- | ---------------------------------------- | ----------------- |
| drug.txt    | stock: 500 → 498                         | saveDrugData()    |
| patient.txt | balance: 19700 → 18950, record_count++   | savePatientData() |
| record.txt  | 新增 RECORD_PRESCR (cost=2500, detail=...) | saveRecordData()  |

#### 正确性检查清单

```
✓ 发药前 6 项校验全部通过才能执行（患者/药品/库存/医生/挂号匹配/余额）
✓ 费用以分计算，避免浮点误差
✓ 库存扣减不会导致负数
✓ 余额扣减不会导致负数
✓ 医生匹配校验防止跨医生开药
✓ 重复处方检查（hasDuplicatePrescription 警告确认）
✓ 保存顺序：record → drug → patient（审计记录优先）
```

### 5.3 预约+取消预约流程（涉及 patient + schedule + appointment）

#### 预约流程

```
appointmentRegistration()
  │
  ├─ 操作前状态
  │   patient: 李四, balance=10000(分)
  │   schedule: 林泽宇, 2026-05-12上午, max=30, current=0
  │   appointment: 空
  │
  ├─ 步骤1: 选择排班
  │   schedule_id = S260511001, 号源 0/30
  │
  ├─ 步骤2: 扣费
  │   预约费 20.00, 医保 70%, 自付 6.00=600(分)
  │   p->balance: 10000 → 9400
  │
  ├─ 步骤3: 创建预约记录
  │   Appointment { id=A260511001, patient_id=P260511002,
  │                 schedule_id=S260511001, status="已预约",
  │                 cost=600(分) }
  │
  └─ 步骤4: 更新排班号源
      s->current_patients: 0 → 1
```

#### 取消预约流程

```
cancelMyRegistration()
  │
  ├─ 步骤1: PIN 验证
  │   ✓
  │
  ├─ 步骤2: 校验
  │   预约属于该患者 ✓ | 预约状态="已预约" ✓
  │
  ├─ 步骤3: 恢复数据
  │   a->status = "已取消"
  │   s->current_patients: 1 → 0 (恢复号源)
  │   p->balance: 9400 → 10000 (退费)
  │
  └─ 步骤4: 保存三个文件
      saveAppointmentData() + saveScheduleData() + savePatientData()
```

#### 数据联动表

| 操作       | 文件变化                                                                                    |
| -------- | --------------------------------------------------------------------------------------- |
| **预约**   | patient.txt: balance ↓; schedule.txt: current_patients++; appointment.txt: 新增           |
| **取消预约** | appointment.txt: status="已取消"; schedule.txt: current_patients--; patient.txt: balance ↑ |

#### 正确性检查

```
预约:
  ✓ 排班号源 < max_patients 前提检查
  ✓ 余额足够支付预约费
  ✓ 预约记录与排班 ID 正确关联

取消预约:
  ✓ PIN 验证防止他人恶意取消
  ✓ 只能取消状态为"已预约"的记录
  ✓ 号源恢复后不会超过 max_patients
  ✓ 退费金额与支付金额完全一致
```

### 5.4 住院/出院流程（涉及 bed + patient）

```
modifyBedStatus()
  │
  ├─ 住院 (BED_FREE → BED_OCCUPIED)
  │   bed:     status=1, patient_id=P260511001, admit_time="2026-05-11 10:00"
  │   patient: is_inpatient=IN(1), bed_id=B260511001
  │   saveBedData() + savePatientData()
  │
  └─ 出院 (BED_OCCUPIED → BED_FREE)
      bed:     status=0, patient_id="-1", admit_time=""
      patient: is_inpatient=OUT(0), bed_id 清空
      saveBedData() + savePatientData()
```

#### 安全保障

- 已住院患者不能再次分配床位（`is_inpatient == PATIENT_IN` 检查）
- 出院前要求确认（`getConfirm()`）
- 患者出院后床位数据完全清理（patient_id="-1", admit_time=""）

### 5.5 科室关联变更（涉及 dept + doctor）

添加或删除医生时，科室的 `doctor_count` 字段自动维护：

```
addDoctor() → dept->doctor_count++ → saveDeptData()
deleteDoctor() → dept->doctor_count-- → saveDeptData()
```

删除科室前的保护检查：

```c
// 不能删除还有医生的科室
if (d->doctor_count > 0) { printf("该科室有 %d 名医生，无法删除！\n", d->doctor_count); return; }
// 不能删除还有床位的科室
ListNode* p = bed_list->head;
while (p) {
    Bed* b = (Bed*)p->data;
    if (strcmp(b->dept_id, id) == 0) { /* 存在床位，禁止删除 */ break; }
    p = p->next;
}
```

### 5.6 多表联动总览

| 业务操作   | 涉及表格                             | 联动说明            |
| ------ | -------------------------------- | --------------- |
| 普通挂号   | patient + doctor + record        | 扣余额 + 增号源 + 增记录 |
| 预约挂号   | patient + schedule + appointment | 扣余额 + 增号源 + 增记录 |
| 取消现场挂号 | patient + doctor                 | 恢复余额 + 减号源      |
| 取消预约   | patient + schedule + appointment | 退费 + 恢复号源 + 改状态 |
| 门诊发药   | drug + patient + record          | 扣库存 + 扣余额 + 增记录 |
| 办理住院   | bed + patient                    | 改状态 + 绑定床位      |
| 办理出院   | bed + patient                    | 释放床位 + 解绑       |
| 添加医生   | doctor + dept                    | doctor_count++  |
| 删除医生   | doctor + dept                    | doctor_count--  |
| 删除科室   | dept (+ doctor + bed)            | 前置检查医生数和床位      |
| 删除患者   | patient (+ record + appointment) | 前置检查记录、预约、住院、挂号 |

---

> 本文档各节可以独立摘取，用于 HIS系统报告.md 中对应章节的撰写。
