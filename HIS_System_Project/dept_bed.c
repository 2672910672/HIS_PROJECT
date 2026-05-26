#include "his.h"

/*
 * 科室与床位管理模块
 *   科室: addDept / modifyDept / deleteDept / queryDept
 *   床位: addBed / modifyBedStatus / deleteBed / queryBed
 *   统计: statsSubMenu / calculateBedStats
 *   dept_bedModule() — 模块入口（含医生/排班子菜单跳转）
 */
// --- 科室内部 ---
static void inputDeptInfo(Department* d);
static void formatDeptLine(void* data, char* line);
static void parseDeptLine(char* line, void* data);
static Department* findDeptByID(const char* id);
static void addDept();
static void modifyDept();
static void deleteDept();
static void queryDept();
static void deptSubMenu();
// --- 床位内部 ---
static void inputBedInfo(Bed* b);
static void formatBedLine(void* data, char* line);
static void parseBedLine(char* line, void* data);
static void addBed();
static void modifyBedStatus();
static void deleteBed();
static void queryBed();
static void bedSubMenu();
// --- 统计 ---
static void calculateBedStats(const char* dept_id, int* total, int* occupied);
static void printBedStats(const char* title, int total, int occupied);
static void statsSubMenu();

// ==================== 1. 科室信息管理相关函数 ====================
static void addDept() {
    Department d;
    memset(&d, 0, sizeof(Department));
    inputDeptInfo(&d);
    int retry = 0;
    do {
        GenerateID(d.id, ID_PREFIX_DEPT);
        if (++retry > MAX_ID_RETRY) {
            printf("[错误] 无法生成唯一科室ID！\n");
            return;
        }
    } while (FindNode(dept_list, d.id) != NULL);
    d.doctor_count = 0;
    if (InsertNode(dept_list, -1, &d, sizeof(Department), d.id) == 0) {
        printf("\n[成功] 科室添加成功，科室ID: %s\n", d.id);
        saveDeptData();
    }
    else {
        printf("\n[失败] 科室添加失败！\n");
    }
}

static void modifyDept() {
    char id[MAX_ID_LEN] = { 0 };
    printf("\n请输入需要修改的科室ID: ");
    readString(id, sizeof(id));
    if (strlen(id) == 0) {
        printf("[错误] 输入ID无效！\n");
        return;
    }
    ListNode* node = FindNode(dept_list, id);
    if (!node) {
        printf("\n[错误] 未找到该科室！\n");
        return;
    }
    Department* d = (Department*)node->data;
    printf("\n当前科室信息:\n");
    printDeptInfo(d);
    printf("\n--- 请输入新的科室名称 ---\n");
    inputDeptInfo(d);
    printf("\n[成功] 科室信息修改成功！\n");
    saveDeptData();
}

static void deleteDept() {
    char id[MAX_ID_LEN];
    printf("\n请输入需要删除的科室ID (输入0取消): ");
    readString(id, sizeof(id));
    if (strlen(id) == 0 || strcmp(id, "0") == 0) {
        printf("\n[取消] 已取消删除操作。\n");
        return;
    }
    Department* d = findDeptByID(id);
    if (!d) { printf("\n[错误] 未找到该科室！\n"); return; }
    if (d->doctor_count > 0) {
        printf("\n[错误] 该科室有 %d 名医生，无法删除！\n", d->doctor_count);
        return;
    }
    int has_bed = 0;
    ListNode* p = bed_list->head;
    while (p) {
        Bed* b = (Bed*)p->data;
        if (strcmp(b->dept_id, id) == 0) { has_bed = 1; break; }
        p = p->next;
    }
    if (has_bed) { printf("\n[错误] 该科室下存在床位，无法删除！\n"); return; }
    printf("\n[确认] 确定要删除科室 %s (%s) 吗？(y/n): ", d->name, d->id);
    if (getConfirm()) {
        if (DeleteNode(dept_list, id) == 0) { printf("\n[成功] 科室删除成功！\n"); saveDeptData(); }
    }
    else { printf("\n[取消] 已取消删除操作。\n"); }
}

