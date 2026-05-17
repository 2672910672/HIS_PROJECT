#include "his.h"

/*
 * 程序入口、登录认证、主菜单控制
 *   main()           — 初始化链装数据 → 登录 → 主菜单循环
 *   admin/doctor/patient 三种角色登录及各自主菜单
 *   管理员密码持久化到 data/admin.dat，启动时覆盖默认值
 */

/* 内存中的管理员密码副本，启动时从文件加载，修改时同步更新 */
static char s_admin_password[MAX_PWD_LEN] = ADMIN_PASSWORD;

/*
 * 全局医生会话变量
 *
 * 医生登录成功后，其 ID 和姓名写入这两个变量。
 * 后续所有医生端操作（查看患者、写医疗记录、改密码）都通过
 * g_current_doctor_id 识别身份，无需反复输入医生ID。
 * 医生菜单退出时自动清空，防止会话混淆。
 */
char g_current_doctor_id[MAX_ID_LEN] = {0};
char g_current_doctor_name[MAX_NAME_LEN] = {0};

/* 管理员密码持久化文件路径 */
#define ADMIN_CFG_FILE "data/admin.dat"

/*
 * 从文件加载管理员密码
 * 文件不存在（首次运行）时静默忽略，使用默认密码。
 * 文件存在时读取第一行覆盖内存中的密码副本。
 */
static void loadAdminConfig() {
    FILE* fp = fopen(ADMIN_CFG_FILE, "r");
    if (!fp) return;
    char buf[MAX_PWD_LEN];
    if (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\n")] = '\0';
        if (strlen(buf) > 0) {
            HIS_STRNCPY(s_admin_password, buf, MAX_PWD_LEN);
        }
    }
    fclose(fp);
}

/* 将修改后的管理员密码写回文件 */
static void saveAdminConfig() {
    FILE* fp = fopen(ADMIN_CFG_FILE, "w");
    if (!fp) return;
    fprintf(fp, "%s\n", s_admin_password);
    fclose(fp);
}

/*
 * 修改管理员密码
 * 流程：验证旧密码 → 输入新密码 → 再次输入确认 → 保存
 * 安全措施：
 *   - 旧密码不匹配则直接返回
 *   - 新密码不允许为空
 *   - 两次输入必须一致
 */
static void changeAdminPassword() {
    char old[MAX_PWD_LEN], new1[MAX_PWD_LEN], new2[MAX_PWD_LEN];
    printf("\n--- 修改管理员密码 ---\n");

    printf("请输入当前密码: ");
    if (!inputLine(old, sizeof(old))) return;
    if (strcmp(old, s_admin_password) != 0) {
        printf("[错误] 当前密码错误！\n");
        return;
    }

    printf("请输入新密码: ");
    if (!inputLine(new1, sizeof(new1))) return;
    if (strlen(new1) == 0) {
        printf("[错误] 密码不能为空！\n");
        return;
    }

    printf("请再次输入新密码: ");
    if (!inputLine(new2, sizeof(new2))) return;
    if (strcmp(new1, new2) != 0) {
        printf("[错误] 两次输入的密码不一致！\n");
        return;
    }

    HIS_STRNCPY(s_admin_password, new1, MAX_PWD_LEN);
    saveAdminConfig();
    printf("[成功] 管理员密码已修改！\n");
}

/*
 * ==========================================================================
 * 全局链表变量
 *
 * 所有模块的数据在内存中以通用单向链表形式存在。
 * 使用 extern 在 his.h 中声明，各 .c 文件可以直接读写。
 *
 * 数据生命周期：
 *   main() → initGlobalLists() → loadAllHisData() → [运行期间] → saveAllHisData() → freeGlobalLists()
 *
 * 运行期间每次修改立即调用 saveXxxData() 写回文件（实时持久化），
 * 退出时再统一保存一次确保数据不丢。
 * ==========================================================================
 */
