#include "his.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/*
 * 医疗记录管理模块
 *   addRecord / modifyRecord / deleteRecord / queryRecord
 *   save/loadRecordData — 文件持久化
 *   记录类型包括：挂号、诊断、检查、住院、处方
 */
static void formatRecord(void* data, char* line);
static void parseRecord(char* line, void* data);
static void displayPatientRecords(const char* patient_id);
static void inputRecordInfo(MedicalRecord* r);
static Patient* validateDoctorPatientAccess(const char* patient_id, const char* doctor_id);
static void addMedicalRecordByDoctor(int recordType);

// ==================== 文件 I/O (医疗记录) ====================
void saveRecordData(void) {
    SaveDataToFile(record_list, FILE_RECORD, formatRecord);
}

static void formatRecord(void* data, char* line) {
    MedicalRecord* r = (MedicalRecord*)data;
    snprintf(line, MAX_LINE_LEN, "%s|%s|%s|%d|%lld|%s|%s|%d",
        r->id, r->patient_id, r->doctor_id,
        r->type, r->cost,
        r->detail[0] ? r->detail : "",
        r->create_time, r->cancelled);
}

void loadRecordData(void) {
    LoadDataFromFile(record_list, FILE_RECORD, parseRecord);
}

static void parseRecord(char* line, void* data) {
    MedicalRecord* r = (MedicalRecord*)data;
    memset(r, 0, sizeof(MedicalRecord));
    char* rest = line;
    char* token;

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(r->id, token, sizeof(r->id));

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(r->patient_id, token, sizeof(r->patient_id));

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(r->doctor_id, token, sizeof(r->doctor_id));

    token = next_token(&rest); if (!token) return;
    r->type = atoi(token);

    token = next_token(&rest); if (!token) return;
    r->cost = atoll(token);

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(r->detail, token, sizeof(r->detail));

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(r->create_time, token, sizeof(r->create_time));

    // 可选第8个字段：cancelled（向后兼容）
    token = next_token(&rest);
    if (token) r->cancelled = atoi(token);
}

// ==================== 医生端权限校验辅助 ====================
static Patient* validateDoctorPatientAccess(const char* patient_id, const char* doctor_id) {
    ListNode* pn = FindNode(patient_list, patient_id);
    if (!pn) {
        printf("[错误] 患者不存在！\n");
        return NULL;
    }
    Patient* p = (Patient*)pn->data;
    if (p->register_status == REG_STATUS_NONE) {
        printf("[提示] 该患者未挂号，请先完成挂号再创建医疗记录。\n");
        return NULL;
    }
    if (!FindNode(doctor_list, doctor_id)) {
        printf("[错误] 医生ID不存在！\n");
        return NULL;
    }
    if (strcmp(p->doctor_id, doctor_id) != 0) {
        printf("[提示] 该患者未挂此医生的号，无法创建医疗记录。\n");
        return NULL;
    }
    return p;
}

// 根据记录类型返回对应的中文名称
static const char* recordTypeName(int type) {
    return type == RECORD_DIAGNOSIS ? "诊断" :
           type == RECORD_PRESCR    ? "处方" : "记录";
}

// 医生新增诊断/处方记录（消除 case 2 与 case 3 的重复）
static void addMedicalRecordByDoctor(int recordType) {
    MedicalRecord r;
    memset(&r, 0, sizeof(MedicalRecord));
    const char* label = recordTypeName(recordType);
    printf("\n请输入患者ID: ");
    inputLine(r.patient_id, sizeof(r.patient_id));
    HIS_STRNCPY(r.doctor_id, g_current_doctor_id, MAX_ID_LEN);
    if (!validateDoctorPatientAccess(r.patient_id, r.doctor_id)) return;
    r.type = recordType;
    printf("请输入%s费用: ", label);
    char buf[MAX_LINE_LEN];
    inputLine(buf, sizeof(buf));
    r.cost = (long long)(atof(buf) * 100.0 + 0.5);
    printf("请输入%s详情: ", label);
    inputLine(r.detail, sizeof(r.detail));
    if (!ValidateNoPipe(r.detail)) { printf("详情不能包含分隔符'|'！\n"); return; }

    if (generateUniqueID(r.id, ID_PREFIX_RECORD, record_list) != 0) {
        printf("[错误] 无法生成唯一%s记录ID！\n", label);
        return;
    }
    HisGetSystemTime(r.create_time);
    if (InsertNode(record_list, -1, &r, sizeof(MedicalRecord), r.id) == 0) {
        ListNode* pn = FindNode(patient_list, r.patient_id);
        if (pn) ((Patient*)pn->data)->record_count++;
        saveRecordData();
        savePatientData();
        printf("\n[成功] %s记录已添加！\n", label);
    }
}

