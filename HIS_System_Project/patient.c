#include "his.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/*
 * 患者管理模块
 *   addPatient / modifyPatient / deletePatient / queryPatient
 *   patientRecharge / verifyPatientPin
 *   printPatientInfo     — 打印患者信息（跨模块调用）
 *   save/loadPatientData — 文件持久化
 *   patientViewOnlyModule — 只读查询（其他模块使用）
 *   patientModule()      — 模块入口
 */

// 患者认证宏：空指针检查 + PIN验证
#define ENSURE_PATIENT_AUTH(p) do { \
    if (!(p)) return; \
    if (!verifyPatientPin(p)) return; \
} while(0)

static int inputPatientBasicInfo(Patient* p);
static void inputAndModifyPatient(void);
static void inputAndDeletePatient(void);
static void modifyPatientInfo(Patient* p);
static void queryPatientSubMenu(void);
static void formatPatient(void* data, char* line);
static void parsePatient(char* line, void* data);

// 独立输入验证函数（跨模块调用，声明在 his.h）
int inputName(char* out_name, size_t cap);
int inputAge(int* out_age);
int inputGender(char* out_gender, size_t cap);
int inputInsuranceRatio(float* out_ratio);
int inputBalance(long long* out_balance);
int inputPhone(char* out_phone, size_t cap, const char* exclude_id);
int inputIDCard(char* out_idcard, size_t cap, const char* exclude_id, int current_age);
void inputPin(char* out_pin);

// 按 '|' 分割字符串，返回当前字段（可为空），将 *str 更新为剩余部分
// 与 strtok 不同，连续分隔符不会跳过，正确产生空字符串字段
char* next_token(char** str) {
    if (!str || !*str) return NULL;
    char* start = *str;
    char* p = strchr(start, '|');
    if (p) { *p = '\0'; *str = p + 1; }
    else { *str = NULL; }
    return start;
}

// ==================== 打印函数 ====================
void printPatientInfo(void* data) {
    Patient* p = (Patient*)data;
    printf("ID: %s | 姓名: %s | 年龄: %d | 性别: %s | 手机: %s | 身份证: %s | 余额: %.2f | 医保: %.0f%% | 状态: %s\n",
        p->id, p->name, p->age, p->gender,
        p->phone, p->id_card,
        (double)p->balance / 100.0, p->insurance_ratio * 100,
        p->is_inpatient ? "住院" : "未住院");
}

// ==================== 文件 I/O (患者) ====================
void savePatientData(void) {
    SaveDataToFile(patient_list, FILE_PATIENT, formatPatient);
}

static void formatPatient(void* data, char* line) {
    /* 格式: id|name|age|gender|insurance_ratio|balance|is_inpatient|bed_id|record_count|phone|id_card|doctor_id|dept_id|register_status|register_time|pin|register_record_id */
    Patient* p = (Patient*)data;
    char time_buf[MAX_TIME_LEN + 2] = "";
    if (p->register_time[0] != '\0') {
        time_buf[0] = '|';
        strcpy(time_buf + 1, p->register_time);
    }
    snprintf(line, MAX_LINE_LEN, "%s|%s|%d|%s|%.2f|%lld|%d|%s|%d|%s|%s|%s|%s|%d%s|%s|%s",
        p->id, p->name, p->age, p->gender,
        p->insurance_ratio, p->balance,
        p->is_inpatient, p->bed_id, p->record_count,
        p->phone, p->id_card,
        p->doctor_id, p->dept_id, p->register_status,
        time_buf, p->pin, p->register_record_id);
}

void loadPatientData(void) {
    LoadDataFromFile(patient_list, FILE_PATIENT, parsePatient);
}