LinkList* patient_list;     // 患者链表（含挂号信息、余额、医保比例等）
LinkList* doctor_list;      // 医生链表（含账号密码、科室归属、挂号限额）
LinkList* dept_list;        // 科室链表（科室名称、医生数量）
LinkList* bed_list;         // 床位链表（病房类型、所属科室、占用状态）
LinkList* drug_list;        // 药品链表（通用名/商品名/别名、库存、预警阈值）
LinkList* record_list;      // 医疗记录链表（诊断/处方/收费记录，关联患者+医生）
LinkList* schedule_list;    // 排班链表（医生排班时间段、号源数量）
LinkList* appointment_list; // 预约链表（患者预约记录，关联排班ID）

/*
 * 一次性加载所有数据
 *
 * 按照依赖顺序加载：先加载科室和医生（其他模块引用），再加载患者、床位等。
 * 空文件（首次运行）可正常加载为空链表，不报错。
 */
static void loadAllHisData(void) {
    loadAdminConfig();           // 管理员持久化密码（可选文件）
    loadDeptData();              // 科室数据（无依赖，最先加载）
    loadDoctorData();            // 医生数据（关联科室 ID）
    loadBedData();               // 床位数据（关联科室 ID）
    loadPatientData();           // 患者数据（关联医生/科室 ID）
    loadRecordData();            // 医疗记录（关联患者/医生 ID）
    loadDrugData();              // 药品数据（关联科室 ID）
    loadScheduleData();          // 排班数据（关联医生 ID）
    loadAppointmentData();       // 预约数据（关联排班 ID、患者 ID）
}

/*
 * 退出前统一保存所有数据
 *
 * 运行期间各模块的修改已经即时写回文件（每次 saveXxxData()），
 * 此处作为兜底，确保内存中的最新状态全部持久化。
 */
static void saveAllHisData(void) {
    savePatientData();
    saveRecordData();
    saveDeptData();
    saveDoctorData();
    saveBedData();
    saveDrugData();
    saveScheduleData();
    saveAppointmentData();
}

/*
 * 初始化 8 个全局链表
 *
 * 每个链表调用 InitList() 分配头节点，长度为 0。
 * 必须在 loadAllHisData() 之前调用。
 */
void initGlobalLists() {
    patient_list = InitList();
    doctor_list = InitList();
    dept_list = InitList();
    bed_list = InitList();
    drug_list = InitList();
    record_list = InitList();
    schedule_list = InitList();
    appointment_list = InitList();
}

/*
 * 释放 8 个全局链表
 *
 * 遍历每个链表的节点，依次释放 data 指针和节点本身，最后释放链表头。
 * 在 saveAllHisData() 之后、exit(0) 之前调用。
 */
void freeGlobalLists() {
    FreeList(patient_list);
    FreeList(doctor_list);
    FreeList(dept_list);
    FreeList(bed_list);
    FreeList(drug_list);
    FreeList(record_list);
    FreeList(schedule_list);
    FreeList(appointment_list);
}

/*
 * 打印系统主菜单
 * 三个入口：管理员、医生、患者，以及退出选项。
 */
void printMainMenu() {
    PrintSeparator();
    printf("           HIS医院信息系统 [主菜单]\n");
    PrintSeparator();
    printf("  1. 管理员登录\n");
    printf("  2. 医生登录\n");
    printf("  3. 患者操作\n");
    printf("  0. 退出系统\n");
    PrintSeparator();
    printf("请输入选择: ");
}

/*
 * ==========================================================================
 * 管理员登录验证
 *
 * 流程：
 *   输入账号 → 输入密码 → 比对（硬编码账号 + 可持久化密码）→ 成功/失败
 *
 * 安全设计：
 *   1. 锁定机制：连续失败 5 次后锁定（static 变量，进程级，重启后恢复）
 *   2. 账号硬编码：管理员账号固定为 admin（在 his_config.h 中定义），不可修改
 *   3. 密码持久化：默认密码 123456，支持运行时通过管理员菜单修改，
 *      修改后的密码保存在 data/admin.dat 文件中
 *   4. 密码明文存储：管理员密码仅用于本地登录，风险可控
 * ==========================================================================
 */
