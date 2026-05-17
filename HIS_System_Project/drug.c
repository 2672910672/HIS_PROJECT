#include "his.h"

/*
 * 药品管理模块
 *   信息管理: addDrug / modifyDrug / deleteDrug / queryDrug
 *   库存管理: drugInbound / drugOutbound / viewStockWarning
 *   门诊发药: issuePrescription / hasDuplicatePrescription
 *   printDrugInfo        — 打印药品信息（跨模块调用）
 *   save/loadDrugData    — 文件持久化
 *   drugModule()         — 模块入口
 */
// --- 药品管理 ---
static void inputDrugInfo(Drug* d);                     //输入药品信息
void printDrugInfo(void* data);                         //打印药品信息
static void formatDrugLine(void* data, char* line);     //格式化药品信息为一行字符串
static void parseDrugLine(char* line, void* data);      //将一行药品文本转换为药品结构
// --- 文件持久化 ---
void saveDrugData(void);
void loadDrugData(void);
// --- 子菜单 ---
static void drugInfoSubMenu();                          //药品信息管理子菜单
static void drugStockSubMenu();                         //药品库存管理子菜单
static void prescriptionSubMenu();                      //开药发药子菜单

// ==================== 1. 药品基本信息管理相关函数 ====================
// 添加药品 (支持多科室)
static void addDrug() {
    Drug d;
    memset(&d, 0, sizeof(Drug));

    // 选择科室
    printf("\n--- 请选择所属科室 ---\n");
    if (dept_list->length == 0) {
        printf("[错误] 暂无科室，请先添加科室！\n");
        return;
    }
    TraverseList(dept_list, printDeptInfo);

    char dept_id[MAX_ID_LEN];
    printf("\n请输入所属科室ID: ");
    readString(dept_id, sizeof(dept_id));

    ListNode* dept_node = FindNode(dept_list, dept_id);
    if (!dept_node) {
        printf("\n[错误] 科室ID不存在！\n");
        return;
    }
    Department* dept = (Department*)dept_node->data;

    inputDrugInfo(&d);
    HIS_STRNCPY(d.dept_id, dept_id, MAX_ID_LEN);
    int retry = 0;
    do {
        GenerateID(d.id, ID_PREFIX_DRUG);
        if (++retry > 10) {
            printf("[错误] 无法生成唯一药品ID！\n");
            return;
        }
    } while (FindNode(drug_list, d.id) != NULL);

    if (InsertNode(drug_list, -1, &d, sizeof(Drug), d.id) == 0) {
        printf("\n[成功] 药品添加成功，药品ID: %s\n", d.id);
        saveDrugData();
    }
    else {
        printf("\n[失败] 药品添加失败！\n");
    }
}