static void parsePatient(char* line, void* data) {
    Patient* p = (Patient*)data;
    memset(p, 0, sizeof(Patient));
    char* rest = line;
    char* token;

    token = next_token(&rest); if (!token) return; HIS_STRNCPY(p->id, token, sizeof(p->id));
    token = next_token(&rest); if (!token) return; HIS_STRNCPY(p->name, token, sizeof(p->name));
    token = next_token(&rest); if (!token) return; p->age = atoi(token);
    token = next_token(&rest); if (!token) return; HIS_STRNCPY(p->gender, token, sizeof(p->gender));
    token = next_token(&rest); if (!token) return; p->insurance_ratio = atof(token);
    token = next_token(&rest); if (!token) return; p->balance = atoll(token);
    token = next_token(&rest); if (!token) return; p->is_inpatient = atoi(token);
    token = next_token(&rest); if (!token) return; HIS_STRNCPY(p->bed_id, token, sizeof(p->bed_id));
    token = next_token(&rest); if (!token) return; p->record_count = atoi(token);
    token = next_token(&rest); if (!token) return; HIS_STRNCPY(p->phone, token, sizeof(p->phone));
    token = next_token(&rest); if (!token) return; HIS_STRNCPY(p->id_card, token, sizeof(p->id_card));
    token = next_token(&rest); if (!token) return; HIS_STRNCPY(p->doctor_id, token, sizeof(p->doctor_id));
    token = next_token(&rest); if (!token) return; HIS_STRNCPY(p->dept_id, token, sizeof(p->dept_id));
    token = next_token(&rest); if (!token) return; p->register_status = atoi(token);

    // 可选第15个字段：register_time（向后兼容）
    token = next_token(&rest);
    if (token && token[0] != '\0') {
        HIS_STRNCPY(p->register_time, token, sizeof(p->register_time));
    }

    // 可选第16个字段：pin访问密码（向后兼容）
    token = next_token(&rest);
    if (token) {
        HIS_STRNCPY(p->pin, token, sizeof(p->pin));
    }

    // 可选第17个字段：register_record_id（向后兼容）
    token = next_token(&rest);
    if (token) {
        HIS_STRNCPY(p->register_record_id, token, sizeof(p->register_record_id));
    }
}

// ==================== 手机号唯一性检查 ====================
int isPhoneUsedByOther(const char* phone, const char* exclude_id) {
    if (!phone || strlen(phone) == 0) return 0;
    ListNode* p = patient_list->head;
    while (p) {
        Patient* pt = (Patient*)p->data;
        if (strcmp(pt->phone, phone) == 0 &&
            (!exclude_id || strcmp(pt->id, exclude_id) != 0)) {
            return 1;
        }
        p = p->next;
    }
    return 0;
}

// ==================== 身份证号辅助校验 ====================
int getAgeFromIDCard(const char* id_card) {
    if (!id_card || strlen(id_card) < 14) return -1;
    char year_str[5], month_str[3], day_str[3];
    memcpy(year_str, id_card + 6, 4); year_str[4] = '\0';
    memcpy(month_str, id_card + 10, 2); month_str[2] = '\0';
    memcpy(day_str, id_card + 12, 2); day_str[2] = '\0';
    int birth_year = atoi(year_str);
    int birth_month = atoi(month_str);
    int birth_day = atoi(day_str);
    if (birth_year < 1900 || birth_month < 1 || birth_month > 12 || birth_day < 1 || birth_day > 31)
        return -1;
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    int cur_year = tm->tm_year + 1900;
    int cur_month = tm->tm_mon + 1;
    int cur_day = tm->tm_mday;
    int age = cur_year - birth_year;
    if (cur_month < birth_month || (cur_month == birth_month && cur_day < birth_day))
        age--;
    return age;
}

int isIDCardUsedByOther(const char* id_card, const char* exclude_id) {
    if (!id_card || strlen(id_card) == 0) return 0;
    ListNode* p = patient_list->head;
    while (p) {
        Patient* pt = (Patient*)p->data;
        if (strcmp(pt->id_card, id_card) == 0 &&
            (!exclude_id || strcmp(pt->id, exclude_id) != 0)) {
            return 1;
        }
        p = p->next;
    }
    return 0;
}