static int adminLogin() {
    static int fail_count = 0;  /* static：跨多次调用累计失败次数 */
    if (fail_count >= 5) {
        PrintSeparator();
        printf("[锁定] 登录尝试次数过多，管理员账号已被锁定！\n");
        PrintSeparator();
        return 0;
    }

    char username[MAX_NAME_LEN];
    char password[MAX_PWD_LEN];
    char buf[MAX_LINE_LEN];

    PrintSeparator();
    printf("             管理员登录\n");
    PrintSeparator();

    printf("请输入账号：");
    if (!inputLine(buf, sizeof(buf))) return 0;
    HIS_STRNCPY(username, buf, MAX_NAME_LEN);

    printf("请输入密码：");
    if (!inputLine(buf, sizeof(buf))) return 0;
    HIS_STRNCPY(password, buf, MAX_PWD_LEN);

    if (strcmp(username, ADMIN_USERNAME) == 0 &&
        strcmp(password, s_admin_password) == 0) {
        fail_count = 0;  /* 成功后清零计数器 */
        PrintSeparator();
        printf("[登录成功] 欢迎使用HIS医院信息系统！\n");
        PrintSeparator();
        return 1;
    }
    else {
        fail_count++;
        PrintSeparator();
        printf("[登录失败] 用户名或密码错误！（%d/5次尝试）\n", fail_count);
        PrintSeparator();
        return 0;
    }
}

/*
 * ==========================================================================
 * 医生登录验证
 *
 * 密码存储方案：
 *   医生密码在 data/doctor.txt 中以 nibble-swap 混淆后的形式存储。
 *   nibble-swap 是自逆变换（f(f(x)) = x），非加密，仅防明文存储。
 *
 * 登录流程：
 *   1. 用户输入账号 + 明文密码
 *   2. 将输入的密码进行 nibble-swap 混淆
 *   3. 遍历 doctor_list，比对账号 + 混淆后密码
 *   4. 若比对失败，尝试明文兼容迁移路径（见下方说明）
 *
 * 明文兼容迁移（问题 #5 的修复）：
 *   旧版本在 loadDoctorData() 中用 isprint 判断密码是否已混淆，
 *   但 nibble-swap 后部分字符仍可打印（如 '3'→'3'），导致重启后
 *   反复混淆↔还原。修复后迁移移至登录时按需处理：
 *     先做混淆后比对，失败后用输入密码混淆 d->password（假设其为明文），
 *     若匹配说明 d->password 仍为明文，自动替换为混淆版本并保存。
 *
 * 会话跟踪：
 *   登录成功后设置 g_current_doctor_id / g_current_doctor_name，
 *   后续所有医生端操作通过这两个全局变量识别身份。
 *   医生菜单退出时清空这两个变量。
 * ==========================================================================
 */