// ==================== 医生菜单函数 ====================

void queryPatientByDoctor(void) {
    printf("\n========== 查看我的患者 ==========\n");

    ListNode* doc_node = FindNode(doctor_list, g_current_doctor_id);
    if (!doc_node) {
        printf("\n[错误] 未找到该医生信息！\n");
        return;
    }
    Doctor* doc = (Doctor*)doc_node->data;
    printf("\n医生: %s (科室ID: %s)\n", doc->name, doc->dept_id);

    printf("\n--- 当前挂号患者列表 ---\n");
    int count = 0;
    ListNode* pn = patient_list->head;
    while (pn) {
        Patient* p = (Patient*)pn->data;
        if (strcmp(p->doctor_id, g_current_doctor_id) == 0 && p->register_status != REG_STATUS_NONE) {
            count++;
            printf("  患者ID: %s | 姓名: %s | 状态: %s | 挂号时间: %s\n",
                p->id, p->name,
                p->register_status == REG_STATUS_PENDING ? "待就诊" :
                p->register_status == REG_STATUS_IN_PROGRESS ? "就诊中" : "已完成",
                p->register_time);
        }
        pn = pn->next;
    }
    if (count == 0) {
        printf("  暂无挂号患者。\n");
    }

    // 也显示预约该医生的患者
    printf("\n--- 预约该医生的患者 ---\n");
    int appt_count = 0;
    ListNode* an = appointment_list->head;
    while (an) {
        Appointment* a = (Appointment*)an->data;
        if (strlen(a->schedule_id) > 0) {
            ListNode* sn = FindNode(schedule_list, a->schedule_id);
            if (sn) {
                DoctorSchedule* s = (DoctorSchedule*)sn->data;
                if (strcmp(s->doctor_id, g_current_doctor_id) == 0 && strcmp(a->status, "已预约") == 0) {
                    appt_count++;
                    ListNode* pn2 = FindNode(patient_list, a->patient_id);
                    char pname[MAX_NAME_LEN] = "未知";
                    if (pn2) {
                        Patient* pp = (Patient*)pn2->data;
                        HIS_STRNCPY(pname, pp->name, sizeof(pname));
                    }
                    printf("  预约ID: %s | 患者: %s | 日期: %s %s | 状态: %s\n",
                        a->id, pname, s->date, s->time_slot, a->status);
                }
            }
        }
        an = an->next;
    }
    if (appt_count == 0) {
        printf("  暂无预约患者。\n");
    }

    waitForEnter();
}

void medicalRecordModule(void) {
    int choice;
    while (1) {
        printf("\n========== 医疗记录管理 (医生端) ==========\n");
        printf("  1. 查看患者医疗记录\n");
        printf("  2. 新增诊断记录\n");
        printf("  3. 新增处方记录\n");
        printf("  4. 修改就诊状态\n");
        printf("  0. 返回\n");
        printf("请选择: ");
        choice = getValidChoice(0, 4);

        switch (choice) {
        case 1: {
            char pid[MAX_ID_LEN];
            printf("\n请输入患者ID: ");
            inputLine(pid, sizeof(pid));
            displayPatientRecords(pid);
            waitForEnter();
            break;
        }
        case 2:
            addMedicalRecordByDoctor(RECORD_DIAGNOSIS);
            break;
        case 3:
            addMedicalRecordByDoctor(RECORD_PRESCR);
            break;
        case 4: {
            char pid[MAX_ID_LEN];
            printf("\n请输入患者ID: ");
            inputLine(pid, sizeof(pid));
            ListNode* pn = FindNode(patient_list, pid);
            if (!pn) {
                printf("[错误] 患者不存在！\n");
                break;
            }
            Patient* p = (Patient*)pn->data;
            if (p->register_status == REG_STATUS_NONE) {
                printf("[提示] 该患者未挂号。\n");
                break;
            }
            if (strcmp(p->doctor_id, g_current_doctor_id) != 0) {
                printf("[提示] 该患者未挂您的号，无法修改就诊状态。\n");
                break;
            }
            printf("\n当前状态: %s\n",
                p->register_status == REG_STATUS_PENDING ? "待就诊" :
                p->register_status == REG_STATUS_IN_PROGRESS ? "就诊中" : "已完成");
            printf("请选择新状态:\n");
            printf("  1. 就诊中\n");
            printf("  2. 已完成\n");
            printf("  0. 取消\n");
            printf("请选择: ");
            int st = getValidChoice(0, 2);
            if (st == 0) break;
            if (st == 1) {
                if (p->register_status != REG_STATUS_PENDING) {
                    printf("[提示] 仅待就诊状态可转为就诊中。\n");
                    break;
                }
                p->register_status = REG_STATUS_IN_PROGRESS;
            }
            else if (st == 2) {
                if (p->register_status != REG_STATUS_IN_PROGRESS) {
                    printf("[提示] 仅就诊中状态可转为已完成。\n");
                    break;
                }
                p->register_status = REG_STATUS_DONE;
            }
            savePatientData();
            printf("\n[成功] 就诊状态已更新！\n");
            break;
        }
        case 0:
            return;
        default:
            printf("无效选择！\n");
        }
    }
}