static void queryDept() {
    printf("\n--- 科室查询 ---\n");
    printf("1. 按ID精确查询\n");
    printf("2. 列出所有科室\n");
    printf("请选择: ");
    int choice = getValidChoice(1, 2);
    if (choice == 1) {
        char id[MAX_ID_LEN];
        printf("\n请输入科室ID: ");
        readString(id, sizeof(id));
        Department* d = findDeptByID(id);
        if (d) { printf("\n--- 查询结果 ---\n"); printDeptInfo(d); }
        else { printf("\n[错误] 未找到该科室！\n"); }
    }
    else if (choice == 2) {
        printf("\n--- 所有科室列表 ---\n");
        if (dept_list->length == 0) printf("暂无科室数据。\n");
        else TraverseList(dept_list, printDeptInfo);
    }
}

// ==================== 2. 床位管理相关函数 ====================
static void addBed() {
    Bed b;
    memset(&b, 0, sizeof(Bed));
    printf("\n--- 请选择所属科室 ---\n");
    if (dept_list->length == 0) { printf("[错误] 暂无科室，请先添加科室！\n"); return; }
    TraverseList(dept_list, printDeptInfo);
    char dept_id[MAX_ID_LEN];
    printf("\n请输入所属科室ID: ");
    readString(dept_id, sizeof(dept_id));
    ListNode* dept_node = FindNode(dept_list, dept_id);
    if (!dept_node) { printf("\n[错误] 科室ID不存在！\n"); return; }
    inputBedInfo(&b);
    HIS_STRNCPY(b.dept_id, dept_id, MAX_ID_LEN);
    int retry = 0;
    do {
        GenerateID(b.id, ID_PREFIX_BED);
        if (++retry > MAX_ID_RETRY) {
            printf("[错误] 无法生成唯一床位ID！\n");
            return;
        }
    } while (FindNode(bed_list, b.id) != NULL);
    b.status = BED_FREE;
    HIS_STRNCPY(b.patient_id, "-1", MAX_ID_LEN);
    b.admit_time[0] = '\0';
    if (InsertNode(bed_list, -1, &b, sizeof(Bed), b.id) == 0) {
        printf("\n[成功] 床位添加成功，床位ID: %s\n", b.id);
        saveBedData();
    }
    else { printf("\n[失败] 床位添加失败！\n"); }
}

static void modifyBedStatus() {
    char bed_id[MAX_ID_LEN];
    printf("\n请输入床位ID: ");
    readString(bed_id, sizeof(bed_id));
    ListNode* bed_node = FindNode(bed_list, bed_id);
    if (!bed_node) { printf("\n[错误] 未找到该床位！\n"); return; }
    Bed* b = (Bed*)bed_node->data;
    printf("\n当前床位状态:\n");
    printBedInfo(b);
    if (b->status == BED_FREE) {
        char patient_id[MAX_ID_LEN];
        printf("\n--- 办理住院 ---\n");
        printf("请输入患者ID: ");
        readString(patient_id, sizeof(patient_id));
        ListNode* patient_node = FindNode(patient_list, patient_id);
        if (!patient_node) { printf("\n[错误] 患者ID不存在！\n"); return; }
        Patient* p = (Patient*)patient_node->data;
        if (p->is_inpatient == PATIENT_IN) {
            printf("\n[错误] 患者 %s 已处于住院状态，请先办理出院后再安排住院。\n", patient_id);
            return;
        }
        b->status = BED_OCCUPIED;
        HIS_STRNCPY(b->patient_id, patient_id, MAX_ID_LEN);
        HisGetSystemTime(b->admit_time);
        p->is_inpatient = PATIENT_IN;
        HIS_STRNCPY(p->bed_id, bed_id, MAX_ID_LEN);
        printf("\n[成功] 住院办理成功，患者 %s 已入住床位 %s\n", patient_id, bed_id);
        saveBedData();
        savePatientData();
    }
    else {
        printf("\n--- 办理出院 ---\n");
        printf("[确认] 确定要为患者 %s 办理出院吗？(y/n): ", b->patient_id);
        if (getConfirm()) {
            ListNode* patient_node = FindNode(patient_list, b->patient_id);
            if (patient_node) {
                Patient* p = (Patient*)patient_node->data;
                p->is_inpatient = PATIENT_OUT;
                HIS_STRNCPY(p->bed_id, "-1", MAX_ID_LEN);
            }
            b->status = BED_FREE;
            HIS_STRNCPY(b->patient_id, "-1", MAX_ID_LEN);
            b->admit_time[0] = '\0';
            printf("\n[成功] 出院办理成功，床位 %s 已释放\n", bed_id);
            saveBedData();
            savePatientData();
        }
        else {
            printf("\n[取消] 已取消出院操作。\n");
        }
    }
}