// ==================== 患者查询子菜单 ====================
static void queryPatientSubMenu(void) {
    int choice;
    while (1) {
        printf("\n====== 患者查询 ======\n");
        printf("  1. 按ID查询\n");
        printf("  2. 按姓名查询（模糊查询，支持重名）\n");
        printf("  3. 遍历所有患者\n");
        printf("  4. 按挂号科室查询\n");
        printf("  5. 按就诊状态查询\n");
        printf("  0. 返回\n");
        printf("请选择: ");
        choice = getValidChoice(0, 5);

        switch (choice) {
        case 1: {
            char id[MAX_ID_LEN];
            printf("\n请输入患者ID: ");
            inputLine(id, sizeof(id));
            ListNode* node = FindNode(patient_list, id);
            if (!node) { printf("未找到该患者。\n"); break; }
            printPatientInfo(node->data);
            break;
        }
        case 2: {
            char name[MAX_NAME_LEN];
            printf("\n请输入患者姓名: ");
            inputLine(name, sizeof(name));
            if (strlen(name) == 0) { printf("姓名不能为空！\n"); break; }
            int count = 0;
            ListNode* p = patient_list->head;
            while (p) {
                Patient* pt = (Patient*)p->data;
                if (strstr(pt->name, name) != NULL) {
                    count++;
                    printf("  ID: %s | 姓名: %s | 年龄: %d | 性别: %s | 手机: %s | 状态: %s\n",
                        pt->id, pt->name, pt->age, pt->gender, pt->phone,
                        pt->register_status == REG_STATUS_NONE ? "未挂号" :
                        pt->register_status == REG_STATUS_PENDING ? "待就诊" :
                        pt->register_status == REG_STATUS_IN_PROGRESS ? "就诊中" : "已完成");
                }
                p = p->next;
            }
            printf("共找到 %d 位患者。\n", count);
            break;
        }
        case 3:
            printf("\n------ 患者列表 ------\n");
            if (!patient_list || patient_list->length == 0)
                printf("  当前无患者数据。\n");
            else
                TraverseList(patient_list, printPatientInfo);
            break;
        case 4: {
            char dept_id[MAX_ID_LEN];
            printf("\n请输入科室ID: ");
            inputLine(dept_id, sizeof(dept_id));
            ListNode* dept_node = FindNode(dept_list, dept_id);
            if (!dept_node) { printf("科室不存在！\n"); break; }
            Department* dept = (Department*)dept_node->data;
            printf("\n科室 %s (%s) 下挂号患者:\n", dept->id, dept->name);
            int count = 0;
            ListNode* p = patient_list->head;
            while (p) {
                Patient* pt = (Patient*)p->data;
                if (strcmp(pt->dept_id, dept_id) == 0 && pt->register_status != REG_STATUS_NONE) {
                    count++;
                    printf("  ID: %s | 姓名: %s | 医生ID: %s | 状态: %s\n",
                        pt->id, pt->name, pt->doctor_id,
                        pt->register_status == REG_STATUS_PENDING ? "待就诊" :
                        pt->register_status == REG_STATUS_IN_PROGRESS ? "就诊中" : "已完成");
                }
                p = p->next;
            }
            if (count == 0) printf("  该科室下无挂号患者。\n");
            break;
        }
        case 5: {
            printf("\n请选择就诊状态:\n");
            printf("  1. 待就诊\n");
            printf("  2. 就诊中\n");
            printf("  3. 已完成\n");
            printf("  0. 取消\n");
            printf("请选择: ");
            int st = getValidChoice(0, 3);
            if (st == 0) break;
            const char* label = st == 1 ? "待就诊" : st == 2 ? "就诊中" : "已完成";
            printf("\n--- %s 患者 ---\n", label);
            int count = 0;
            ListNode* p = patient_list->head;
            while (p) {
                Patient* pt = (Patient*)p->data;
                if (pt->register_status == st) {
                    count++;
                    printf("  ID: %s | 姓名: %s | 科室ID: %s | 医生ID: %s | 挂号时间: %s\n",
                        pt->id, pt->name, pt->dept_id, pt->doctor_id, pt->register_time);
                }
                p = p->next;
            }
            if (count == 0) printf("  无符合条件的患者。\n");
            break;
        }
        case 0:
            return;
        }
    }
}

