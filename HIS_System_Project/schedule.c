#include "his.h"

/*
 * 排班管理模块
 *   addSchedule / viewSchedule / deleteSchedule / modifySchedule
 *   selectDoctorByDept  — 按科室列出医生供选择（addSchedule 内部调用）
 *   save/loadScheduleData — 文件持久化
 *   scheduleSubMenu() — 子菜单入口
 */
static void addSchedule();
static void viewSchedule();
static void deleteSchedule();
static void modifySchedule();
static void printScheduleInfo(void* data);
static void formatSchedule(void* data, char* line);
static void parseSchedule(char* line, void* data);

// ==================== 文件 I/O ====================
void saveScheduleData(void) {
    SaveDataToFile(schedule_list, FILE_SCHEDULE, formatSchedule);
}

void loadScheduleData(void) {
    LoadDataFromFile(schedule_list, FILE_SCHEDULE, parseSchedule);
}

static void formatSchedule(void* data, char* line) {
    DoctorSchedule* s = (DoctorSchedule*)data;
    snprintf(line, MAX_LINE_LEN, "%s|%s|%s|%s|%s|%d|%d|%d",
        s->id, s->doctor_id, s->dept_id,
        s->date, s->time_slot,
        s->max_patients, s->current_patients,
        s->is_available);
}

static void parseSchedule(char* line, void* data) {
    DoctorSchedule* s = (DoctorSchedule*)data;
    memset(s, 0, sizeof(DoctorSchedule));
    char* rest = line;
    char* token;

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(s->id, token, sizeof(s->id));

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(s->doctor_id, token, sizeof(s->doctor_id));

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(s->dept_id, token, sizeof(s->dept_id));

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(s->date, token, sizeof(s->date));

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(s->time_slot, token, sizeof(s->time_slot));

    token = next_token(&rest); if (!token) return;
    s->max_patients = atoi(token);

    token = next_token(&rest); if (!token) return;
    s->current_patients = atoi(token);

    token = next_token(&rest); if (!token) return;
    s->is_available = atoi(token);
}

// ==================== 排班子菜单 ====================
void scheduleSubMenu() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("              排班管理\n");
        PrintSeparator();
        printf("  1. 新增排班\n");
        printf("  2. 查看排班\n");
        printf("  3. 删除排班\n");
        printf("  4. 修改排班\n");
        printf("  0. 返回上级\n");
        PrintSeparator();
        printf("请输入选项: ");
        choice = getValidChoice(0, 4);
        switch (choice) {
        case 1: addSchedule(); break;
        case 2: viewSchedule(); break;
        case 3: deleteSchedule(); break;
        case 4: modifySchedule(); break;
        case 0: return;
        default: printf("\n[错误] 无效选项\n");
        }
    }
}

// 按科室选取医生，返回 0 成功 / -1 失败
static int selectDoctorByDept(const char* dept_id, char* out_doctor_id) {
    printf("\n--- %s 的医生列表 ---\n", dept_id);
    ListNode* docn = doctor_list->head;
    int doc_count = 0;
    while (docn) {
        Doctor* d = (Doctor*)docn->data;
        if (strcmp(d->dept_id, dept_id) == 0) {
            doc_count++;
            printf("  %s | %s\n", d->id, d->name);
        }
        docn = docn->next;
    }
    if (doc_count == 0) {
        printf("  该科室暂无医生。\n");
        return -1;
    }
    printf("\n请输入医生ID: ");
    readString(out_doctor_id, MAX_ID_LEN);
    if (!FindNode(doctor_list, out_doctor_id)) {
        printf("\n[错误] 医生ID不存在！\n");
        return -1;
    }
    return 0;
}