static void deleteBed() {
    char id[MAX_ID_LEN];
    printf("\n请输入需要删除的床位ID (输入0取消): ");
    readString(id, sizeof(id));
    if (strlen(id) == 0 || strcmp(id, "0") == 0) {
        printf("\n[取消] 已取消删除操作。\n");
        return;
    }
    ListNode* bed_node = FindNode(bed_list, id);
    if (!bed_node) { printf("\n[错误] 未找到该床位！\n"); return; }
    Bed* b = (Bed*)bed_node->data;
    if (b->status == BED_OCCUPIED) { printf("\n[错误] 该床位正在使用中，无法删除！\n"); return; }
    printf("\n[确认] 确定要删除床位 %s 吗？(y/n): ", b->id);
    if (getConfirm()) {
        if (DeleteNode(bed_list, id) == 0) { printf("\n[成功] 床位删除成功！\n"); saveBedData(); }
    }
    else { printf("\n[取消] 已取消删除操作。\n"); }
}

static void queryBed() {
    printf("\n--- 床位查询 ---\n");
    printf("1. 按ID精确查询\n");
    printf("2. 按科室查询\n");
    printf("3. 按状态查询 (空闲/占用)\n");
    printf("4. 列出所有床位\n");
    printf("请选择: ");
    int choice = getValidChoice(1, 4);
    switch (choice) {
    case 1: {
        char id[MAX_ID_LEN];
        printf("\n请输入床位ID：");
        readString(id, sizeof(id));
        ListNode* bed_node = FindNode(bed_list, id);
        if (bed_node) { Bed* b = (Bed*)bed_node->data; printf("\n--- 查询结果 ---\n"); printBedInfo(b); }
        else { printf("\n[错误] 未找到该床位！\n"); }
        break;
    }
    case 2: {
        char dept_id[MAX_ID_LEN];
        printf("\n请输入科室ID：");
        readString(dept_id, sizeof(dept_id));
        ListNode* dept_node = FindNode(dept_list, dept_id);
        if (dept_node) {
            Department* dept = (Department*)dept_node->data;
            printf("\n--- 科室 %s 的床位列表 ---\n", dept->name);
            int found = 0;
            ListNode* p = bed_list->head;
            while (p) {
                Bed* b = (Bed*)p->data;
                if (strcmp(b->dept_id, dept_id) == 0) { printBedInfo(b); found = 1; }
                p = p->next;
            }
            if (!found) printf("暂无该科室的床位。\n");
        }
        else { printf("\n[错误] 该科室不存在！\n"); }
        break;
    }
    case 3: {
        int status;
        printf("\n请选择床位状态：0-空闲，1-占用：");
        status = getValidChoice(0, 1);
        printf("\n--- %s 床位列表 ---\n", status == 0 ? "空闲" : "占用");
        int found = 0;
        ListNode* p = bed_list->head;
        while (p) {
            Bed* b = (Bed*)p->data;
            if (b->status == status) { printBedInfo(b); found = 1; }
            p = p->next;
        }
        if (!found) printf("没有%s床位。\n", status == 0 ? "空闲" : "占用");
        break;
    }
    case 4:
        printf("\n--- 所有床位列表 ---\n");
        if (!bed_list->head) printf("暂无床位记录。\n");
        else { ListNode* p = bed_list->head; while (p) { printBedInfo((Bed*)p->data); p = p->next; } }
        break;
    default:
        printf("\n[错误] 无效选项！\n");
    }
}

// ==================== 3. 数据持久化相关函数 ====================
void saveDeptData() { SaveDataToFile(dept_list, FILE_DEPT, formatDeptLine); }
void loadDeptData(void) {
    LoadDataFromFile(dept_list, FILE_DEPT, parseDeptLine);
}

void saveBedData() { SaveDataToFile(bed_list, FILE_BED, formatBedLine); }
void loadBedData(void) {
    LoadDataFromFile(bed_list, FILE_BED, parseBedLine);
}