// ==================== 患者管理入口 (管理员菜单) ====================
void patientModule(void) {
    int choice;
    while (1) {
        printf("\n====== 患者与医疗记录管理 ======\n");
        printf("  1. 添加患者\n");
        printf("  2. 查询患者\n");
        printf("  3. 修改患者信息\n");
        printf("  4. 删除患者\n");
        printf("  5. 医疗记录管理\n");
        printf("  0. 返回\n");
        printf("请选择: ");
        choice = getValidChoice(0, 5);

        switch (choice) {
        case 1: {
            Patient p;
            memset(&p, 0, sizeof(Patient));
            if (inputPatientBasicInfo(&p) == 0) {
                printf("\n患者创建失败！\n");
                break;
            }
            if (generateUniqueID(p.id, ID_PREFIX_PATIENT, patient_list) != 0) {
                printf("[错误] 无法生成唯一患者ID！\n");
                break;
            }

            if (InsertNode(patient_list, -1, &p, sizeof(Patient), p.id) == 0) {
                printf("\n[成功] 患者 %s (ID: %s) 添加成功！\n\n", p.name, p.id);
                savePatientData();
            }
            else {
                printf("[失败] 患者添加失败！\n");
            }
            break;
        }
        case 2:
            queryPatientSubMenu();
            break;
        case 3:
            inputAndModifyPatient();
            break;
        case 4:
            inputAndDeletePatient();
            break;
        case 5: {
            int sub;
            while (1) {
                printf("\n--- 医疗记录管理 ---\n");
                printf("  1. 查看患者的医疗记录\n");
                printf("  2. 新增医疗记录\n");
                printf("  3. 修改医疗记录\n");
                printf("  4. 删除医疗记录\n");
                printf("  0. 返回上级\n");
                printf("请选择: ");
                sub = getValidChoice(0, 4);
                switch (sub) {
                case 1: inputAndViewRecords(); break;
                case 2: inputAndAddRecord(); break;
                case 3: inputAndModifyRecord(); break;
                case 4: inputAndDeleteRecord(); break;
                case 0: goto end_patient;
                default: printf("无效选择！\n");
                }
            }
        end_patient:;
            break;
        }
        case 0:
            return;
        default:
            printf("无效选择！\n");
        }
    }
}

// ==================== 验证患者访问密码 ====================
int verifyPatientPin(Patient* p) {
    if (p->pin[0] == '\0') return 1;
    char buf[64];
    for (int tries = 0; tries < 3; tries++) {
        printf("请输入6位访问密码 (%d次尝试): ", 3 - tries);
        if (!inputLine(buf, sizeof(buf))) return 0;
        if (strcmp(buf, p->pin) == 0) return 1;
        printf("[错误] 密码错误！\n");
    }
    printf("[错误] 密码验证失败已达上限，操作取消。\n");
    return 0;
}

void patientViewOnlyModule(Patient* p) {
    ENSURE_PATIENT_AUTH(p);
    printf("\n--- 个人信息 ---\n");
    printf("  姓名: %s\n", p->name);
    printf("  年龄: %d\n", p->age);
    printf("  性别: %s\n", p->gender);
    printf("  余额: %.2f\n", (double)p->balance / 100.0);

    int count = 0;
    printf("\n--- 医疗记录 ---\n");
    ListNode* rp = record_list->head;
    while (rp) {
        MedicalRecord* r = (MedicalRecord*)rp->data;
        if (strcmp(r->patient_id, p->id) == 0) {
            count++;
            printf("  记录ID: %s | 类型: %s | 费用: %.2f | 时间: %s%s\n",
                r->id,
                r->type == RECORD_REGISTER ? "挂号" :
                r->type == RECORD_DIAGNOSIS ? "诊断" :
                r->type == RECORD_PRESCR ? "处方" :
                r->type == RECORD_EXAM ? "检查" :
                r->type == RECORD_INHOSP ? "住院" : "其他",
                (double)r->cost / 100.0, r->create_time,
                r->cancelled ? " (已取消)" : "");
            if (r->detail[0]) {
                printf("    详情: %s\n", r->detail);
            }
        }
        rp = rp->next;
    }
    if (count == 0) {
        printf("  暂无医疗记录。\n");
    }

    long long total_cost_cents = 0;
    double total_insurance_val = 0;
    int cost_count = 0;
    rp = record_list->head;
    while (rp) {
        MedicalRecord* r = (MedicalRecord*)rp->data;
        if (strcmp(r->patient_id, p->id) == 0 && !r->cancelled) {
            total_cost_cents += r->cost;
            total_insurance_val += r->cost * p->insurance_ratio;
            cost_count++;
        }
        rp = rp->next;
    }
    if (cost_count > 0) {
        printf("\n--- 费用汇总 ---\n");
        printf("  总记录数: %d\n", cost_count);
        printf("  总费用: %.2f 元\n", (double)total_cost_cents / 100.0);
        printf("  医保预计报销: %.2f 元\n", total_insurance_val / 100.0);
        printf("  预计自付: %.2f 元\n", (total_cost_cents - total_insurance_val) / 100.0);
    }

    waitForEnter();
}