static void addSchedule() {
    if (doctor_list->length == 0) {
        printf("\n[错误] 暂无医生，请先添加医生。\n");
        return;
    }

    // 选择科室
    printf("\n--- 选择科室 ---\n");
    TraverseList(dept_list, printDeptInfo);
    char dept_id[MAX_ID_LEN];
    printf("\n请输入科室ID: ");
    readString(dept_id, sizeof(dept_id));

    if (!FindNode(dept_list, dept_id)) {
        printf("\n[错误] 科室ID不存在！\n");
        return;
    }

    char doctor_id[MAX_ID_LEN];
    if (selectDoctorByDept(dept_id, doctor_id) != 0)
        return;

    // 输入排班信息
    DoctorSchedule s;
    memset(&s, 0, sizeof(DoctorSchedule));

    char buf[MAX_LINE_LEN];
    while (1) {
        printf("请输入排班日期 (YYYY-MM-DD): ");
        inputLine(buf, sizeof(buf));
        if (strlen(buf) == 10 && buf[4] == '-' && buf[7] == '-') {
            HIS_STRNCPY(s.date, buf, sizeof(s.date));
            break;
        }
        printf("[错误] 日期格式无效，请使用 YYYY-MM-DD 格式！\n");
    }

    while (1) {
        printf("请输入时间段 (如 08:00-10:00): ");
        inputLine(buf, sizeof(buf));
        if (strlen(buf) > 0) {
            if (!ValidateNoPipe(buf)) { printf("[错误] 时间段不能包含分隔符'|'！\n"); continue; }
            HIS_STRNCPY(s.time_slot, buf, sizeof(s.time_slot));
            break;
        }
        printf("[错误] 时间段不能为空！\n");
    }

    // 获取医生每日最大挂号量
    int doctor_max = 0;
    {
        ListNode* dn = FindNode(doctor_list, doctor_id);
        if (dn) doctor_max = ((Doctor*)dn->data)->max_register;
    }
    while (1) {
        printf("请输入最大号源数 (该医生每日上限 %d): ", doctor_max > 0 ? doctor_max : 999);
        inputLine(buf, sizeof(buf));
        int max_p = atoi(buf);
        if (max_p > 0) {
            if (doctor_max > 0 && max_p > doctor_max) {
                printf("[错误] 号源数不能超过医生每日最大挂号量 (%d)！\n", doctor_max);
                continue;
            }
            s.max_patients = max_p;
            break;
        }
        printf("[错误] 号源数必须大于0！\n");
    }

    HIS_STRNCPY(s.doctor_id, doctor_id, sizeof(s.doctor_id));
    HIS_STRNCPY(s.dept_id, dept_id, sizeof(s.dept_id));
    s.is_available = 1;
    s.current_patients = 0;

    int retry = 0;
    do {
        GenerateID(s.id, ID_PREFIX_SCHEDULE);
        if (++retry > 10) {
            printf("[错误] 无法生成唯一排班ID！\n");
            return;
        }
    } while (FindNode(schedule_list, s.id) != NULL);

    if (InsertNode(schedule_list, -1, &s, sizeof(DoctorSchedule), s.id) == 0) {
        saveScheduleData();
        printf("\n[成功] 排班添加成功！\n");
        printf("  排班ID: %s\n", s.id);
        printf("  医生ID: %s | 日期: %s | 时段: %s | 号源: %d\n",
            s.doctor_id, s.date, s.time_slot, s.max_patients);
    }
    else {
        printf("\n[失败] 排班添加失败！\n");
    }
}

static void viewSchedule() {
    printf("\n--- 查看排班 ---\n");
    printf("  1. 查看所有排班\n");
    printf("  2. 按医生查看\n");
    printf("  3. 按科室查看\n");
    printf("  4. 按日期查看\n");
    printf("  0. 取消\n");
    printf("请选择: ");
    int choice = getValidChoice(0, 4);
    if (choice == 0) return;

    char filter_id[MAX_ID_LEN] = "";
    char filter_date[11] = "";

    if (choice == 2) {
        printf("请输入医生ID: ");
        readString(filter_id, sizeof(filter_id));
        if (!FindNode(doctor_list, filter_id)) {
            printf("\n[错误] 医生ID不存在！\n");
            return;
        }
    }
    else if (choice == 3) {
        printf("\n--- 科室列表 ---\n");
        TraverseList(dept_list, printDeptInfo);
        printf("\n请输入科室ID: ");
        readString(filter_id, sizeof(filter_id));
        if (!FindNode(dept_list, filter_id)) {
            printf("\n[错误] 科室ID不存在！\n");
            return;
        }
    }
    else if (choice == 4) {
        char buf[MAX_LINE_LEN];
        while (1) {
            printf("请输入日期 (YYYY-MM-DD): ");
            inputLine(buf, sizeof(buf));
            if (strlen(buf) == 10 && buf[4] == '-' && buf[7] == '-') {
                HIS_STRNCPY(filter_date, buf, sizeof(filter_date));
                break;
            }
            printf("[错误] 日期格式无效！\n");
        }
    }

    int count = 0;
    ListNode* sn = schedule_list->head;
    while (sn) {
        DoctorSchedule* s = (DoctorSchedule*)sn->data;
        int match = 0;
        if (choice == 1) match = 1;
        else if (choice == 2) match = (strcmp(s->doctor_id, filter_id) == 0);
        else if (choice == 3) match = (strcmp(s->dept_id, filter_id) == 0);
        else if (choice == 4) match = (strcmp(s->date, filter_date) == 0);

        if (match) {
            if (count == 0) printf("\n");
            count++;
            // 查找医生姓名
            const char* doc_name = "未知";
            ListNode* dn = FindNode(doctor_list, s->doctor_id);
            if (dn) doc_name = ((Doctor*)dn->data)->name;
            // 查找科室名称
            const char* dept_name = "未知";
            ListNode* ddn = FindNode(dept_list, s->dept_id);
            if (ddn) dept_name = ((Department*)ddn->data)->name;

            printf("[%d] 排班ID: %s\n", count, s->id);
            printf("    医生: %s (%s) | 科室: %s\n", doc_name, s->doctor_id, dept_name);
            printf("    日期: %s | 时段: %s\n", s->date, s->time_slot);
            printf("    号源: %d/%d | 状态: %s\n",
                s->current_patients, s->max_patients,
                s->is_available ? "可预约" : "不可预约");
        }
        sn = sn->next;
    }
    if (count == 0) {
        printf("\n  [提示] 暂无排班记录。\n");
    }
    waitForEnter();
}