// 修改药品信息
static void modifyDrug() {
    char id[MAX_ID_LEN];
    printf("\n请输入需要修改的药品ID: ");
    readString(id, sizeof(id));

    ListNode* drug_node = FindNode(drug_list, id);
    if (!drug_node) {
        printf("\n[错误] 未找到该药品！\n");
        return;
    }
    Drug* d = (Drug*)drug_node->data;

    int choice;
    while (1) {
        printf("\n====== 修改药品信息 ======\n");
        printDrugInfo(d);
        PrintSeparator();
        printf("  1. 修改通用名 (当前: %s)\n", d->general_name);
        printf("  2. 修改商品名 (当前: %s)\n", strlen(d->trade_name) ? d->trade_name : "(空)");
        printf("  3. 修改别名 (当前: %s)\n", strlen(d->alias) ? d->alias : "(空)");
        printf("  4. 修改所属科室 (当前: %s)\n", d->dept_id);
        printf("  5. 修改单价 (当前: %.2f)\n", d->price);
        printf("  6. 修改库存 (当前: %d)\n", d->stock);
        printf("  7. 修改预警阈值 (当前: %d)\n", d->warning_threshold);
        printf("  0. 保存并返回\n");
        PrintSeparator();
        printf("请选择要修改的字段: ");
        choice = getValidChoice(0, 7);

        if (choice == 0) break;

        char buf[MAX_LINE_LEN];
        switch (choice) {
        case 1:
            printf("请输入新通用名: ");
            inputLine(buf, sizeof(buf));
            if (!ValidateNoPipe(buf)) { printf("[错误] 通用名不能包含分隔符'|'！\n"); break; }
            if (strlen(buf) == 0) { printf("[错误] 通用名不能为空！\n"); break; }
            HIS_STRNCPY(d->general_name, buf, sizeof(d->general_name));
            printf("[成功] 通用名已更新。\n");
            break;

        case 2:
            printf("请输入新商品名 (直接回车留空): ");
            inputLine(buf, sizeof(buf));
            if (!ValidateNoPipe(buf)) { printf("[错误] 商品名不能包含分隔符'|'！\n"); break; }
            HIS_STRNCPY(d->trade_name, buf, sizeof(d->trade_name));
            printf("[成功] 商品名已更新。\n");
            break;

        case 3:
            printf("请输入新别名 (直接回车留空): ");
            inputLine(buf, sizeof(buf));
            if (!ValidateNoPipe(buf)) { printf("[错误] 别名不能包含分隔符'|'！\n"); break; }
            HIS_STRNCPY(d->alias, buf, sizeof(d->alias));
            printf("[成功] 别名已更新。\n");
            break;

        case 4:
            printf("\n--- 可选科室 ---\n");
            TraverseList(dept_list, printDeptInfo);
            printf("请输入新科室ID: ");
            inputLine(buf, sizeof(buf));
            if (strlen(buf) == 0) { printf("[错误] 科室ID不能为空！\n"); break; }
            if (!FindNode(dept_list, buf)) { printf("[错误] 科室ID不存在！\n"); break; }
            HIS_STRNCPY(d->dept_id, buf, sizeof(d->dept_id));
            printf("[成功] 所属科室已更新。\n");
            break;

        case 5:
            printf("请输入新单价: ");
            inputLine(buf, sizeof(buf));
            {
                float val = atof(buf);
                if (val < 0) { printf("[错误] 单价不能为负数！\n"); break; }
                d->price = val;
                printf("[成功] 单价已更新。\n");
            }
            break;

        case 6:
            printf("请输入新库存: ");
            inputLine(buf, sizeof(buf));
            {
                int val = atoi(buf);
                if (val < 0) { printf("[错误] 库存不能为负数！\n"); break; }
                d->stock = val;
                printf("[成功] 库存已更新。\n");
            }
            break;

        case 7:
            printf("请输入新预警阈值: ");
            inputLine(buf, sizeof(buf));
            {
                int val = atoi(buf);
                if (val < 0) { printf("[错误] 阈值不能为负数！\n"); break; }
                d->warning_threshold = val;
                printf("[成功] 预警阈值已更新。\n");
            }
            break;

        default:
            printf("[错误] 无效选择！\n");
        }
    }

    printf("\n[成功] 药品信息修改成功！\n");
    saveDrugData();
}

// 删除药品 (校验：库存为0)
static void deleteDrug() {
    char id[MAX_ID_LEN];
    printf("\n请输入需要删除的药品ID (输入0取消): ");
    readString(id, sizeof(id));
    if (strlen(id) == 0 || strcmp(id, "0") == 0) {
        printf("\n[取消] 已取消删除操作。\n");
        return;
    }

    ListNode* drug_node = FindNode(drug_list, id);
    if (!drug_node) {
        printf("\n[错误] 未找到该药品！\n");
        return;
    }
    Drug* d = (Drug*)drug_node->data;

    if (d->stock > 0) {
        printf("\n[错误] 该药品库存为 %d，无法删除！\n", d->stock);
        return;
    }

    printf("\n[确认] 确定要删除药品 %s (%s) 吗？(y/n): ", d->general_name, d->id);
    if (getConfirm()) {
        if (DeleteNode(drug_list, id) == 0) {
            printf("\n[成功] 药品删除成功！\n");
            saveDrugData();
        }
    }
    else {
        printf("\n[取消] 已取消删除操作。\n");
    }
}