// ==================== 独立输入验证函数 ====================

int inputName(char* out_name, size_t cap) {
    char buf[MAX_LINE_LEN];
    printf("请输入姓名: ");
    if (!inputLine(buf, sizeof(buf))) return 0;
    if (strlen(buf) == 0) { printf("姓名不能为空！\n"); return 0; }
    if (!ValidateNoPipe(buf)) { printf("姓名不能包含分隔符'|'！\n"); return 0; }
    HIS_STRNCPY(out_name, buf, cap);
    return 1;
}

int inputAge(int* out_age) {
    char buf[MAX_LINE_LEN];
    while (1) {
        printf("请输入年龄 (0-150): ");
        if (!inputLine(buf, sizeof(buf))) return 0;
        if (!ValidateNumber(buf)) { printf("年龄无效，请输入0-150之间的整数！\n"); continue; }
        int age = atoi(buf);
        if (age >= 0 && age <= 150) { *out_age = age; return 1; }
        printf("年龄无效，请输入0-150之间的整数！\n");
    }
}

int inputGender(char* out_gender, size_t cap) {
    char buf[MAX_LINE_LEN];
    while (1) {
        printf("请输入性别 (男/女): ");
        if (!inputLine(buf, sizeof(buf))) return 0;
        if (strcmp(buf, "男") == 0 || strcmp(buf, "女") == 0) {
            HIS_STRNCPY(out_gender, buf, cap);
            return 1;
        }
        printf("性别无效，只能输入\"男\"或\"女\"！\n");
    }
}

int inputInsuranceRatio(float* out_ratio) {
    char buf[MAX_LINE_LEN];
    while (1) {
        printf("请输入医保报销比例 (0.0~1.0): ");
        if (!inputLine(buf, sizeof(buf))) return 0;
        char* endptr;
        float r = strtof(buf, &endptr);
        while (isspace((unsigned char)*endptr)) endptr++;
        if (*endptr != '\0') { printf("请输入有效数字！\n"); continue; }
        if (r >= 0.0f && r <= 1.0f) { *out_ratio = r; return 1; }
        printf("医保报销比例无效，请输入0.0~1.0之间的小数！\n");
    }
}

int inputBalance(long long* out_balance) {
    char buf[MAX_LINE_LEN];
    while (1) {
        printf("请输入初始余额: ");
        if (!inputLine(buf, sizeof(buf))) return 0;
        char* endptr;
        double bal = strtod(buf, &endptr);
        while (isspace((unsigned char)*endptr)) endptr++;
        if (*endptr != '\0') { printf("请输入有效数字！\n"); continue; }
        if (bal >= 0) { *out_balance = (long long)(bal * 100.0 + 0.5); return 1; }
        printf("余额不能为负数！\n");
    }
}

int inputPhone(char* out_phone, size_t cap, const char* exclude_id) {
    char buf[MAX_LINE_LEN];
    while (1) {
        printf("请输入手机号 (11位手机号): ");
        if (!inputLine(buf, sizeof(buf))) return 0;
        if (!ValidatePhone(buf)) {
            printf("手机号格式错误，请输入11位数字且以1开头！\n");
            continue;
        }
        if (isPhoneUsedByOther(buf, exclude_id)) {
            printf("该手机号已被其他患者使用！\n");
            continue;
        }
        HIS_STRNCPY(out_phone, buf, cap);
        return 1;
    }
}

