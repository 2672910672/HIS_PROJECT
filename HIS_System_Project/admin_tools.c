#include "his.h"

/*
 * 全局查询统计与数据备份模块
 *   globalStatsSubMenu()  — 一站式查询（患者/医生/科室/床位/药品）
 *   printAdminStats()     — 管理员统计报表（控制台 + 导出文件）
 *   backupAllData()       — 一键备份所有模块数据到文件
 */
static void printGlobalQuery();
static void printAdminStats();
static void exportStatsToFile(int total_bed, int occupied_bed, int low_stock_count,
    long long reg_revenue, long long appt_revenue, long long drug_revenue,
    long long other_revenue, long long insurance_est);

// ==================== 全局查询统计 ====================
void globalStatsSubMenu() {
    int choice;
    while (1) {
        PrintSeparator();
        printf("           全局查询统计 [子菜单]\n");
        PrintSeparator();
        printf("  1. 一站式全局查询\n");
        printf("  2. 管理员视角统计报表\n");
        printf("  0. 返回上级\n");
        PrintSeparator();
        printf("请输入选项: ");

        choice = getValidChoice(0, 2);

        switch (choice) {
        case 1: printGlobalQuery(); break;
        case 2: printAdminStats(); break;
        case 0: return;
        default: printf("\n[错误] 无效选项\n");
        }
    }
}

static void printGlobalQuery() {
    printf("\n--- 一站式全局查询 ---\n");
    printf("1. 查询患者 (住院/门诊)\n");
    printf("2. 查询医生 (所属科室)\n");
    printf("3. 查询科室 (医生数)\n");
    printf("4. 查询床位 (状态)\n");
    printf("5. 查询药品 (库存)\n");
    printf("请选择: ");

    int choice = getValidChoice(1, 5);

    char id[MAX_ID_LEN];
    printf("\n请输入ID: ");
    readString(id, sizeof(id));

    printf("\n--- 查询结果 ---\n");
    switch (choice) {
    case 1: {
        ListNode* pat_node = FindNode(patient_list, id);
        if (pat_node) {
            Patient* p = (Patient*)pat_node->data;
            printPatientInfo(p);
        }
        else printf("[错误] 未找到该患者！\n");
        break;
    }
    case 2: {
        ListNode* doc_node = FindNode(doctor_list, id);
        if (doc_node) {
            Doctor* d = (Doctor*)doc_node->data;
            printDoctorInfo(d);
        }
        else printf("[错误] 未找到该医生！\n");
        break;
    }
    case 3: {
        ListNode* dept_node = FindNode(dept_list, id);
        if (dept_node) {
            Department* dept = (Department*)dept_node->data;
            printDeptInfo(dept);
        }
        else printf("[错误] 未找到该科室！\n");
        break;
    }
    case 4: {
        ListNode* bed_node = FindNode(bed_list, id);
        if (bed_node) {
            Bed* b = (Bed*)bed_node->data;
            printBedInfo(b);
        }
        else printf("[错误] 未找到该床位！\n");
        break;
    }
    case 5: {
        ListNode* drug_node = FindNode(drug_list, id);
        if (drug_node) {
            Drug* d = (Drug*)drug_node->data;
            printDrugInfo(d);
        }
        else printf("[错误] 未找到该药品！\n");
        break;
    }
    default:
        printf("[错误] 无效选项\n");
    }
}

static void printAdminStats() {
    printf("\n");
    PrintSeparator();
    printf("           管理员视角统计报表\n");
    PrintSeparator();

    // 1. 基础数据统计
    printf("\n[1] 基础数据概览\n");
    printf("  科室数量: %d\n", dept_list->length);
    printf("  医生数量: %d\n", doctor_list->length);
    printf("  患者数量: %d\n", patient_list->length);
    printf("  床位数量: %d\n", bed_list->length);
    printf("  药品数量: %d\n", drug_list->length);
    printf("  医疗记录数量: %d\n", record_list->length);

    // 2. 床位使用率统计
    int total_bed = 0, occupied_bed = 0;
    ListNode* p = bed_list->head;
    while (p) {
        Bed* b = (Bed*)p->data;
        total_bed++;
        if (b->status == BED_OCCUPIED) occupied_bed++;
        p = p->next;
    }
    printf("\n[2] 床位使用情况\n");
    printf("  总床位: %d\n", total_bed);
    printf("  占用床位: %d\n", occupied_bed);
    printf("  空闲床位: %d\n", total_bed - occupied_bed);
    if (total_bed > 0) {
        printf("  全院床位使用率: %.1f%%\n", (float)occupied_bed / total_bed * 100);
    }

    // 3. 药品库存概览 (简化版)
    int low_stock_count = 0;
    p = drug_list->head;
    while (p) {
        Drug* d = (Drug*)p->data;
        if (d->stock < d->warning_threshold) low_stock_count++;
        p = p->next;
    }
    printf("\n==================== 药品库存预警 ====================\n");
    printf("  当前库存低于预警阈值的药品数量：%d 种\n", low_stock_count);

    if (low_stock_count > 0) {
        printf("   提示：请及时补货！\n");
    }
    else {
        printf("   所有药品库存充足。\n");
    }

    // 4. 财务收入统计
    long long reg_revenue = 0, appt_revenue = 0, drug_revenue = 0, other_revenue = 0, insurance_est = 0;
    p = record_list->head;
    while (p) {
        MedicalRecord* r = (MedicalRecord*)p->data;
        if (!r->cancelled) {
            switch (r->type) {
            case RECORD_REGISTER: reg_revenue += r->cost; break;
            case RECORD_PRESCR:   drug_revenue += r->cost; break;
            default:              other_revenue += r->cost; break;
            }
        }
        p = p->next;
    }
    ListNode* ap = appointment_list->head;
    while (ap) {
        Appointment* a = (Appointment*)ap->data;
        if (strcmp(a->status, "已取消") != 0) {
            appt_revenue += a->cost;
        }
        ap = ap->next;
    }
    long long total_revenue = reg_revenue + appt_revenue + drug_revenue + other_revenue;
    insurance_est = (long long)(total_revenue * 0.3);
    printf("\n[3] 财务收入统计\n");
    printf("  挂号费收入: %.2f 元\n", (double)reg_revenue / 100.0);
    printf("  预约费收入: %.2f 元\n", (double)appt_revenue / 100.0);
    printf("  处方药费收入: %.2f 元\n", (double)drug_revenue / 100.0);
    printf("  其他医疗费用: %.2f 元\n", (double)other_revenue / 100.0);
    printf("  总收入合计: %.2f 元\n", (double)total_revenue / 100.0);
    printf("  医保报销预估: %.2f 元\n", (double)insurance_est / 100.0);
    printf("  患者自付合计: %.2f 元\n", (double)(total_revenue - insurance_est) / 100.0);

    exportStatsToFile(total_bed, occupied_bed, low_stock_count,
        reg_revenue, appt_revenue, drug_revenue, other_revenue, insurance_est);
    PrintSeparator();
}