// 查询药品 (支持多种模糊查询)
static void queryDrug() {
    printf("\n--- 药品查询 ---\n");
    printf("1. 按ID精确查询\n");
    printf("2. 按关键词模糊查询 (通用名/商品名/别名)\n");
    printf("3. 按所属科室查询\n");
    printf("4. 列出所有药品\n");
    printf("请选择: ");

    int choice = getValidChoice(1, 4);

    if (choice == 1) {
        char id[MAX_ID_LEN];
        printf("\n请输入药品ID: ");
        readString(id, sizeof(id));

        ListNode* drug_node = FindNode(drug_list, id);
        if (drug_node) {
            Drug* d = (Drug*)drug_node->data;
            printf("\n--- 查询结果 ---\n");
            printDrugInfo(d);
        }
        else {
            printf("\n[错误] 未找到该药品！\n");
        }
    }
    else if (choice == 2) {
        char keyword[MAX_NAME_LEN];
        printf("\n请输入查询关键词: ");
        inputLine(keyword, sizeof(keyword));

        printf("\n--- 包含 \"%s\" 的药品列表 ---\n", keyword);
        int found = 0;
        ListNode* p = drug_list->head;
        while (p) {
            Drug* d = (Drug*)p->data;
            if (strstr(d->general_name, keyword) ||
                strstr(d->trade_name, keyword) ||
                strstr(d->alias, keyword)) {
                printDrugInfo(d);
                found = 1;
            }
            p = p->next;
        }
        if (!found) printf("未找到匹配药品。\n");
    }
    else if (choice == 3) {
        char dept_id[MAX_ID_LEN];
        printf("\n请输入科室ID: ");
        readString(dept_id, sizeof(dept_id));

        printf("\n--- 科室 %s 的药品列表 ---\n", dept_id);
        int found = 0;
        ListNode* p = drug_list->head;
        while (p) {
            Drug* d = (Drug*)p->data;
            if (strcmp(d->dept_id, dept_id) == 0) {
                printDrugInfo(d);
                found = 1;
            }
            p = p->next;
        }
        if (!found) printf("暂无该科室的药品。\n");
    }
    else if (choice == 4) {
        printf("\n--- 所有药品列表 ---\n");
        if (drug_list->length == 0) {
            printf("暂无药品数据。\n");
        }
        else {
            TraverseList(drug_list, printDrugInfo);
        }
    }
}

// ==================== 2. 药品库存管理相关函数 ====================
// 药品入库
static void drugInbound() {
    char id[MAX_ID_LEN];
    printf("\n请输入药品ID: ");
    readString(id, sizeof(id));

    ListNode* drug_node = FindNode(drug_list, id);
    if (!drug_node) {
        printf("\n[错误] 未找到该药品！\n");
        return;
    }
    Drug* d = (Drug*)drug_node->data;

    printf("\n当前药品: %s | 当前库存: %d | 预警阈值: %d\n",
        d->general_name, d->stock, d->warning_threshold);

    int quantity;
    char buf_q[64];
    while (1) {
        printf("请输入入库数量: ");
        readString(buf_q, sizeof(buf_q));
        if (strlen(buf_q) == 0) continue;
        quantity = atoi(buf_q);
        if (quantity > 0) break;
        printf("[错误] 输入无效，请重新输入: ");
    }

    d->stock += quantity;
    printf("\n[成功] 入库成功，当前库存: %d\n", d->stock);

    // 操作记录
    {
        char now[30];
        GetSystemTime(now);
        printf("  [操作记录] %s | 入库 %s x%d | 经办人: 管理员\n", now, d->general_name, quantity);
    }

    // 预警提示
    if (d->stock < d->warning_threshold) {
        printf("\n[警告] 当前库存 (%d) 低于预警阈值 (%d)，请及时补货！\n",
            d->stock, d->warning_threshold);
    }

    saveDrugData();
}