void queryMyAppointment(void) {
    printf("\n========== 查看我的预约 ==========\n");

    ListNode* doc_node = FindNode(doctor_list, g_current_doctor_id);
    if (!doc_node) {
        printf("\n[错误] 未找到该医生信息！\n");
        return;
    }
    Doctor* doc = (Doctor*)doc_node->data;
    printf("\n医生: %s\n", doc->name);

    printf("\n--- 预约列表 ---\n");
    int count = 0;

    ListNode* sn = schedule_list->head;
    while (sn) {
        DoctorSchedule* s = (DoctorSchedule*)sn->data;
        if (strcmp(s->doctor_id, g_current_doctor_id) == 0) {
            ListNode* an = appointment_list->head;
            while (an) {
                Appointment* a = (Appointment*)an->data;
                if (strcmp(a->schedule_id, s->id) == 0) {
                    count++;
                    ListNode* pn = FindNode(patient_list, a->patient_id);
                    char pname[MAX_NAME_LEN] = "未知";
                    if (pn) {
                        Patient* pp = (Patient*)pn->data;
                        HIS_STRNCPY(pname, pp->name, sizeof(pname));
                    }
                    printf("  预约ID: %s | 患者: %s | 日期: %s %s | 状态: %s | 费用: %.2f\n",
                        a->id, pname, s->date, s->time_slot, a->status, (double)a->cost / 100.0);
                }
                an = an->next;
            }
        }
        sn = sn->next;
    }

    if (count == 0) {
        printf("  暂无预约记录。\n");
    }

    waitForEnter();
}

// ==================== 医疗记录辅助函数 ====================

static void displayPatientRecords(const char* patient_id) {
    if (!patient_id) return;
    int count = 0;
    printf("\n--- 患者的医疗记录 ---\n");
    ListNode* rp = record_list->head;
    while (rp) {
        MedicalRecord* r = (MedicalRecord*)rp->data;
        if (strcmp(r->patient_id, patient_id) == 0) {
            count++;
            char doctor_name[MAX_NAME_LEN] = "未知";
            ListNode* doc_node = FindNode(doctor_list, r->doctor_id);
            if (doc_node) {
                Doctor* d = (Doctor*)doc_node->data;
                HIS_STRNCPY(doctor_name, d->name, sizeof(doctor_name));
            }
            printf("  记录ID: %s | 医生: %s | 类型: %s | 费用: %.2f | 时间: %s\n",
                r->id, doctor_name,
                r->type == RECORD_REGISTER ? "挂号" :
                r->type == RECORD_DIAGNOSIS ? "诊断" :
                r->type == RECORD_PRESCR ? "处方" :
                r->type == RECORD_EXAM ? "检查" :
                r->type == RECORD_INHOSP ? "住院" : "其他",
                (double)r->cost / 100.0, r->create_time);
            if (r->detail[0]) {
                printf("    详情: %s\n", r->detail);
            }
        }
        rp = rp->next;
    }
    if (count == 0) {
        printf("  该患者暂无医疗记录。\n");
    }
}

static void inputRecordInfo(MedicalRecord* r) {
    char buf[MAX_LINE_LEN];

    printf("请输入患者ID: ");
    inputLine(buf, sizeof(buf));
    HIS_STRNCPY(r->patient_id, buf, sizeof(r->patient_id));

    printf("请输入医生ID: ");
    inputLine(buf, sizeof(buf));
    HIS_STRNCPY(r->doctor_id, buf, sizeof(r->doctor_id));

    while (1) {
        printf("请输入记录类型 (1-挂号 2-诊断 3-检查 4-住院 5-处方): ");
        inputLine(buf, sizeof(buf));
        r->type = atoi(buf);
        if (r->type >= 1 && r->type <= 5) break;
        printf("记录类型无效，请输入1-5之间的数字！\n");
    }

    printf("请输入费用: ");
    inputLine(buf, sizeof(buf));
    r->cost = (long long)(atof(buf) * 100.0 + 0.5);

    printf("请输入详情: ");
    inputLine(buf, sizeof(buf));
    if (!ValidateNoPipe(buf)) { printf("详情不能包含分隔符'|'！\n"); return; }
    HIS_STRNCPY(r->detail, buf, sizeof(r->detail));
}