// 导出统计报表到时间戳文件
static void exportStatsToFile(int total_bed, int occupied_bed, int low_stock_count,
    long long reg_revenue, long long appt_revenue, long long drug_revenue,
    long long other_revenue, long long insurance_est) {
    char now[30];
    HisGetSystemTime(now);
    char safe_time[30];
    int si = 0;
    for (int ci = 0; now[ci] && si < (int)sizeof(safe_time) - 1; ci++) {
        if (now[ci] == ':' || now[ci] == ' ') safe_time[si++] = '_';
        else safe_time[si++] = now[ci];
    }
    safe_time[si] = '\0';
    char filename[64];
    snprintf(filename, sizeof(filename), "admin_stats_%s.txt", safe_time);
    FILE* fp = fopen(filename, "w");
    if (fp) {
        fprintf(fp, "=== 管理员统计报表 ===\n");
        fprintf(fp, "生成时间: %s\n\n", now);
        fprintf(fp, "科室数量: %d\n", dept_list->length);
        fprintf(fp, "医生数量: %d\n", doctor_list->length);
        fprintf(fp, "患者数量: %d\n", patient_list->length);
        fprintf(fp, "床位数量: %d\n", bed_list->length);
        fprintf(fp, "药品数量: %d\n", drug_list->length);
        fprintf(fp, "医疗记录数量: %d\n\n", record_list->length);
        fprintf(fp, "总床位: %d | 占用: %d | 空闲: %d | 使用率: %.1f%%\n",
            total_bed, occupied_bed, total_bed - occupied_bed,
            total_bed > 0 ? (float)occupied_bed / total_bed * 100 : 0);
        fprintf(fp, "库存预警药品: %d 种\n\n", low_stock_count);
        fprintf(fp, "[3] 财务收入统计\n");
        fprintf(fp, "  挂号费收入: %.2f 元\n", (double)reg_revenue / 100.0);
        fprintf(fp, "  预约费收入: %.2f 元\n", (double)appt_revenue / 100.0);
        fprintf(fp, "  处方药费收入: %.2f 元\n", (double)drug_revenue / 100.0);
        fprintf(fp, "  其他医疗费用: %.2f 元\n", (double)other_revenue / 100.0);
        long long total_revenue = reg_revenue + appt_revenue + drug_revenue + other_revenue;
        fprintf(fp, "  总收入合计: %.2f 元\n", (double)total_revenue / 100.0);
        fprintf(fp, "  医保报销预估: %.2f 元\n", (double)insurance_est / 100.0);
        fprintf(fp, "  患者自付合计: %.2f 元\n\n", (double)(total_revenue - insurance_est) / 100.0);
        fprintf(fp, "=== 报表结束 ===\n");
        fclose(fp);
        printf("  [导出] 报表已保存至 %s\n", filename);
    }
}

// ==================== 数据备份 ====================
void backupAllData() {
    printf("\n--- 一键数据备份 ---\n");
    printf("正在备份所有数据...\n");

    saveDrugData();
    savePatientData();
    saveRecordData();
    saveDeptData();
    saveDoctorData();
    saveBedData();
    saveScheduleData();
    saveAppointmentData();

    printf("\n[成功] 所有数据已备份！\n");
    printf("  患者数据: %s\n", FILE_PATIENT);
    printf("  医生数据: %s\n", FILE_DOCTOR);
    printf("  科室数据: %s\n", FILE_DEPT);
    printf("  床位数据: %s\n", FILE_BED);
    printf("  药品数据: %s\n", FILE_DRUG);
    printf("  记录数据: %s\n", FILE_RECORD);
    printf("  排班数据: %s\n", FILE_SCHEDULE);
    printf("  预约数据: %s\n", FILE_APPOINTMENT);
    printf("\n 提示：所有数据已自动保存到当前程序目录。\n");
}