// 药品出库
static void drugOutbound() {
    char id[MAX_ID_LEN];
    printf("\n请输入药品ID: ");
    readString(id, sizeof(id));

    ListNode* drug_node = FindNode(drug_list, id);
    if (!drug_node) {
        printf("\n[错误] 未找到该药品！\n");
        return;
    }
    Drug* d = (Drug*)drug_node->data;

    printf("\n当前药品: %s | 当前库存: %d\n", d->general_name, d->stock);

    int quantity;
    char buf_q[64];
    while (1) {
        printf("请输入出库数量: ");
        readString(buf_q, sizeof(buf_q));
        if (strlen(buf_q) == 0) continue;
        quantity = atoi(buf_q);
        if (quantity > 0) break;
        printf("[错误] 输入无效，请重新输入: ");
    }

    if (d->stock < quantity) {
        printf("\n[失败] 库存不足！当前库存: %d\n", d->stock);
        return;
    }

    d->stock -= quantity;
    printf("\n[成功] 出库成功，当前库存: %d\n", d->stock);

    // 操作记录
    {
        char now[30];
        GetSystemTime(now);
        printf("  [操作记录] %s | 出库 %s x%d | 经办人: 管理员\n", now, d->general_name, quantity);
    }

    if (d->stock < d->warning_threshold) {
        printf("\n[警告] 当前库存 (%d) 低于预警阈值 (%d)，请及时补货！\n",
            d->stock, d->warning_threshold);
    }

    saveDrugData();
}

// 查看库存预警
static void viewStockWarning() {
    printf("\n--- 库存预警药品列表 ---\n");
    int found = 0;
    ListNode* p = drug_list->head;
    while (p) {
        Drug* d = (Drug*)p->data;
        if (d->stock < d->warning_threshold) {
            printDrugInfo(d);
            found = 1;
        }
        p = p->next;
    }
    if (!found) printf("暂无药品缺货，无需补货。\n");
}

// ==================== 3. 发药开药相关函数 (模拟门诊) ====================
// 检查患者是否已有处方记录
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

