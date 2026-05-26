#include "his.h"

/*
 * 医生管理模块
 *   addDoctor / modifyDoctor / deleteDoctor / queryDoctor
 *   printDoctorInfo      — 打印医生信息（跨模块调用）
 *   save/loadDoctorData  — 文件持久化
 *   doctorSubMenu()      — 子菜单入口
 */
static void inputDoctorInfo(Doctor* d);
static void formatDoctorLine(void* data, char* line);
static void parseDoctorLine(char* line, void* data);

// --- 非静态函数前向声明（供 doctorSubMenu 调用） ---
void addDoctor();
void modifyDoctor();
void deleteDoctor();
void queryDoctor();

// ==================== 医生信息管理相关函数 ====================
void doctorSubMenu() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("           医生信息管理 [子菜单]\n");
        PrintSeparator();
        printf("  1. 添加医生\n");
        printf("  2. 修改医生信息\n");
        printf("  3. 删除医生\n");
        printf("  4. 查询医生\n");
        printf("  0. 返回上级\n");
        PrintSeparator();
        printf("请输入选项: ");
        choice = getValidChoice(0, 4);
        switch (choice) {
        case 1: addDoctor(); break;
        case 2: modifyDoctor(); break;
        case 3: deleteDoctor(); break;
        case 4: queryDoctor(); break;
        case 0: return;
        default: printf("\n[错误] 无效选项\n");
        }
    }
}

void addDoctor() {
    Doctor d;
    memset(&d, 0, sizeof(Doctor));
    printf("\n--- 请选择所属科室 ---\n");
    if (dept_list->length == 0) { printf("[错误] 暂无科室，请先添加科室！\n"); return; }
    TraverseList(dept_list, printDeptInfo);
    char dept_id[MAX_ID_LEN];
    printf("\n请输入所属科室ID: ");
    readString(dept_id, sizeof(dept_id));
    ListNode* dept_node = FindNode(dept_list, dept_id);
    if (!dept_node) { printf("\n[错误] 科室ID不存在！\n"); return; }
    Department* dept = (Department*)dept_node->data;
    inputDoctorInfo(&d);

    // 密码输入（inputDoctorInfo 不再处理密码）
    char* nl;
    while (1) {
        printf("请输入登录密码: ");
        if (!fgets(d.password, MAX_PWD_LEN, stdin)) { ClearInputBuffer(); continue; }
        nl = strchr(d.password, '\n');
        if (nl) *nl = '\0';
        else ClearInputBuffer();
        if (strlen(d.password) == 0) { printf("[错误] 密码不能为空！\n"); continue; }
        break;
    }
    passwordObfuscate(d.password);

    // 设置每日最大挂号量
    char buf[MAX_LINE_LEN];
    while (1) {
        printf("请输入每日最大挂号量 (默认30): ");
        inputLine(buf, sizeof(buf));
        int max_reg = atoi(buf);
        if (strlen(buf) == 0) { d.max_register = 30; break; }
        if (max_reg > 0) { d.max_register = max_reg; break; }
        printf("[错误] 挂号量必须大于0！\n");
    }

    // 检查账号唯一性
    {
        ListNode* p = doctor_list->head;
        while (p) {
            Doctor* existing = (Doctor*)p->data;
            if (strcmp(existing->account, d.account) == 0) {
                printf("\n[错误] 登录账号 '%s' 已被使用！\n", d.account);
                return;
            }
            p = p->next;
        }
    }

    HIS_STRNCPY(d.dept_id, dept_id, MAX_ID_LEN);
    int retry = 0;
    do {
        GenerateID(d.id, ID_PREFIX_DOCTOR);
        if (++retry > MAX_ID_RETRY) {
            printf("[错误] 无法生成唯一医生ID！\n");
            return;
        }
    } while (FindNode(doctor_list, d.id) != NULL);
    if (InsertNode(doctor_list, -1, &d, sizeof(Doctor), d.id) == 0) {
        dept->doctor_count++;
        printf("\n[成功] 医生添加成功，医生ID: %s\n", d.id);
        saveDoctorData();
        saveDeptData();
    }
    else { printf("\n[失败] 医生添加失败！\n"); }
}

