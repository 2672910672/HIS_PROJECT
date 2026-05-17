#include "his.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/*
 * 预约挂号模块
 *   normalRegistration / appointmentRegistration
 *   selectDeptAndDoctor / selectDeptOnly
 *   viewMyRegistration / cancelMyRegistration
 *   save/loadAppointmentData — 预约记录文件持久化
 */
static void formatAppointment(void* data, char* line);
static void parseAppointment(char* line, void* data);
static void checkAndResetDoctorDaily(Doctor* d);
static int selectDeptAndDoctor(char* out_dept_id, char* out_doctor_id);

// ==================== 文件 I/O (预约) ====================
void saveAppointmentData(void) {
    SaveDataToFile(appointment_list, FILE_APPOINTMENT, formatAppointment);
}

static void formatAppointment(void* data, char* line) {
    Appointment* a = (Appointment*)data;
    snprintf(line, MAX_LINE_LEN, "%s|%s|%s|%s|%s|%lld",
        a->id, a->patient_id, a->schedule_id,
        a->status, a->create_time, a->cost);
}

void loadAppointmentData(void) {
    LoadDataFromFile(appointment_list, FILE_APPOINTMENT, parseAppointment);
}

static void parseAppointment(char* line, void* data) {
    Appointment* a = (Appointment*)data;
    memset(a, 0, sizeof(Appointment));
    char* rest = line;
    char* token;

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(a->id, token, sizeof(a->id));

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(a->patient_id, token, sizeof(a->patient_id));

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(a->schedule_id, token, sizeof(a->schedule_id));

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(a->status, token, sizeof(a->status));

    token = next_token(&rest); if (!token) return;
    HIS_STRNCPY(a->create_time, token, sizeof(a->create_time));

    token = next_token(&rest); if (!token) return;
    a->cost = atoll(token);
}

// ==================== 内部辅助函数 ====================

// 费用计算：cost（分）* (1 - insurance_ratio)，四舍五入到分
static long long calcPay(long long cost, float ratio) {
    return (long long)(cost * (1.0 - ratio) + 0.5);
}

// 共用科室选择：列出所有科室，用户选择，返回 0 成功 / -1 取消
static int selectDeptMenu(char* out_dept_id) {
    if (!dept_list || dept_list->length == 0) {
        printf("\n[错误] 系统中暂无科室数据，请联系管理员添加科室。\n");
        return -1;
    }
    printf("\n--- 选择科室 ---\n");
    printf("  可用的科室列表:\n");
    int index = 1;
    ListNode* dn = dept_list->head;
    while (dn) {
        Department* dept = (Department*)dn->data;
        printf("  %d. %s (ID: %s)\n", index, dept->name, dept->id);
        index++;
        dn = dn->next;
    }
    printf("  0. 取消\n");
    printf("请选择科室编号: ");
    int dept_choice = getValidChoice(0, dept_list->length);
    if (dept_choice == 0) return -1;

    dn = dept_list->head;
    for (int i = 1; i < dept_choice; i++) dn = dn->next;
    Department* selected_dept = (Department*)dn->data;
    HIS_STRNCPY(out_dept_id, selected_dept->id, MAX_ID_LEN);
    return 0;
}