int inputIDCard(char* out_idcard, size_t cap, const char* exclude_id, int current_age) {
    char buf[MAX_LINE_LEN];
    while (1) {
        printf("请输入身份证号 (18位): ");
        if (!inputLine(buf, sizeof(buf))) return 0;
        if (!ValidateIDCard(buf)) { printf("身份证号格式错误，请输入18位有效身份证号！\n"); continue; }
        if (isIDCardUsedByOther(buf, exclude_id)) { printf("该身份证号已被其他患者使用！\n"); continue; }
        int id_age = getAgeFromIDCard(buf);
        if (id_age > 0 && id_age != current_age) {
            printf("身份证出生日期计算年龄为%d岁，与输入的%d岁不符，请核对！\n", id_age, current_age);
            return 0;  // 通知调用方重新输入年龄
        }
        HIS_STRNCPY(out_idcard, buf, cap);
        return 1;
    }
}

void inputPin(char* out_pin) {
    char buf[MAX_LINE_LEN];
    out_pin[0] = '\0';
    printf("请设置6位访问密码 (用于保护个人隐私，直接回车则不设置): ");
    if (!inputLine(buf, sizeof(buf))) return;
    if (strlen(buf) == 6) {
        int valid = 1;
        for (int i = 0; i < 6; i++) {
            if (buf[i] < '0' || buf[i] > '9') { valid = 0; break; }
        }
        if (valid) HIS_STRNCPY(out_pin, buf, 7);
        else printf("[提示] 密码包含非数字字符，已跳过设置。\n");
    }
    else if (strlen(buf) > 0) {
        printf("[提示] 密码长度不为6位，已跳过设置。\n");
    }
}

// ==================== 创建与修改患者 ====================

static int inputPatientBasicInfo(Patient* p) {
    if (!inputName(p->name, sizeof(p->name))) return 0;
    if (!inputGender(p->gender, sizeof(p->gender))) return 0;
    if (!inputInsuranceRatio(&p->insurance_ratio)) return 0;
    if (!inputBalance(&p->balance)) return 0;
    if (!inputPhone(p->phone, sizeof(p->phone), NULL)) return 0;
    while (1) {
        if (!inputAge(&p->age)) return 0;
        if (inputIDCard(p->id_card, sizeof(p->id_card), NULL, p->age)) break;
        printf("\n年龄和身份证号不一致，请重新输入。\n");
    }
    inputPin(p->pin);
    return 1;
}

static void modifyPatientInfo(Patient* p) {
    int choice;
    while (1) {
        printf("\n--- 修改患者信息 ---\n");
        printf("  1. 姓名: %s\n", p->name);
        printf("  2. 年龄: %d\n", p->age);
        printf("  3. 性别: %s\n", p->gender);
        printf("  4. 医保比例: %.2f\n", p->insurance_ratio);
        printf("  5. 余额: %.2f\n", (double)p->balance / 100.0);
        printf("  6. 手机号: %s\n", p->phone);
        printf("  7. 身份证: %s\n", p->id_card);
        printf("  0. 保存并返回\n");
        printf("请选择要修改的字段: ");
        choice = getValidChoice(0, 7);
        if (choice == 0) break;

        switch (choice) {
        case 1: inputName(p->name, sizeof(p->name)); break;
        case 2: inputAge(&p->age); break;
        case 3: inputGender(p->gender, sizeof(p->gender)); break;
        case 4: inputInsuranceRatio(&p->insurance_ratio); break;
        case 5: inputBalance(&p->balance); break;
        case 6: inputPhone(p->phone, sizeof(p->phone), p->id); break;
        case 7: inputIDCard(p->id_card, sizeof(p->id_card), p->id, p->age); break;
        default: printf("无效选择！\n");
        }
    }
}