void modifyDoctor() {
    char id[MAX_ID_LEN];
    printf("\n请输入需要修改的医生ID: ");
    readString(id, sizeof(id));
    ListNode* node = FindNode(doctor_list, id);
    if (!node) { printf("\n[错误] 未找到该医生！\n"); return; }
    Doctor* d = (Doctor*)node->data;

    int choice;
    while (1) {
        printf("\n====== 修改医生信息 ======\n");
        printDoctorInfo(d);
        PrintSeparator();
        printf("  1. 修改姓名 (当前: %s)\n", d->name);
        printf("  2. 修改所属科室 (当前: %s)\n", d->dept_id);
        printf("  3. 修改擅长领域 (当前: %s)\n", d->specialty);
        printf("  4. 修改登录账号 (当前: %s)\n", d->account);
        printf("  5. 修改登录密码\n");
        printf("  6. 修改每日最大挂号量 (当前: %d)\n", d->max_register);
        printf("  0. 保存并返回\n");
        PrintSeparator();
        printf("请选择要修改的字段: ");
        choice = getValidChoice(0, 6);

        if (choice == 0) break;

        char buf[MAX_LINE_LEN];
        switch (choice) {
        case 1:
            printf("请输入新姓名: ");
            inputLine(buf, sizeof(buf));
            if (!ValidateNoPipe(buf)) { printf("[错误] 姓名不能包含分隔符'|'！\n"); break; }
            if (strlen(buf) == 0) { printf("[错误] 姓名不能为空！\n"); break; }
            HIS_STRNCPY(d->name, buf, sizeof(d->name));
            printf("[成功] 姓名已更新。\n");
            break;

        case 2: {
            printf("请输入新科室ID: ");
            inputLine(buf, sizeof(buf));
            if (strlen(buf) == 0) { printf("[错误] 科室ID不能为空！\n"); break; }
            if (!FindNode(dept_list, buf)) { printf("[错误] 科室ID不存在！\n"); break; }
            HIS_STRNCPY(d->dept_id, buf, sizeof(d->dept_id));
            printf("[成功] 所属科室已更新。\n");
            break;
        }

        case 3:
            printf("请输入新擅长领域: ");
            inputLine(buf, sizeof(buf));
            if (!ValidateNoPipe(buf)) { printf("[错误] 特长不能包含分隔符'|'！\n"); break; }
            HIS_STRNCPY(d->specialty, buf, sizeof(d->specialty));
            printf("[成功] 擅长领域已更新。\n");
            break;

        case 4:
            printf("请输入新登录账号: ");
            inputLine(buf, sizeof(buf));
            if (!ValidateNoPipe(buf)) { printf("[错误] 账号不能包含分隔符'|'！\n"); break; }
            if (strlen(buf) == 0) { printf("[错误] 账号不能为空！\n"); break; }
            // 检查账号唯一性（排除自身）
            {
                ListNode* p = doctor_list->head;
                while (p) {
                    Doctor* existing = (Doctor*)p->data;
                    if (existing != d && strcmp(existing->account, buf) == 0) {
                        printf("[错误] 账号 '%s' 已被其他医生使用！\n", buf);
                        break;
                    }
                    p = p->next;
                }
                if (p) break; // 账号冲突
            }
            HIS_STRNCPY(d->account, buf, sizeof(d->account));
            printf("[成功] 登录账号已更新。\n");
            break;

        case 5:
            printf("请输入新密码: ");
            inputLine(buf, sizeof(buf));
            if (strlen(buf) == 0) { printf("[错误] 密码不能为空！\n"); break; }
            HIS_STRNCPY(d->password, buf, MAX_PWD_LEN);
            passwordObfuscate(d->password);
            printf("[成功] 密码已更新。\n");
            break;

        case 6:
            printf("请输入每日最大挂号量: ");
            inputLine(buf, sizeof(buf));
            if (strlen(buf) == 0) { printf("[错误] 输入不能为空！\n"); break; }
            {
                int max_reg = atoi(buf);
                if (max_reg <= 0) { printf("[错误] 挂号量必须为正整数！\n"); break; }
                d->max_register = max_reg;
                printf("[成功] 每日最大挂号量已更新。\n");
            }
            break;

        default:
            printf("[错误] 无效选择！\n");
        }
    }

    printf("\n[成功] 医生信息修改成功！\n");
    saveDoctorData();
}