static int doctorLogin(void) {
    static int fail_count = 0;  /* static：跨多次调用累计失败次数 */
    if (fail_count >= 5) {
        PrintSeparator();
        printf("[锁定] 登录尝试次数过多，医生登录已被锁定！\n");
        PrintSeparator();
        return 0;
    }

    /* 清除上次会话残留 */
    g_current_doctor_id[0] = '\0';
    g_current_doctor_name[0] = '\0';

    char username[MAX_NAME_LEN];
    char password[MAX_PWD_LEN];
    char buf[MAX_LINE_LEN];

    PrintSeparator();
    printf("             医生登录系统\n");
    PrintSeparator();

    /* 医生列表为空时提前提示，避免让用户以为系统卡死 */
    if (!doctor_list || doctor_list->length == 0) {
        printf("[提示] 系统中尚无医生数据，请使用管理员账号在「科室/医生」中先维护医生及登录账号。\n");
        PrintSeparator();
        return 0;
    }

    printf("请输入登录账号：");
    if (!inputLine(buf, sizeof(buf))) return 0;
    HIS_STRNCPY(username, buf, MAX_NAME_LEN);

    printf("请输入密码：");
    if (!inputLine(buf, sizeof(buf))) return 0;
    HIS_STRNCPY(password, buf, MAX_PWD_LEN);
    passwordObfuscate(password);  /* 将输入密码混淆，与文件中已混淆的密码比对 */

    ListNode* node = doctor_list->head;
    while (node) {
        Doctor* d = (Doctor*)node->data;

        /* 路径一：直接比对（输入密码混淆后 == 文件中已混淆的密码） */
        if (d && strcmp(d->account, username) == 0 && strcmp(d->password, password) == 0) {
            PrintSeparator();
            printf("[登录成功] 医生 %s 已登录。\n", d->name);
            PrintSeparator();
            fail_count = 0;
            HIS_STRNCPY(g_current_doctor_id, d->id, MAX_ID_LEN);
            HIS_STRNCPY(g_current_doctor_name, d->name, MAX_NAME_LEN);
            return 1;
        }

        /* 路径二：明文兼容迁移（旧数据中密码仍是明文） */
        if (d && strcmp(d->account, username) == 0) {
            char check[MAX_PWD_LEN];
            HIS_STRNCPY(check, d->password, MAX_PWD_LEN);
            passwordObfuscate(check);  /* 将文件中的密码混淆，与已混淆的输入比对 */
            if (strcmp(check, password) == 0) {
                /* 匹配成功 → d->password 原来是明文，替换为混淆版本 */
                HIS_STRNCPY(d->password, password, MAX_PWD_LEN);
                saveDoctorData();  /* 立即写回文件，下次登录走路径一 */
                PrintSeparator();
                printf("[登录成功] 医生 %s 已登录（已迁移密码）。\n", d->name);
                PrintSeparator();
                fail_count = 0;
                HIS_STRNCPY(g_current_doctor_id, d->id, MAX_ID_LEN);
                HIS_STRNCPY(g_current_doctor_name, d->name, MAX_NAME_LEN);
                return 1;
            }
        }
        node = node->next;
    }

    /* 遍历完整个链表仍未匹配 */
    fail_count++;
    PrintSeparator();
    printf("[登录失败] 账号或密码错误（请与医生档案中的登录账号、密码一致）。（%d/5次尝试）\n", fail_count);
    PrintSeparator();
    return 0;
}

/*
 * ==========================================================================
 * 管理员菜单
 *
 * 四个功能入口 + 返回主菜单。
 * 选择后调用对应模块的入口函数，模块内部有自己的子菜单循环。
 * 模块返回后回到此菜单。
 * ==========================================================================
 */
static void adminMenu() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("             管理员菜单\n");
        PrintSeparator();
        printf("  1. 患者与医疗记录管理\n");
        printf("  2. 科室/医生/床位管理\n");
        printf("  3. 药品药房管理\n");
        printf("  4. 修改管理员密码\n");
        printf("  0. 返回主菜单\n");
        PrintSeparator();
        printf("请输入选择：");

        choice = getValidChoice(0, 4);

        switch (choice) {
        case 1: patientModule(); break;
        case 2: dept_bedModule(); break;
        case 3: drugModule(); break;
        case 4: changeAdminPassword(); break;
        case 0: return;
        default: printf("无效输入！\n");
        }
    }
}

/*
 * ==========================================================================
 * 医生自助功能
 *
 * 以下三个函数依赖全局会话变量 g_current_doctor_id，仅在医生登录后可用。
 * viewMyProfile    — 查看当前登录医生的个人资料
 * viewMySchedule   — 查看当前医生的排班信息
 * changeDoctorPassword — 修改当前医生密码（需要验证旧密码）
 * ==========================================================================
 */