static void inputAndModifyPatient(void) {
    char id[MAX_ID_LEN];
    printf("\n请输入要修改的患者ID: ");
    inputLine(id, sizeof(id));

    ListNode* node = FindNode(patient_list, id);
    if (!node) {
        printf("[错误] 未找到该患者！\n");
        return;
    }
    Patient* p = (Patient*)node->data;
    printf("\n当前患者: %s (ID: %s)\n", p->name, p->id);
    modifyPatientInfo(p);
    savePatientData();
    printf("\n[成功] 患者信息已更新！\n");
}

static void inputAndDeletePatient(void) {
    char id[MAX_ID_LEN];
    printf("\n请输入要删除的患者ID (输入0取消): ");
    inputLine(id, sizeof(id));
    if (strlen(id) == 0 || strcmp(id, "0") == 0) {
        printf("\n[取消] 已取消删除操作。\n");
        return;
    }

    ListNode* node = FindNode(patient_list, id);
    if (!node) {
        printf("[错误] 未找到该患者！\n");
        return;
    }

    Patient* p = (Patient*)node->data;

    {
        ListNode* rp = record_list->head;
        while (rp) {
            MedicalRecord* r = (MedicalRecord*)rp->data;
            if (strcmp(r->patient_id, p->id) == 0) {
                printf("\n[错误] 该患者有医疗记录，请先删除相关记录！\n");
                return;
            }
            rp = rp->next;
        }
        ListNode* ap = appointment_list->head;
        while (ap) {
            Appointment* a = (Appointment*)ap->data;
            if (strcmp(a->patient_id, id) == 0) {
                printf("\n[错误] 该患者有预约记录，请先取消预约！\n");
                return;
            }
            ap = ap->next;
        }
        if (p->is_inpatient) {
            printf("\n[错误] 该患者正在住院中，请先办理出院！\n");
            return;
        }
        if (p->register_status != REG_STATUS_NONE) {
            printf("\n[错误] 该患者当前有挂号记录，请先取消挂号！\n");
            return;
        }
    }

    char patient_name[MAX_NAME_LEN];
    HIS_STRNCPY(patient_name, p->name, sizeof(patient_name));

    printf("确认删除患者 %s (ID: %s) ? (y/n): ", patient_name, id);
    if (!getConfirm()) {
        printf("已取消。\n");
        return;
    }
    if (DeleteNode(patient_list, id) == 0) {
        printf("\n[成功] 患者 %s 已删除！\n", patient_name);
        savePatientData();
    }
    else {
        printf("[失败] 删除失败！\n");
    }
}

// ==================== 患者自助充值 ====================
void patientRecharge(Patient* p) {
    ENSURE_PATIENT_AUTH(p);

    printf("\n当前患者: %s (ID: %s)\n", p->name, p->id);
    printf("当前余额: %.2f 元\n", (double)p->balance / 100.0);

    const long long MAX_BALANCE = 50000000;       // 500000.00 元
    const long long MAX_SINGLE_AMOUNT = 10000000; // 100000.00 元

    while (1) {
        char buf[64];
        printf("\n请输入充值金额 (正数，单次不超过 %.2f 元，输入 0 取消): ", (double)MAX_SINGLE_AMOUNT / 100.0);
        if (!inputLine(buf, sizeof(buf))) {
            printf("输入异常，请重新输入\n");
            continue;
        }

        if (strcmp(buf, "0") == 0) {
            printf("\n[取消] 已取消充值操作。\n");
            return;
        }

        char* endptr;
        double amount_yuan = strtod(buf, &endptr);
        while (isspace((unsigned char)*endptr)) endptr++;
        if (*endptr != '\0' || amount_yuan <= 0) {
            printf("金额无效！请输入正数，或输入 0 取消\n");
            continue;
        }
        long long amount = (long long)(amount_yuan * 100.0 + 0.5);
        if (amount > MAX_SINGLE_AMOUNT) {
            printf("单次充值不能超过 %.2f 元！\n", (double)MAX_SINGLE_AMOUNT / 100.0);
            continue;
        }

        if (p->balance + amount > MAX_BALANCE) {
            printf("充值后余额将超过 %.2f 元上限，最多可充值 %.2f 元\n",
                (double)MAX_BALANCE / 100.0, (double)(MAX_BALANCE - p->balance) / 100.0);
            continue;
        }

        p->balance += amount;
        printf("\n【成功】充值成功！\n");
        printf("  充值金额: %.2f 元\n", (double)amount / 100.0);
        printf("  当前余额: %.2f 元\n", (double)p->balance / 100.0);
        savePatientData();
        break;
    }
}