void deleteDoctor() {
    char id[MAX_ID_LEN];
    printf("\n请输入需要删除的医生ID (输入0取消): ");
    readString(id, sizeof(id));
    if (strlen(id) == 0 || strcmp(id, "0") == 0) {
        printf("\n[取消] 已取消删除操作。\n");
        return;
    }
    ListNode* node = FindNode(doctor_list, id);
    if (!node) { printf("\n[错误] 未找到该医生！\n"); return; }
    Doctor* d = (Doctor*)node->data;

    // 检查关联数据
    {
        // 检查是否有排班
        ListNode* sn = schedule_list->head;
        while (sn) {
            DoctorSchedule* s = (DoctorSchedule*)sn->data;
            if (strcmp(s->doctor_id, id) == 0) {
                printf("\n[错误] 该医生有排班记录，请先删除排班！\n");
                return;
            }
            sn = sn->next;
        }
        // 检查是否有患者正在挂号
        ListNode* pn = patient_list->head;
        while (pn) {
            Patient* p = (Patient*)pn->data;
            if (strcmp(p->doctor_id, id) == 0 && p->register_status != REG_STATUS_NONE) {
                printf("\n[错误] 有患者正在挂该医生的号，无法删除！\n");
                return;
            }
            pn = pn->next;
        }
    }

    printf("\n[确认] 确定要删除医生 %s (%s) 吗？(y/n): ", d->name, d->id);
    if (getConfirm()) {
        ListNode* dept_node = FindNode(dept_list, d->dept_id);
        if (dept_node) { Department* dept = (Department*)dept_node->data; dept->doctor_count--; }
        if (DeleteNode(doctor_list, id) == 0) { printf("\n[成功] 医生删除成功！\n"); saveDoctorData(); saveDeptData(); }
    }
    else { printf("\n[取消] 已取消删除操作。\n"); }
}

void queryDoctor() {
    printf("\n--- 医生查询 ---\n");
    printf("1. 按ID精确查询\n");
    printf("2. 按科室查询\n");
    printf("3. 列出所有医生\n");
    printf("请选择: ");
    int choice = getValidChoice(1, 3);
    switch (choice) {
    case 1: {
        char id[MAX_ID_LEN];
        printf("\n请输入医生ID：");
        readString(id, sizeof(id));
        ListNode* node = FindNode(doctor_list, id);
        if (node) { Doctor* d = (Doctor*)node->data; printf("\n--- 查询结果 ---\n"); printDoctorInfo(d); }
        else { printf("\n[错误] 未找到该医生！\n"); }
        break;
    }
    case 2: {
        char dept_id[MAX_ID_LEN];
        printf("\n请输入科室ID：");
        readString(dept_id, sizeof(dept_id));
        printf("\n--- 科室 %s 的医生列表 ---\n", dept_id);
        int found = 0;
        ListNode* p = doctor_list->head;
        while (p) {
            Doctor* d = (Doctor*)p->data;
            if (strcmp(d->dept_id, dept_id) == 0) { printDoctorInfo(d); found = 1; }
            p = p->next;
        }
        if (!found) printf("暂无该科室的医生。\n");
        break;
    }
    case 3:
        printf("\n--- 所有医生列表 ---\n");
        if (doctor_list->length == 0) printf("暂无医生数据。\n");
        else TraverseList(doctor_list, printDoctorInfo);
        break;
    default:
        printf("\n[错误] 无效选项！\n");
    }
}