/* 查看个人信息：从 doctor_list 中查找当前医生 ID 并打印 */
static void viewMyProfile(void) {
    ListNode* node = FindNode(doctor_list, g_current_doctor_id);
    if (!node) { printf("[错误] 未找到医生信息！\n"); return; }
    Doctor* d = (Doctor*)node->data;
    printf("\n========== 个人信息 ==========\n");
    printf("  医生ID: %s\n", d->id);
    printf("  姓名: %s\n", d->name);
    printf("  科室ID: %s\n", d->dept_id);
    printf("  擅长领域: %s\n", d->specialty);
    printf("  登录账号: %s\n", d->account);
    printf("  每日挂号限额: %d\n", d->max_register);
    printf("  当前已挂号: %d\n", d->current_register);
    waitForEnter();
}

/* 查看个人排班：遍历 schedule_list 过滤当前医生 ID */
static void viewMySchedule(void) {
    printf("\n========== 我的排班 ==========\n");
    int count = 0;
    ListNode* sn = schedule_list->head;
    while (sn) {
        DoctorSchedule* s = (DoctorSchedule*)sn->data;
        if (strcmp(s->doctor_id, g_current_doctor_id) == 0) {
            count++;
            printf("  排班ID: %s | 日期: %s | 时间段: %s | 号源: %d/%d | 状态: %s\n",
                s->id, s->date, s->time_slot,
                s->current_patients, s->max_patients,
                s->is_available ? "可用" : "不可用");
        }
        sn = sn->next;
    }
    if (count == 0) printf("  暂无排班信息。\n");
    waitForEnter();
}

/*
 * 修改医生密码
 *
 * 流程：验证旧密码 → 输入新密码 → 确认新密码 → nibble-swap 混淆 → 保存
 * 旧密码验证方式：将输入明文混淆后与文件中已混淆的密码比对
 * 新密码存储方式：nibble-swap 混淆后保存到 doctor.txt
 */
static void changeDoctorPassword(void) {
    char old[MAX_PWD_LEN], new1[MAX_PWD_LEN], new2[MAX_PWD_LEN];
    printf("\n========== 修改密码 ==========\n");

    ListNode* node = FindNode(doctor_list, g_current_doctor_id);
    if (!node) { printf("[错误] 未找到医生信息！\n"); return; }
    Doctor* d = (Doctor*)node->data;

    printf("请输入当前密码: ");
    if (!inputLine(old, sizeof(old))) return;

    /* 将输入的明文密码混淆后与文件中的混淆密码比对 */
    char obfuscated_old[MAX_PWD_LEN];
    HIS_STRNCPY(obfuscated_old, old, MAX_PWD_LEN);
    passwordObfuscate(obfuscated_old);

    if (strcmp(obfuscated_old, d->password) != 0) {
        printf("[错误] 当前密码错误！\n");
        return;
    }

    printf("请输入新密码: ");
    if (!inputLine(new1, sizeof(new1))) return;
    if (strlen(new1) == 0) { printf("[错误] 密码不能为空！\n"); return; }

    printf("请再次输入新密码: ");
    if (!inputLine(new2, sizeof(new2))) return;
    if (strcmp(new1, new2) != 0) { printf("[错误] 两次输入的密码不一致！\n"); return; }

    HIS_STRNCPY(d->password, new1, MAX_PWD_LEN);
    passwordObfuscate(d->password);
    saveDoctorData();
    printf("[成功] 密码已修改！\n");
}

/*
 * ==========================================================================
 * 医生菜单（医生工作站）
 *
 * 菜单标题会显示当前医生的姓名（取自 g_current_doctor_name），
 * 以确认当前登录身份。
 *
 * 功能说明：
 *   1. 查看我的患者     — 列出挂了自己号的患者（queryPatientByDoctor）
 *   2. 管理医疗记录     — 新增诊断/处方记录、查看记录、修改就诊状态
 *   3. 查看患者预约信息  — 查看预约了自己的患者列表
 *   4. 查看我的排班     — 查看自己的排班时间段和号源
 *   5. 查看个人信息     — 查看档案（含账号）
 *   6. 修改密码         — 修改登录密码（混淆后持久化）
 *   0. 返回主菜单       — 清除医生会话变量
 * ==========================================================================
 */