// ==================== 创建新患者（交互式） ====================

static Patient* createNewPatientInteractive(void) {
    printf("\n--- 创建新患者 ---\n");

    Patient new_p;
    memset(&new_p, 0, sizeof(Patient));

    if (!inputName(new_p.name, sizeof(new_p.name))) return NULL;
    if (!inputGender(new_p.gender, sizeof(new_p.gender))) return NULL;
    if (!inputPhone(new_p.phone, sizeof(new_p.phone), NULL)) return NULL;

    while (1) {
        if (!inputAge(&new_p.age)) return NULL;
        if (inputIDCard(new_p.id_card, sizeof(new_p.id_card), NULL, new_p.age))
            break;
        printf("\n年龄和身份证号不一致，请重新输入。\n");
    }

    inputPin(new_p.pin);

    if (generateUniqueID(new_p.id, ID_PREFIX_PATIENT, patient_list) != 0) {
        printf("[错误] 无法生成唯一患者ID！\n");
        return NULL;
    }

    new_p.insurance_ratio = DEFAULT_INSURANCE;
    new_p.balance = 0;
    new_p.is_inpatient = 0;
    new_p.register_status = REG_STATUS_NONE;

    if (InsertNode(patient_list, -1, &new_p, sizeof(Patient), new_p.id) == 0) {
        savePatientData();
        printf("\n[成功] 新患者 %s (ID: %s) 已创建！\n", new_p.name, new_p.id);

        ListNode* new_node = FindNode(patient_list, new_p.id);
        if (new_node) return (Patient*)new_node->data;
    }

    return NULL;
}

// ==================== 患者登录 ====================

Patient* patientLogin(void) {
    while (1) {
        printf("\n====== 患者登录 ======\n");
        printf("  请输入患者 ID 登录\n");
        printf("  或输入 '1' 创建新患者\n");
        printf("  输入 '0' 退出\n");
        printf("请选择: ");
        char input[MAX_ID_LEN];
        inputLine(input, sizeof(input));

        if (strcmp(input, "0") == 0) return NULL;
        if (strcmp(input, "1") == 0 && !FindNode(patient_list, "1")) {
            Patient* p = createNewPatientInteractive();
            if (p) {
                printf("[成功] 新患者已创建，自动登录。\n");
                return p;
            }
            continue;
        }

        ListNode* node = FindNode(patient_list, input);
        if (!node) {
            printf("[错误] 未找到患者 ID '%s'。\n", input);
            continue;
        }
        Patient* p = (Patient*)node->data;
        if (!verifyPatientPin(p)) {
            printf("[错误] 密码验证失败。\n");
            continue;
        }
        return p;
    }
}

// ==================== 患者自助服务主菜单 ====================

void patientSelfService(void) {
    Patient* p = patientLogin();
    if (!p) return;

    int choice;
    do {
        printf("\n====== 患者自助服务 ======\n");
        printf("  1. 普通挂号\n");
        printf("  2. 预约挂号\n");
        printf("  3. 查看我的挂号/预约记录\n");
        printf("  4. 取消我的挂号/预约\n");
        printf("  5. 查看我的医疗记录\n");
        printf("  6. 自助充值\n");
        printf("  0. 退出登录\n");
        printf("请选择: ");
        choice = getValidChoice(0, 6);

        switch (choice) {
        case 1: normalRegistration(p); break;
        case 2: appointmentRegistration(p); break;
        case 3: viewMyRegistration(p); break;
        case 4: cancelMyRegistration(p); break;
        case 5: patientViewOnlyModule(p); break;
        case 6: patientRecharge(p); break;
        case 0: printf("已退出登录。\n"); break;
        default: printf("无效选择！\n");
        }
    } while (choice != 0);
}