static void issuePrescription() {
    char patient_id[MAX_ID_LEN], drug_id[MAX_ID_LEN], doctor_id[MAX_ID_LEN];
    int quantity;

    // 1. 输入并校验患者
    printf("\n--- 门诊发药 ---\n");
    printf("请输入患者ID: ");
    readString(patient_id, sizeof(patient_id));

    ListNode* pat_node = FindNode(patient_list, patient_id);
    if (!pat_node) {
        printf("\n[错误] 患者ID不存在！\n");
        return;
    }
    Patient* p = (Patient*)pat_node->data;

    // 检查是否已有处方记录（防重复发药）
    if (hasDuplicatePrescription(patient_id)) {
        printf("[警告] 该患者已有处方记录，确认继续发药？(y/n): ");
        if (!getConfirm()) {
            printf("\n[取消] 已取消发药操作。\n");
            return;
        }
    }

    // 选择药品
    printf("--- 请选择药品 ---\n");
    printf("请输入药品ID: ");
    readString(drug_id, sizeof(drug_id));

    ListNode* drug_node = FindNode(drug_list, drug_id);
    if (!drug_node) {
        printf("\n[错误] 药品ID不存在！\n");
        return;
    }
    Drug* d = (Drug*)drug_node->data;

    // 3. 输入数量并校验
    char buf_q[64];
    while (1) {
        printf("请输入发药数量: ");
        readString(buf_q, sizeof(buf_q));
        if (strlen(buf_q) == 0) continue;
        quantity = atoi(buf_q);
        if (quantity > 0) break;
        printf("[错误] 输入无效，请重新输入: ");
    }

    if (d->stock < quantity) {
        printf("\n[失败] 药品库存不足！当前库存: %d\n", d->stock);
        return;
    }

    // 4. 输入医生ID
    printf("请输入医生ID: ");
    readString(doctor_id, sizeof(doctor_id));

    // 校验医生ID是否存在
    ListNode* doc_check = FindNode(doctor_list, doctor_id);
    if (!doc_check) {
        printf("\n[错误] 医生ID不存在！\n");
        return;
    }
    // 校验患者是否挂过该医生的号
    if (strcmp(p->doctor_id, doctor_id) != 0) {
        printf("\n[错误] 该患者未挂该医生的号，无法为其发药！\n");
        return;
    }

    // 5. 计算费用（统一用分计算，避免浮点精度误差）
    long long total_cost_cents = (long long)(d->price * quantity * 100.0 + 0.5);
    long long insurance_pay_cents = (long long)(total_cost_cents * p->insurance_ratio);
    long long patient_pay_cents = total_cost_cents - insurance_pay_cents;

    printf("\n--- 费用明细 ---\n");
    printf("  药品: %s x%d\n", d->general_name, quantity);
    printf("  总费用: %.2f 元\n", (double)total_cost_cents / 100.0);
    printf("  医保报销 (%.0f%%): %.2f 元\n", p->insurance_ratio * 100, (double)insurance_pay_cents / 100.0);
    printf("  患者自付: %.2f 元\n", (double)patient_pay_cents / 100.0);
    printf("  患者当前余额: %.2f 元\n", (double)p->balance / 100.0);

    // 6. 校验余额
    if (p->balance < patient_pay_cents) {
        printf("\n[失败] 患者余额不足！需自付 %.2f 元，当前余额 %.2f 元\n",
            (double)patient_pay_cents / 100.0, (double)p->balance / 100.0);
        return;
    }

    // 7. 最终确认
    printf("\n[确认] 确认发药并扣费吗？(y/n): ");
    if (!getConfirm()) {
        printf("\n[取消] 已取消开药操作。\n");
        return;
    }

    // 8. 执行数据更新
    // 操作1: 扣减药品库存
    d->stock -= quantity;
    // 操作2: 扣减患者余额
    p->balance -= patient_pay_cents;
    // 操作3: 添加医疗记录
    MedicalRecord r;
    memset(&r, 0, sizeof(MedicalRecord));
    {
        int retry = 0;
        do {
            GenerateID(r.id, ID_PREFIX_RECORD);
            if (++retry > 10) {
                printf("[错误] 无法生成唯一发药记录ID！\n");
                return;
            }
        } while (FindNode(record_list, r.id) != NULL);
    }
    HIS_STRNCPY(r.patient_id, patient_id, MAX_ID_LEN);
    HIS_STRNCPY(r.doctor_id, doctor_id, MAX_ID_LEN);
    r.type = RECORD_PRESCR;
    r.cost = total_cost_cents;
    snprintf(r.detail, MAX_DETAIL_LEN, "门诊发药: %s x%d, 医保报销%.2f元",
        d->general_name, quantity, (double)insurance_pay_cents / 100.0);
    GetSystemTime(r.create_time);
    InsertNode(record_list, -1, &r, sizeof(MedicalRecord), r.id);
    p->record_count++;

    // 9. 保存所有数据（先存记录作为审计证据，再存财务数据）
    saveRecordData();  // 先存记录（事务的权威证据）
    saveDrugData();    // 再存库存变更
    savePatientData(); // 最后存余额变更

    printf("\n[成功] 发药成功！\n");
    printf("  患者余额已扣除 %.2f 元，当前余额: %.2f 元\n", (double)patient_pay_cents / 100.0, (double)p->balance / 100.0);
    printf("  药品库存已扣除 %d，当前库存: %d\n", quantity, d->stock);

    // 预警提示
    if (d->stock < d->warning_threshold) {
        printf("\n[警告] 药品 %s 库存 (%d) 低于预警阈值 (%d)！\n",
            d->general_name, d->stock, d->warning_threshold);
    }
}