void printDoctorInfo(void* data) {
    if (!data) { printf("\n[错误] 医生数据为空！\n"); return; }
    Doctor* d = (Doctor*)data;
    ListNode* dn = FindNode(dept_list, d->dept_id);
    Department* dept = dn ? (Department*)dn->data : NULL;
    printf("\n[医生ID: %s]\n", d->id);
    printf("  姓名: %-10s  所属科室: %s\n", d->name, dept ? dept->name : "未知");
    printf("  特长: %s\n", d->specialty);
}

void saveDoctorData() { SaveDataToFile(doctor_list, FILE_DOCTOR, formatDoctorLine); }
void loadDoctorData(void) {
    LoadDataFromFile(doctor_list, FILE_DOCTOR, parseDoctorLine);
}

// ==================== 静态辅助函数 ====================
static void inputDoctorInfo(Doctor* d) {
    char* nl;
    while (1) {
        printf("请输入医生姓名: ");
        if (!fgets(d->name, MAX_NAME_LEN, stdin)) { ClearInputBuffer(); continue; }
        nl = strchr(d->name, '\n');
        if (nl) *nl = '\0';
        else {
            ClearInputBuffer();
            size_t _len = strlen(d->name);
            if (_len > 0 && (unsigned char)d->name[_len - 1] >= 0x81)
                d->name[_len - 1] = '\0';
        }
        if (!ValidateNoPipe(d->name)) { printf("[错误] 姓名不能包含分隔符'|'。\n"); continue; }
        if (strlen(d->name) == 0) { printf("[错误] 姓名不能为空！\n"); continue; }
        break;
    }
    while (1) {
        printf("请输入医生特长: ");
        if (!fgets(d->specialty, MAX_SPECIALTY_LEN, stdin)) { ClearInputBuffer(); continue; }
        nl = strchr(d->specialty, '\n');
        if (nl) *nl = '\0';
        else {
            ClearInputBuffer();
            size_t _len = strlen(d->specialty);
            if (_len > 0 && (unsigned char)d->specialty[_len - 1] >= 0x81)
                d->specialty[_len - 1] = '\0';
        }
        if (!ValidateNoPipe(d->specialty)) { printf("[错误] 特长不能包含分隔符'|'。\n"); continue; }
        break;
    }
    while (1) {
        printf("请输入登录账号: ");
        if (!fgets(d->account, MAX_NAME_LEN, stdin)) { ClearInputBuffer(); continue; }
        nl = strchr(d->account, '\n');
        if (nl) *nl = '\0';
        else {
            ClearInputBuffer();
            size_t _len = strlen(d->account);
            if (_len > 0 && (unsigned char)d->account[_len - 1] >= 0x81)
                d->account[_len - 1] = '\0';
        }
        if (!ValidateNoPipe(d->account)) { printf("[错误] 账号不能包含分隔符'|'。\n"); continue; }
        if (strlen(d->account) == 0) { printf("[错误] 账号不能为空！\n"); continue; }
        break;
    }
}

static void formatDoctorLine(void* data, char* line) {
    Doctor* d = (Doctor*)data;
    snprintf(line, MAX_LINE_LEN, "%s|%s|%s|%s|%s|%s|%d|%d|%s",
        d->id, d->name, d->dept_id, d->specialty, d->account, d->password,
        d->max_register, d->current_register, d->register_date);
}

static void parseDoctorLine(char* line, void* data) {
    Doctor* d = (Doctor*)data;
    memset(d, 0, sizeof(Doctor));
    char date_buf[11] = "";
    int n = sscanf(line, "%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%d|%d|%10s",
        d->id, d->name, d->dept_id, d->specialty, d->account, d->password,
        &d->max_register, &d->current_register, date_buf);
    if (n < 8) { d->max_register = 0; d->current_register = 0; }
    if (n >= 9) {
        HIS_STRNCPY(d->register_date, date_buf, sizeof(d->register_date));
    }
}