static void doctorMenu() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("             医生工作站 [%s]\n", g_current_doctor_name);
        PrintSeparator();
        printf("  1. 查看我的患者\n");
        printf("  2. 管理医疗记录（新增/查看/修改状态）\n");
        printf("  3. 查看患者预约信息\n");
        printf("  4. 查看我的排班\n");
        printf("  5. 查看个人信息\n");
        printf("  6. 修改密码\n");
        printf("  0. 返回主菜单\n");
        PrintSeparator();
        printf("请输入选择：");

        choice = getValidChoice(0, 6);

        switch (choice) {
        case 1:
            queryPatientByDoctor();
            break;
        case 2:
            medicalRecordModule();
            break;
        case 3:
            queryMyAppointment();
            break;
        case 4:
            viewMySchedule();
            break;
        case 5:
            viewMyProfile();
            break;
        case 6:
            changeDoctorPassword();
            break;
        case 0:
            /* 清除医生会话，防止后续菜单误用 */
            g_current_doctor_id[0] = '\0';
            g_current_doctor_name[0] = '\0';
            printf("\n返回主菜单...\n\n");
            return;
        default:
            printf("\n[错误] 无效选择，请重新输入！\n");
        }
    }
}

/*
 * ==========================================================================
 * 患者菜单（自助服务）
 *
 * 患者无需登录，直接选择操作。部分敏感操作（查看记录、充值、取消挂号）
 * 需要验证患者 PIN 码（6位数字密码，创建时可设置）。
 *
 * 功能说明：
 *   1. 普通挂号     — 现场选择科室医生，扣费后立即就诊
 *   2. 预约挂号     — 选择医生排班时段，提前预约
 *   3. 查看挂号记录  — 查看当前挂号状态和预约记录
 *   4. 取消挂号/预约 — 退费、释放号源
 *   5. 查看医疗记录  — 只读查看自己的就诊/处方记录（PIN码保护）
 *   6. 自助充值     — 向账户余额充值（上限 10 万/次，总额不超过 50 万）
 * ==========================================================================
 */
static void patientMenu() {
    patientSelfService();
}

/*
 * ==========================================================================
 * 主函数 — 程序入口
 *
 * 初始化顺序：
 *   initGlobalLists() → loadAllHisData() → [主循环] → saveAllHisData() → freeGlobalLists()
 *
 * 主循环：
 *   显示主菜单 → 用户选择角色 → 登录验证 → 进入对应菜单
 *   菜单返回后回到主循环，再次显示主菜单。
 *   选择 0 时保存数据 → 释放内存 → 退出。
 *
 * 注意：患者操作不需要登录验证，直接进入患者菜单。
 * ==========================================================================
 */
int main(void) {
    initGlobalLists();   /* 分配 8 个全局链表的头节点 */
    loadAllHisData();    /* 从 data 目录的 txt 文件加载所有持久化数据 */
    int choice;

    while (1) {
        printMainMenu();
        choice = getValidChoice(0, 3);

        switch (choice) {
        case 1:
            if (adminLogin()) adminMenu();
            break;
        case 2:
            if (doctorLogin()) doctorMenu();
            break;
        case 3:
            patientMenu();   /* 无需登录，直接进入 */
            break;
        case 0:
            /* 退出流程：保存 → 释放 → 退出 */
            PrintSeparator();
            printf("正在保存数据并退出...\n");
            saveAllHisData();
            freeGlobalLists();
            printf("感谢使用！再见。\n");
            PrintSeparator();
            exit(0);
        default:
            printf("[错误] 无效选择，请重新输入！\n");
        }
    }
    return 0;
}