// ==================== 4. 辅助函数实现 ====================
static Department* findDeptByID(const char* id) {
    ListNode* node = FindNode(dept_list, id);
    return node ? (Department*)node->data : NULL;
}

static void inputDeptInfo(Department* d) {
    char* nl;
    while (1) {
        printf("请输入科室名称: ");
        if (!fgets(d->name, MAX_NAME_LEN, stdin)) { ClearInputBuffer(); continue; }
        nl = strchr(d->name, '\n');
        if (nl) *nl = '\0';
        else {
            ClearInputBuffer();
            size_t _len = strlen(d->name);
            if (_len > 0 && (unsigned char)d->name[_len - 1] >= 0x81)
                d->name[_len - 1] = '\0';
        }
        if (!ValidateNoPipe(d->name)) { printf("[错误] 名称不能包含分隔符'|'。\n"); continue; }
        if (strlen(d->name) == 0) { printf("[错误] 名称不能为空！\n"); continue; }
        break;
    }
}

void printDeptInfo(void* data) {
    if (!data) { printf("[错误] 显示数据为空！\n"); return; }
    Department* dept = (Department*)data;
    printf("=====================================\n");
    printf("科室ID：%s\n", dept->id);
    printf("科室名称：%s\n", dept->name);
    printf("科室医生数：%d\n", dept->doctor_count);
    printf("=====================================\n");
}

static void formatDeptLine(void* data, char* line) {
    Department* d = (Department*)data;
    snprintf(line, MAX_LINE_LEN, "%s|%s|%d", d->id, d->name, d->doctor_count);
}

static void parseDeptLine(char* line, void* data) {
    Department* d = (Department*)data;
    sscanf(line, "%19[^|]|%49[^|]|%d", d->id, d->name, &d->doctor_count);
}

static void inputBedInfo(Bed* b) {
    printf("\n===== 输入床位信息 =====\n");
    printf(" 1. 普通病房\n");
    printf(" 2. 双人病房\n");
    printf(" 3. VIP 病房\n");
    printf("请选择病房类型：");
    int type_choice = getValidChoice(1, 3);
    b->room_type = (RoomType)type_choice;
    switch (b->room_type) {
    case 1: printf("[提示] 普通病房：3人间，标配医疗设备\n"); break;
    case 2: printf("[提示] 双人病房：2人间，环境舒适\n"); break;
    case 3: printf("[提示] VIP 病房：1人间，独立卫浴及高级护理\n"); break;
    }
}

static void formatBedLine(void* data, char* line) {
    Bed* b = (Bed*)data;
    snprintf(line, MAX_LINE_LEN, "%s|%d|%s|%d|%s|%s",
        b->id, b->room_type, b->dept_id, b->status, b->patient_id, b->admit_time);
}

static void parseBedLine(char* line, void* data) {
    Bed* b = (Bed*)data;
    memset(b, 0, sizeof(Bed));
    int room_type, status;
    sscanf(line, "%[^|]|%d|%[^|]|%d|%[^|]|%[^\n]",
        b->id, &room_type, b->dept_id, &status, b->patient_id, b->admit_time);
    b->room_type = (RoomType)room_type;
    b->status = (BedStatus)status;
}

// ==================== 5. 统计与辅助函数实现 ====================
static void calculateBedStats(const char* dept_id, int* total, int* occupied) {
    *total = 0;
    *occupied = 0;
    ListNode* p = bed_list->head;
    while (p) {
        Bed* b = (Bed*)p->data;
        if (dept_id == NULL || strcmp(b->dept_id, dept_id) == 0) {
            (*total)++;
            if (b->status == BED_OCCUPIED) (*occupied)++;
        }
        p = p->next;
    }
}

static void printBedStats(const char* title, int total, int occupied) {
    int free_beds = total - occupied;
    double occupancy_rate = (total > 0) ? ((double)occupied / total) * 100 : 0;
    printf("\n==================== %s ====================\n", title);
    printf("  总床位: %d\n", total);
    printf("  已占用: %d\n", occupied);
    printf("  空闲:   %d\n", free_beds);
    printf("  占用率: %.1f%%\n", occupancy_rate);
    printf("==================================================\n");
}