// ==================== 管理员视角的医疗记录 CRUD ====================

void inputAndViewRecords(void) {
    char id[MAX_ID_LEN];
    printf("请输入患者ID: ");
    inputLine(id, sizeof(id));
    displayPatientRecords(id);
}

void inputAndAddRecord(void) {
    MedicalRecord r;
    memset(&r, 0, sizeof(MedicalRecord));
    inputRecordInfo(&r);

    if (!FindNode(patient_list, r.patient_id)) {
        printf("[错误] 患者ID不存在！\n");
        return;
    }

    if (!FindNode(doctor_list, r.doctor_id)) {
        printf("[错误] 医生ID不存在！\n");
        return;
    }

    // 校验：医生科室与患者挂号科室匹配
    {
        Patient* pt = (Patient*)FindNode(patient_list, r.patient_id)->data;
        Doctor* doc = (Doctor*)FindNode(doctor_list, r.doctor_id)->data;
        if (strcmp(pt->dept_id, doc->dept_id) != 0) {
            printf("[警告] 医生所属科室 (%s) 与患者科室 (%s) 不匹配！\n",
                doc->dept_id, pt->dept_id);
        }
    }

    if (generateUniqueID(r.id, ID_PREFIX_RECORD, record_list) != 0) {
        printf("[错误] 无法生成唯一记录ID！\n");
        return;
    }

    HisGetSystemTime(r.create_time);

    if (InsertNode(record_list, -1, &r, sizeof(MedicalRecord), r.id) == 0) {
        ListNode* pn = FindNode(patient_list, r.patient_id);
        if (pn) {
            Patient* p = (Patient*)pn->data;
            p->record_count++;
        }
        printf("[成功] 医疗记录 %s 添加成功！\n", r.id);
        saveRecordData();
        savePatientData();
    }
    else {
        printf("[失败] 添加医疗记录失败！\n");
    }
}

void inputAndModifyRecord(void) {
    char id[MAX_ID_LEN];
    printf("请输入要修改的记录ID: ");
    inputLine(id, sizeof(id));

    ListNode* node = FindNode(record_list, id);
    if (!node) {
        printf("[错误] 记录不存在！\n");
        return;
    }
    MedicalRecord* r = (MedicalRecord*)node->data;
    printf("当前记录: %s (患者: %s, 类型: %d)\n", r->id, r->patient_id, r->type);

    char buf[MAX_LINE_LEN];
    printf("请输入新的详情 (当前: %s): ", r->detail);
    inputLine(buf, sizeof(buf));
    if (strlen(buf) > 0) {
        if (!ValidateNoPipe(buf)) { printf("详情不能包含分隔符'|'！\n"); return; }
        HIS_STRNCPY(r->detail, buf, sizeof(r->detail));
    }

    printf("请输入新的费用 (当前: %.2f): ", (double)r->cost / 100.0);
    inputLine(buf, sizeof(buf));
    if (strlen(buf) > 0) {
        r->cost = (long long)(atof(buf) * 100.0 + 0.5);
    }

    printf("请输入新的记录类型 (1-挂号 2-诊断 3-检查 4-住院 5-处方) (当前: %d): ", r->type);
    inputLine(buf, sizeof(buf));
    if (strlen(buf) > 0) {
        int t = atoi(buf);
        if (t >= 1 && t <= 5) r->type = t;
    }

    saveRecordData();
    printf("[成功] 记录已更新！\n");
}

void inputAndDeleteRecord(void) {
    char id[MAX_ID_LEN];
    printf("请输入要删除的记录ID (输入0取消): ");
    inputLine(id, sizeof(id));
    if (strlen(id) == 0 || strcmp(id, "0") == 0) {
        printf("\n[取消] 已取消删除操作。\n");
        return;
    }

    ListNode* node = FindNode(record_list, id);
    if (!node) {
        printf("[错误] 记录不存在！\n");
        return;
    }
    MedicalRecord* r = (MedicalRecord*)node->data;
    char del_patient_id[MAX_ID_LEN];
    HIS_STRNCPY(del_patient_id, r->patient_id, MAX_ID_LEN);

    printf("确认删除记录 %s? (y/n): ", id);
    if (!getConfirm()) {
        printf("已取消。\n");
        return;
    }
    if (DeleteNode(record_list, id) == 0) {
        ListNode* pn = FindNode(patient_list, del_patient_id);
        if (pn && ((Patient*)pn->data)->record_count > 0) {
            ((Patient*)pn->data)->record_count--;
            savePatientData();
        }
        printf("[成功] 记录已删除！\n");
        saveRecordData();
    }
}