// ==================== 4. 数据持久化相关函数 ====================
void saveDrugData(void) {
    SaveDataToFile(drug_list, FILE_DRUG, formatDrugLine);
}

void loadDrugData(void) {
    LoadDataFromFile(drug_list, FILE_DRUG, parseDrugLine);
}

// ==================== 5. 辅助函数实现 ====================
// 输入药品信息
static void inputDrugInfo(Drug* d) {
    // 通用名（必填，不允许 |）
    while (1) {
        printf("请输入药品通用名: ");
        inputLine(d->general_name, sizeof(d->general_name));
        if (!ValidateNoPipe(d->general_name)) { printf("[错误] 通用名不能包含分隔符'|'。\n"); continue; }
        if (strlen(d->general_name) == 0) { printf("[错误] 通用名不能为空！\n"); continue; }
        break;
    }

    // 商品名、别名（可选，但若有值则不能含 |）
    while (1) {
        printf("请输入药品商品名 (可选回车): ");
        inputLine(d->trade_name, sizeof(d->trade_name));
        if (!ValidateNoPipe(d->trade_name)) { printf("[错误] 商品名不能包含分隔符'|'。\n"); continue; }
        break;
    }

    while (1) {
        printf("请输入药品别名 (可选回车): ");
        inputLine(d->alias, sizeof(d->alias));
        if (!ValidateNoPipe(d->alias)) { printf("[错误] 别名不能包含分隔符'|'。\n"); continue; }
        break;
    }

    // 数字输入（带合法性校验）
    char buf_num[64];
    while (1) {
        printf("请输入单价: ");
        readString(buf_num, sizeof(buf_num));
        if (strlen(buf_num) == 0) continue;
        d->price = (float)atof(buf_num);
        if (d->price >= 0) break;
        printf("输入无效，请输入非负数: ");
    }

    while (1) {
        printf("请输入初始库存: ");
        readString(buf_num, sizeof(buf_num));
        if (strlen(buf_num) == 0) continue;
        d->stock = atoi(buf_num);
        if (d->stock >= 0) break;
        printf("输入无效，请输入非负数: ");
    }

    int default_threshold = (int)(d->stock * DRUG_WARNING_RATIO);
    if (default_threshold < 1) default_threshold = 1;
    printf("请输入库存预警阈值 (直接回车默认 %d，即库存的 %.0f%%): ", default_threshold, DRUG_WARNING_RATIO * 100);
    char buf_threshold[32];
    if (!inputLine(buf_threshold, sizeof(buf_threshold))) { d->warning_threshold = default_threshold; return; }
    if (strlen(buf_threshold) == 0) {
        d->warning_threshold = default_threshold;
    }
    else {
        d->warning_threshold = atoi(buf_threshold);
        while (d->warning_threshold < 0) {
            printf("输入无效，请输入非负数: ");
            if (!inputLine(buf_threshold, sizeof(buf_threshold))) break;
            d->warning_threshold = atoi(buf_threshold);
        }
    }
}

//打印药品信息
void printDrugInfo(void* data) {
    Drug* d = (Drug*)data;
    ListNode* dept_node = FindNode(dept_list, d->dept_id);
    Department* dept = dept_node ? (Department*)dept_node->data : NULL;

    // 库存状态判断
    const char* stock_status;
    if (d->stock == 0) stock_status = "[警告]缺货";
    else if (d->stock < d->warning_threshold) stock_status = "[警告]不足";
    else stock_status = "[正常]充足";

    printf("\n[药品ID: %s]\n", d->id);
    printf("  通用名: %-20s 商品名: %s\n", d->general_name, strlen(d->trade_name) > 0 ? d->trade_name : "-");
    printf("  别名: %-20s 所属科室: %s\n", strlen(d->alias) > 0 ? d->alias : "-", dept ? dept->name : "未知");
    printf("  单价: %.2f 元      库存: %d / 阈值: %d  [%s]\n",
        d->price, d->stock, d->warning_threshold, stock_status);
}