void printBedInfo(void* data) {
    if (!data) { printf("\n[错误] 床位数据为空！\n"); return; }
    Bed* b = (Bed*)data;
    Department* dept = findDeptByID(b->dept_id);
    const char* type_str[] = { "未知", "普通", "双人", "VIP" };
    int room_idx = (b->room_type >= 0 && b->room_type <= 3) ? b->room_type : 0;
    const char* status_str = (b->status == BED_FREE) ? "空闲" : "占用";
    printf("\n[床位ID: %s]\n", b->id);
    printf("  所属科室: %-15s  病房类型: %s\n", dept ? dept->name : "未知", type_str[room_idx]);
    printf("  状态: %-6s", status_str);
    if (b->status == BED_OCCUPIED) printf("  住院患者: %s  入住时间: %s", b->patient_id, b->admit_time);
    printf("\n");
}

static void statsSubMenu() {
    printf("\n==================== 统计查询 ====================\n");
    printf(" 1. 按科室统计床位使用情况\n");
    printf(" 2. 全院床位统计\n");
    printf(" 0. 返回上一级菜单\n");
    printf("==================================================\n");
    printf("请选择: ");
    int choice = getValidChoice(0, 2);
    int total, occupied;
    switch (choice) {
    case 1: {
        char dept_id[MAX_ID_LEN];
        printf("\n请输入科室ID: ");
        readString(dept_id, sizeof(dept_id));
        ListNode* dept_node = FindNode(dept_list, dept_id);
        if (dept_node) {
            Department* dept = (Department*)dept_node->data;
            calculateBedStats(dept_id, &total, &occupied);
            char title[64];
            sprintf(title, "科室 %s 床位统计", dept->name);
            printBedStats(title, total, occupied);
        }
        else { printf("\n[错误] 该科室不存在！\n"); }
        break;
    }
    case 2:
        calculateBedStats(NULL, &total, &occupied);
        printBedStats("全院床位统计", total, occupied);
        break;
    case 0: printf("\n[提示] 返回上一级菜单\n"); break;
    default: printf("\n[错误] 无效选项！\n");
    }
}

// ==================== 6. 子菜单 ====================
static void deptSubMenu() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("           科室信息管理 [子菜单]\n");
        PrintSeparator();
        printf("  1. 添加科室\n");
        printf("  2. 修改科室信息\n");
        printf("  3. 删除科室\n");
        printf("  4. 查询科室\n");
        printf("  0. 返回上级\n");
        PrintSeparator();
        printf("请输入选项: ");
        choice = getValidChoice(0, 4);
        switch (choice) {
        case 1: addDept(); break;
        case 2: modifyDept(); break;
        case 3: deleteDept(); break;
        case 4: queryDept(); break;
        case 0: return;
        default: printf("\n[错误] 无效选项\n");
        }
    }
}

static void bedSubMenu() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("           床位管理 [子菜单]\n");
        PrintSeparator();
        printf("  1. 添加床位\n");
        printf("  2. 办理住院/出院 (修改状态)\n");
        printf("  3. 删除床位\n");
        printf("  4. 查询床位\n");
        printf("  0. 返回上级\n");
        PrintSeparator();
        printf("请输入选项: ");
        choice = getValidChoice(0, 4);
        switch (choice) {
        case 1: addBed(); break;
        case 2: modifyBedStatus(); break;
        case 3: deleteBed(); break;
        case 4: queryBed(); break;
        case 0: return;
        default: printf("\n[错误] 无效选项\n");
        }
    }
}

// ==================== 7. 模块对外接口 ====================
void dept_bedModule() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("        科室/医生/床位管理模块\n");
        PrintSeparator();
        printf("  1. 科室信息管理\n");
        printf("  2. 医生信息管理\n");
        printf("  3. 床位管理\n");
        printf("  4. 床位统计查询\n");
        printf("  5. 排班管理\n");
        printf("  0. 返回主菜单\n");
        PrintSeparator();
        printf("请输入选项: ");
        choice = getValidChoice(0, 5);
        switch (choice) {
        case 1: deptSubMenu(); break;
        case 2: doctorSubMenu(); break;
        case 3: bedSubMenu(); break;
        case 4: statsSubMenu(); break;
        case 5: scheduleSubMenu(); break;
        case 0: return;
        default: printf("\n[错误] 无效选项\n");
        }
    }
}