static void deleteSchedule() {
    // 先显示所有排班
    if (schedule_list->length == 0) {
        printf("\n[提示] 暂无排班记录。\n");
        return;
    }
    viewSchedule();

    char id[MAX_ID_LEN];
    printf("请输入要删除的排班ID (输入0取消): ");
    readString(id, sizeof(id));
    if (strlen(id) == 0 || strcmp(id, "0") == 0) {
        printf("\n[取消] 已取消删除操作。\n");
        return;
    }

    ListNode* sn = FindNode(schedule_list, id);
    if (!sn) {
        printf("\n[错误] 未找到该排班！\n");
        return;
    }

    // 检查是否有预约关联
    {
        ListNode* ap = appointment_list->head;
        while (ap) {
            Appointment* a = (Appointment*)ap->data;
            if (strcmp(a->schedule_id, id) == 0 && strcmp(a->status, "已取消") != 0) {
                printf("\n[错误] 该排班下有未取消的预约记录，请先取消预约再删除排班！\n");
                return;
            }
            ap = ap->next;
        }
    }

    DoctorSchedule* s = (DoctorSchedule*)sn->data;
    printf("\n确认删除排班 [%s] %s %s ? (y/n): ", s->id, s->date, s->time_slot);
    if (getConfirm()) {
        if (DeleteNode(schedule_list, id) == 0) {
            saveScheduleData();
            printf("\n[成功] 排班已删除。\n");
        }
        else {
            printf("\n[失败] 删除失败。\n");
        }
    }
    else {
        printf("\n已取消操作。\n");
    }
}

/* 打印单个排班信息（辅助显示） */
static void printScheduleInfo(void* data) {
    DoctorSchedule* s = (DoctorSchedule*)data;
    char doctor_name[MAX_NAME_LEN] = "未知";
    ListNode* dn = FindNode(doctor_list, s->doctor_id);
    if (dn) {
        Doctor* d = (Doctor*)dn->data;
        HIS_STRNCPY(doctor_name, d->name, sizeof(doctor_name));
    }
    char dept_name[MAX_NAME_LEN] = "未知";
    ListNode* dpn = FindNode(dept_list, s->dept_id);
    if (dpn) {
        Department* dept = (Department*)dpn->data;
        HIS_STRNCPY(dept_name, dept->name, sizeof(dept_name));
    }
    printf("  排班ID: %s | 科室: %s | 医生: %s | 日期: %s | 时段: %s | 已预约: %d/%d | 状态: %s\n",
        s->id, dept_name, doctor_name, s->date, s->time_slot,
        s->current_patients, s->max_patients,
        s->is_available ? "可用" : "不可用");
}

/* 修改排班 */
static void modifySchedule(void) {
    char id[MAX_ID_LEN];
    printf("\n请输入要修改的排班ID: ");
    readString(id, sizeof(id));

    ListNode* node = FindNode(schedule_list, id);
    if (!node) {
        printf("[错误] 未找到该排班！\n");
        return;
    }

    DoctorSchedule* s = (DoctorSchedule*)node->data;
    char buf[MAX_LINE_LEN];

    while (1) {
        PrintSeparator();
        printf("  --- 修改排班 ---\n");
        printScheduleInfo(s);
        printf("\n  1. 修改时段\n");
        printf("  2. 修改最大患者数\n");
        printf("  3. 切换可用状态\n");
        printf("  0. 保存并返回\n");
        printf("请选择: ");
        int choice = getValidChoice(0, 3);
        if (choice == 0) break;

        switch (choice) {
        case 1: {
            printf("请选择新时段:\n");
            printf("  1. 上午\n");
            printf("  2. 下午\n");
            printf("  3. 晚上\n");
            printf("请选择: ");
            int sc = getValidChoice(1, 3);
            switch (sc) {
            case 1: HIS_STRNCPY(s->time_slot, "上午", sizeof(s->time_slot)); break;
            case 2: HIS_STRNCPY(s->time_slot, "下午", sizeof(s->time_slot)); break;
            case 3: HIS_STRNCPY(s->time_slot, "晚上", sizeof(s->time_slot)); break;
            }
            printf("[成功] 时段已修改为: %s\n", s->time_slot);
            break;
        }
        case 2: {
            printf("请输入新的最大患者数 (当前: %d): ", s->max_patients);
            if (!inputLine(buf, sizeof(buf))) break;
            int mp = atoi(buf);
            if (mp <= 0) {
                printf("[错误] 最大患者数必须为正整数！\n");
            }
            else if (mp < s->current_patients) {
                printf("[错误] 新的最大患者数不能小于当前已预约数 (%d)！\n", s->current_patients);
            }
            else {
                s->max_patients = mp;
                printf("[成功] 最大患者数已修改为: %d\n", s->max_patients);
            }
            break;
        }
        case 3: {
            s->is_available = !s->is_available;
            printf("[成功] 排班状态已切换为: %s\n", s->is_available ? "可用" : "不可用");
            break;
        }
        default:
            printf("无效选择！\n");
        }
    }

    saveScheduleData();
    printf("\n[成功] 排班信息已保存。\n");
}