//格式化药品信息为一行字符串
static void formatDrugLine(void* data, char* line) {
    Drug* d = (Drug*)data;
    snprintf(line, MAX_LINE_LEN, "%s|%s|%s|%s|%.2f|%d|%d|%s",
        d->id, d->general_name, d->trade_name, d->alias,
        d->price, d->stock, d->warning_threshold, d->dept_id);
}

//将一行药品文本转换为药品结构
static void parseDrugLine(char* line, void* data) {
    Drug* d = (Drug*)data;
    memset(d, 0, sizeof(Drug));
    sscanf(line, "%19[^|]|%49[^|]|%49[^|]|%49[^|]|%f|%d|%d|%19[^\n]",
        d->id, d->general_name, d->trade_name, d->alias,
        &d->price, &d->stock, &d->warning_threshold, d->dept_id);
}

// ==================== 5. 子菜单与对外接口 ====================
//药品信息管理子菜单
static void drugInfoSubMenu() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("           药品信息管理 [子菜单]\n");
        PrintSeparator();
        printf("  1. 添加药品\n");
        printf("  2. 修改药品信息\n");
        printf("  3. 删除药品\n");
        printf("  4. 查询药品\n");
        printf("  0. 返回上级\n");
        PrintSeparator();
        printf("请输入选项: ");

        choice = getValidChoice(0, 4);

        switch (choice) {
        case 1: addDrug(); break;
        case 2: modifyDrug(); break;
        case 3: deleteDrug(); break;
        case 4: queryDrug(); break;
        case 0: return;
        default: printf("\n[错误] 无效选项\n");
        }
    }
}

//药品库存管理子菜单
static void drugStockSubMenu() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("           药品库存管理 [子菜单]\n");
        PrintSeparator();
        printf("  1. 药品入库\n");
        printf("  2. 药品出库\n");
        printf("  3. 查看库存预警\n");
        printf("  0. 返回上级\n");
        PrintSeparator();
        printf("请输入选项: ");

        choice = getValidChoice(0, 3);

        switch (choice) {
        case 1: drugInbound(); break;
        case 2: drugOutbound(); break;
        case 3: viewStockWarning(); break;
        case 0: return;
        default: printf("\n[错误] 无效选项\n");
        }
    }
}

// 开药发药子菜单
static void prescriptionSubMenu() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("           门诊发药 [子菜单]\n");
        PrintSeparator();
        printf("  1. 门诊发药 (模拟开药)\n");
        printf("  0. 返回上级\n");
        PrintSeparator();
        printf("请输入选项: ");

        choice = getValidChoice(0, 1);

        switch (choice) {
        case 1: issuePrescription(); break;
        case 0: return;
        default: printf("\n[错误] 无效选项\n");
        }
    }
}

// 模块唯一对外接口 (供主程序调用)
void drugModule() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("             药品管理与统计模块\n");
        PrintSeparator();
        printf("  1. 药品信息管理\n");
        printf("  2. 药品库存管理\n");
        printf("  3. 门诊发药\n");
        printf("  4. 全局查询统计\n");
        printf("  5. 一键数据备份\n");
        printf("  0. 返回主菜单\n");
        PrintSeparator();
        printf("请输入选项: ");

        choice = getValidChoice(0, 5);

        switch (choice) {
        case 1: drugInfoSubMenu(); break;
        case 2: drugStockSubMenu(); break;
        case 3: prescriptionSubMenu(); break;
        case 4: globalStatsSubMenu(); break;
        case 5: backupAllData(); break;
        case 0: return;
        default: printf("\n[错误] 无效选项\n");
        }
    }
}