// 检查并重置医生每日挂号数（跨日自动清零）
static void checkAndResetDoctorDaily(Doctor* d) {
    char today[32];
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    snprintf(today, sizeof(today), "%04d-%02d-%02d",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
    if (strcmp(d->register_date, today) != 0) {
        d->current_register = 0;
        HIS_STRNCPY(d->register_date, today, sizeof(d->register_date));
    }
}

// 选择科室和显示医生
static int selectDeptAndDoctor(char* out_dept_id, char* out_doctor_id) {
    if (selectDeptMenu(out_dept_id) != 0) return -1;

    ListNode* dn = FindNode(dept_list, out_dept_id);
    if (!dn) return -1;
    Department* selected_dept = (Department*)dn->data;
    printf("\n--- %s 的医生列表 ---\n", selected_dept->name);
    int doc_count = 0;
    ListNode* docn = doctor_list->head;
    while (docn) {
        Doctor* d = (Doctor*)docn->data;
        if (strcmp(d->dept_id, selected_dept->id) == 0) {
            checkAndResetDoctorDaily(d);
            doc_count++;
            if (d->max_register > 0)
                printf("  %d. %s | 擅长: %s | 剩余号源: %d/%d\n",
                    doc_count, d->name, d->specialty,
                    d->max_register - d->current_register,
                    d->max_register);
            else
                printf("  %d. %s | 擅长: %s | 剩余号源: 不限\n",
                    doc_count, d->name, d->specialty);
        }
        docn = docn->next;
    }
    if (doc_count == 0) {
        printf("\n  [提示] 该科室暂无医生，请选择其他科室。\n");
        return -1;
    }

    printf("  0. 取消\n");
    printf("请选择医生编号: ");
    int doc_choice = getValidChoice(0, doc_count);
    if (doc_choice == 0) return -1;

    docn = doctor_list->head;
    int found = 0;
    while (docn) {
        Doctor* d = (Doctor*)docn->data;
        if (strcmp(d->dept_id, selected_dept->id) == 0) {
            found++;
            if (found == doc_choice) {
                checkAndResetDoctorDaily(d);
                if (d->max_register > 0 && d->current_register >= d->max_register) {
                    printf("\n[提示] 该医生今日号源已满，请选择其他医生。\n");
                    return -1;
                }
                HIS_STRNCPY(out_doctor_id, d->id, MAX_ID_LEN);
                printf("\n已选择医生: %s\n", d->name);
                return 0;
            }
        }
        docn = docn->next;
    }
    return -1;
}

// 仅选择科室（供预约挂号用）
static int selectDeptOnly(char* out_dept_id) {
    int ret = selectDeptMenu(out_dept_id);
    if (ret == 0) {
        ListNode* dn = FindNode(dept_list, out_dept_id);
        if (dn) printf("\n已选择科室: %s\n", ((Department*)dn->data)->name);
    }
    return ret;
}

void normalRegistration(Patient* p) {
    printf("\n========== 普通挂号 ==========\n");

    if (p->register_status == REG_STATUS_PENDING ||
        p->register_status == REG_STATUS_IN_PROGRESS) {
        printf("\n[提示] 您当前已有挂号记录（状态: %s），请先就诊或取消后再重新挂号。\n",
            p->register_status == REG_STATUS_PENDING ? "待就诊" : "就诊中");
        return;
    }

    char dept_id[MAX_ID_LEN] = "", doctor_id[MAX_ID_LEN] = "";
    if (selectDeptAndDoctor(dept_id, doctor_id) != 0) {
        return;
    }

    long long cost = REGISTRATION_FEE;
    if (cost <= 0) cost = 1000;
    printf("\n挂号费用: %.2f 元", (double)cost / 100.0);
    if (p->insurance_ratio > 0) {
        long long actual = calcPay(cost, p->insurance_ratio);
        printf(" (医保报销 %.0f%%，实际支付: %.2f 元)", p->insurance_ratio * 100, (double)actual / 100.0);
    }
    printf("\n");
    printf("确认挂号? (y/n): ");
    if (!getConfirm()) {
        printf("已取消挂号。\n");
        return;
    }

    long long pay = calcPay(cost, p->insurance_ratio);
    if (p->balance < pay) {
        printf("\n[错误] 余额不足！需要 %.2f 元，当前余额 %.2f 元。\n", (double)pay / 100.0, (double)p->balance / 100.0);
        printf("请先充值后再挂号。\n");
        return;
    }

    // 先构造医疗记录，确保记录能成功创建再执行扣款等不可逆操作
    MedicalRecord record;
    memset(&record, 0, sizeof(MedicalRecord));
    if (generateUniqueID(record.id, ID_PREFIX_RECORD, record_list) != 0) {
        printf("[错误] 无法生成唯一记录ID！\n");
        return;
    }
    HIS_STRNCPY(record.patient_id, p->id, sizeof(record.patient_id));
    HIS_STRNCPY(record.doctor_id, doctor_id, sizeof(record.doctor_id));
    record.type = RECORD_REGISTER;
    record.cost = pay;
    char detail[MAX_DETAIL_LEN];
    snprintf(detail, sizeof(detail), "普通挂号 - 费用: %.2f", (double)pay / 100.0);
    HIS_STRNCPY(record.detail, detail, sizeof(record.detail));
    GetSystemTime(record.create_time);

    if (InsertNode(record_list, -1, &record, sizeof(MedicalRecord), record.id) != 0) {
        printf("[错误] 创建挂号记录失败！\n");
        return;
    }

    // 记录插入成功后再执行不可逆的扣款和状态变更
    p->balance -= pay;
    HIS_STRNCPY(p->doctor_id, doctor_id, sizeof(p->doctor_id));
    HIS_STRNCPY(p->dept_id, dept_id, sizeof(p->dept_id));
    p->register_status = REG_STATUS_PENDING;
    GetSystemTime(p->register_time);
    HIS_STRNCPY(p->register_record_id, record.id, sizeof(p->register_record_id));
    p->record_count++;

    ListNode* doc_node = FindNode(doctor_list, doctor_id);
    if (doc_node) {
        Doctor* d = (Doctor*)doc_node->data;
        checkAndResetDoctorDaily(d);
        d->current_register++;
        saveDoctorData();
    }

    saveRecordData();
    savePatientData();

    printf("\n【挂号成功】\n");
    printf("  患者: %s (ID: %s)\n", p->name, p->id);
    printf("  医生ID: %s\n", doctor_id);
    printf("  支付金额: %.2f 元\n", (double)pay / 100.0);
    printf("  剩余余额: %.2f 元\n", (double)p->balance / 100.0);
    printf("  挂号时间: %s\n", p->register_time);
}

// ==================== 预约挂号 ====================

void appointmentRegistration(Patient* p) {
    printf("\n========== 预约挂号 ==========\n");

    char dept_id[MAX_ID_LEN] = "";
    if (selectDeptOnly(dept_id) != 0) {
        return;
    }

    printf("\n--- 选择预约时间 ---\n");
    int sched_count = 0;
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    char today[11];
    snprintf(today, sizeof(today), "%04d-%02d-%02d",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

    // 遍历并显示该科室所有医生的可用排班
    DoctorSchedule* sched_array[200];
    ListNode* sn = schedule_list->head;
    while (sn && sched_count < 200) {
        DoctorSchedule* s = (DoctorSchedule*)sn->data;
        sn = sn->next;

        if (!s->is_available || s->current_patients >= s->max_patients)
            continue;
        if (strcmp(s->date, today) < 0)
            continue;

        // 查医生，确认属于所选科室
        ListNode* dn = FindNode(doctor_list, s->doctor_id);
        if (!dn) continue;
        Doctor* d = (Doctor*)dn->data;
        if (strcmp(d->dept_id, dept_id) != 0) continue;

        sched_array[sched_count] = s;
        sched_count++;
        printf("  %d. 医生: %s | 日期: %s | 时段: %s | 剩余: %d/%d\n",
            sched_count, d->name, s->date, s->time_slot,
            s->max_patients - s->current_patients, s->max_patients);
    }

    if (sched_count == 0) {
        printf("\n  [提示] 该科室暂无可预约的排班，请选择其他科室。\n");
        return;
    }

    printf("  0. 取消\n");
    printf("请选择排班编号: ");
    int choice = getValidChoice(0, sched_count);
    if (choice == 0) return;

    DoctorSchedule* selected_schedule = sched_array[choice - 1];

    long long cost = APPOINTMENT_FEE;
    if (cost <= 0) cost = 2000;
    printf("\n预约费用: %.2f 元", (double)cost / 100.0);
    if (p->insurance_ratio > 0) {
        long long actual = calcPay(cost, p->insurance_ratio);
        printf(" (医保报销 %.0f%%，实际支付: %.2f 元)", p->insurance_ratio * 100, (double)actual / 100.0);
    }
    printf("\n");

    printf("确认预约? (y/n): ");
    if (!getConfirm()) {
        printf("已取消预约。\n");
        return;
    }

    long long pay = calcPay(cost, p->insurance_ratio);
    if (p->balance < pay) {
        printf("\n[错误] 余额不足！需要 %.2f 元，当前余额 %.2f 元。\n", (double)pay / 100.0, (double)p->balance / 100.0);
        return;
    }

    p->balance -= pay;

    Appointment appt;
    memset(&appt, 0, sizeof(Appointment));
    GenerateID(appt.id, ID_PREFIX_APPOINTMENT);
    HIS_STRNCPY(appt.patient_id, p->id, sizeof(appt.patient_id));
    HIS_STRNCPY(appt.schedule_id, selected_schedule->id, sizeof(appt.schedule_id));
    HIS_STRNCPY(appt.status, "已预约", sizeof(appt.status));
    appt.cost = pay;
    GetSystemTime(appt.create_time);

    if (InsertNode(appointment_list, -1, &appt, sizeof(Appointment), appt.id) == 0) {
        selected_schedule->current_patients++;
        saveAppointmentData();
        saveScheduleData();
        savePatientData();
        printf("\n【预约成功】\n");
        printf("  患者: %s (ID: %s)\n", p->name, p->id);
        printf("  预约日期: %s %s\n", selected_schedule->date, selected_schedule->time_slot);
        printf("  支付金额: %.2f 元\n", (double)pay / 100.0);
        printf("  剩余余额: %.2f 元\n", (double)p->balance / 100.0);
        printf("  预约ID: %s\n", appt.id);
    }
    else {
        printf("\n[失败] 预约失败，请重试。\n");
        p->balance += pay;
    }
}

// ==================== 患者自己查看/取消挂号记录 ====================

void viewMyRegistration(Patient* p) {
    if (!verifyPatientPin(p)) return;
    printf("\n--- 我的挂号/预约记录 ---\n");

    printf("\n【当前挂号状态】\n");
    if (p->register_status == REG_STATUS_NONE) {
        printf("  当前未挂号\n");
    }
    else {
        printf("  状态: %s\n",
            p->register_status == REG_STATUS_PENDING ? "待就诊" :
            p->register_status == REG_STATUS_IN_PROGRESS ? "就诊中" :
            p->register_status == REG_STATUS_DONE ? "已完成" : "未知");
        if (p->register_time[0]) {
            printf("  挂号时间: %s\n", p->register_time);
        }
        if (strlen(p->doctor_id) > 0) {
            ListNode* doc_node = FindNode(doctor_list, p->doctor_id);
            if (doc_node) {
                Doctor* d = (Doctor*)doc_node->data;
                printf("  挂号医生: %s\n", d->name);
            }
        }
    }

    printf("\n【预约记录】\n");
    int appointment_count = 0;
    ListNode* ap = appointment_list->head;
    while (ap) {
        Appointment* a = (Appointment*)ap->data;
        if (strcmp(a->patient_id, p->id) == 0) {
            appointment_count++;
            printf("  预约ID: %s | 状态: %s | 时间: %s | 费用: %.2f\n",
                a->id, a->status, a->create_time, (double)a->cost / 100.0);
            if (strlen(a->schedule_id) > 0) {
                ListNode* s_node = FindNode(schedule_list, a->schedule_id);
                if (s_node) {
                    DoctorSchedule* s = (DoctorSchedule*)s_node->data;
                    printf("    排班日期: %s, 时段: %s\n", s->date, s->time_slot);
                    ListNode* doc_node = FindNode(doctor_list, s->doctor_id);
                    if (doc_node) {
                        Doctor* d = (Doctor*)doc_node->data;
                        printf("    医生: %s\n", d->name);
                    }
                }
            }
        }
        ap = ap->next;
    }
    if (appointment_count == 0) {
        printf("  暂无预约记录。\n");
    }
    waitForEnter();
}

void cancelMyRegistration(Patient* p) {
    if (!verifyPatientPin(p)) return;
    printf("\n--- 取消挂号/预约 ---\n");
    printf("  1. 取消当前现场挂号\n");
    printf("  2. 取消指定预约\n");
    printf("  0. 返回\n");
    printf("请选择: ");
    int choice = getValidChoice(0, 2);
    if (choice == 0) return;

    if (choice == 1) {
        if (p->register_status == REG_STATUS_NONE) {
            printf("\n[提示] 您当前没有挂号记录，无需取消。\n");
            return;
        }

        printf("\n确认取消现场挂号？此操作不可恢复 (y/n): ");
        if (!getConfirm()) {
            printf("\n已取消操作。\n");
            return;
        }
        {
            ListNode* rp = record_list->head;
            while (rp) {
                MedicalRecord* r = (MedicalRecord*)rp->data;
                if (strcmp(r->patient_id, p->id) == 0 &&
                    (r->type == RECORD_DIAGNOSIS || r->type == RECORD_PRESCR)) {
                    printf("[警告] 该患者已有诊断/处方记录，取消挂号不会自动删除。\n");
                    break;
                }
                rp = rp->next;
            }
        }
        if (strlen(p->doctor_id) > 0) {
            ListNode* doc_node = FindNode(doctor_list, p->doctor_id);
            if (doc_node) {
                Doctor* d = (Doctor*)doc_node->data;
                if (d->current_register > 0) d->current_register--;
                saveDoctorData();
            }
        }
        p->register_status = REG_STATUS_NONE;
        // 精确退还挂号费（通过 register_record_id 直接定位）
        if (strlen(p->register_record_id) > 0) {
            ListNode* rn = FindNode(record_list, p->register_record_id);
            if (rn) {
                MedicalRecord* rec = (MedicalRecord*)rn->data;
                p->balance += rec->cost;
                printf("  [退款] 已退还挂号费 %.2f 元\n", (double)rec->cost / 100.0);
                rec->cancelled = 1;  // 标记已取消，汇总时过滤
                saveRecordData();
            }
        }
        p->doctor_id[0] = '\0';
        p->dept_id[0] = '\0';
        p->register_time[0] = '\0';
        savePatientData();
        printf("\n[成功] 已取消现场挂号，相关费用已退还。\n");
    }
    else if (choice == 2) {
        char appt_id[MAX_ID_LEN];
        printf("请输入要取消的预约ID: ");
        inputLine(appt_id, sizeof(appt_id));

        ListNode* an = FindNode(appointment_list, appt_id);
        if (!an) {
            printf("\n[错误] 未找到该预约记录！\n");
            return;
        }
        Appointment* a = (Appointment*)an->data;
        if (strcmp(a->patient_id, p->id) != 0) {
            printf("\n[错误] 该预约不属于此患者！\n");
            return;
        }
        if (strcmp(a->status, "已取消") == 0) {
            printf("\n[提示] 该预约已经取消过了。\n");
            return;
        }

        printf("\n确认取消预约 %s ? (y/n): ", appt_id);
        if (!getConfirm()) {
            printf("\n已取消操作。\n");
            return;
        }
        HIS_STRNCPY(a->status, "已取消", sizeof(a->status));
        if (strlen(a->schedule_id) > 0) {
            ListNode* s_node = FindNode(schedule_list, a->schedule_id);
            if (s_node) {
                DoctorSchedule* s = (DoctorSchedule*)s_node->data;
                if (s->current_patients > 0) s->current_patients--;
                saveScheduleData();
            }
        }
        p->balance += a->cost;
        saveAppointmentData();
        savePatientData();
        printf("\n[成功] 预约已取消，已退还 %.2f 元。\n", (double)a->cost / 100.0);
    }
}
