#include "his.h"

/*
 * 通用工具函数模块
 *   ClearInputBuffer / readString / getConfirm / getValidChoice
 *   GenerateID / ValidateNumber/Phone/IDCard/NoPipe
 *   GetSystemTime / SaveDataToFile / LoadDataFromFile
 *   PrintSeparator / passwordObfuscate
 *   所有模块共用的输入校验、文件 I/O、菜单辅助函数
 */

// 清除输入缓冲区
void ClearInputBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// 统一字符串输入：使用 fgets 安全读取，去除末尾换行，溢出时清空缓冲区
void readString(char* buf, int size) {
    if (!fgets(buf, size, stdin)) {
        ClearInputBuffer();
        buf[0] = '\0';
        return;
    }
    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    else ClearInputBuffer();
}

// 安全的行输入：封装 fgets + 溢出清理，返回 1 成功 / 0 失败（EOF/错误）
int inputLine(char* buf, size_t size) {
    if (!fgets(buf, (int)size, stdin)) {
        ClearInputBuffer();
        return 0;
    }
    char* nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    else ClearInputBuffer();
    return 1;
}

// 统一确认输入：读取 y/n，返回 1 表示确认，0 表示取消
int getConfirm(void) {
    char buf[64];
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) {
        ClearInputBuffer();
        return 0;
    }
    return buf[0] == 'y' || buf[0] == 'Y';
}

// 带消息提示的统一确认：打印 msg + "(y/n): "，返回 1=确认 0=取消
int confirmAction(const char* msg) {
    if (msg) printf("%s (y/n): ", msg);
    fflush(stdout);
    return getConfirm();
}

// 等待用户按回车键继续
void waitForEnter(void) {
    printf("\n按回车键继续...");
    getchar();
}

// 统一菜单输入校验：读取[min, max]范围内的整数选项，避免 scanf 遗留问题
int getValidChoice(int min, int max) {
    char buf[64];
    int choice;
    while (1) {
        if (!inputLine(buf, sizeof(buf))) {
            printf("输入异常，请重新输入: ");
            continue;
        }

        // 检查是否全为数字
        int valid = 1;
        for (int i = 0; buf[i]; i++) {
            if (buf[i] < '0' || buf[i] > '9') {
                valid = 0;
                break;
            }
        }

        if (strlen(buf) == 0) {
            printf("输入不能为空，请重新输入 (%d-%d): ", min, max);
            continue;
        }
        if (!valid) {
            printf("输入无效，只能输入数字 (%d-%d): ", min, max);
            continue;
        }

        choice = atoi(buf);
        if (choice >= min && choice <= max) {
            return choice;
        }
        printf("输入超出范围，请重新输入 (%d-%d): ", min, max);
    }
}

// 打印菜单分隔线
void PrintSeparator() {
    printf("\n");
    for (int i = 0; i < MENU_LINE_LEN; i++) printf("=");
    printf("\n");
}

// 自动生成ID (前缀 + 时间戳6位 + 序号)
void GenerateID(char* id, char type) {
    static int seq = 1;
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    snprintf(id, MAX_ID_LEN, "%c%02d%02d%02d%03d",
        type, tm->tm_year % 100, tm->tm_mon + 1, tm->tm_mday, seq++);
}

// 安全地生成唯一ID：最多尝试 MAX_ID_RETRY 次，返回 0 成功 / -1 失败
int generateUniqueID(char* out_id, char prefix, LinkList* list) {
    for (int i = 0; i < MAX_ID_RETRY; i++) {
        GenerateID(out_id, prefix);
        if (!FindNode(list, out_id)) return 0;
    }
    return -1;
}

// 校验纯数字
int ValidateNumber(const char* str) {
    if (!str || strlen(str) == 0) return 0;
    for (int i = 0; str[i]; i++) {
        if (str[i] < '0' || str[i] > '9') return 0;
    }
    return 1;
}

// 校验ID格式
// 手机号校验：11位数字，以1开头
int ValidatePhone(const char* phone) {
    if (!phone) return 0;
    size_t len = strlen(phone);
    if (len != 11) return 0;
    if (phone[0] != '1') return 0;
    for (size_t i = 0; i < len; i++) {
        if (phone[i] < '0' || phone[i] > '9') return 0;
    }
    return 1;
}

// 身份证校验：18位，前17位为数字，末位为数字或X/x，实现18位加权
int ValidateIDCard(const char* id_card) {
    if (!id_card) return 0;
    size_t len = strlen(id_card);
    if (len != 18) return 0;
    for (size_t i = 0; i < 17; i++) {
        if (id_card[i] < '0' || id_card[i] > '9') return 0;
    }
    char last = id_card[17];
    if (!(last >= '0' && last <= '9') && last != 'X' && last != 'x') return 0;
    // 加权校验 (GB 11643-1999)
    static const int weights[17] = { 7,9,10,5,8,4,2,1,6,3,7,9,10,5,8,4,2 };
    static const char check_chars[] = "10X98765432";
    int sum = 0;
    for (size_t i = 0; i < 17; i++) {
        sum += (id_card[i] - '0') * weights[i];
    }
    int expected_idx = sum % 11;
    char expected_check = check_chars[expected_idx];
    char actual_last = (last >= 'a' && last <= 'z') ? last - 'a' + 'A' : last;
    if (actual_last != expected_check) {
        printf("    [提示] 校验位应为 %c，实际输入为 %c\n", expected_check, actual_last);
    }
    return actual_last == expected_check;
}

// 防止字段分隔符"|"检测
int ValidateNoPipe(const char* str) {
    return str && strchr(str, '|') == NULL;
}

void passwordObfuscate(char* pwd) {
    if (!pwd) return;
    for (int i = 0; pwd[i]; i++) {
        pwd[i] = ((pwd[i] << 4) | ((unsigned char)pwd[i] >> 4));
    }
}

void GetSystemTime(char* time_str) {
    time_t t = time(NULL);
    struct tm* tm = localtime(&t);
    snprintf(time_str, MAX_TIME_LEN, "%04d-%02d-%02d %02d:%02d:%02d",
        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
        tm->tm_hour, tm->tm_min, tm->tm_sec);
}

// 保存数据到文件
int SaveDataToFile(LinkList* list, const char* filename, void (*format_func)(void*, char*)) {
    if (!list || !filename || !format_func) return -1;
    char tmpname[MAX_LINE_LEN];
    snprintf(tmpname, sizeof(tmpname), "%s.tmp", filename);
    FILE* fp = fopen(tmpname, "w");
    if (!fp) return -1;

    ListNode* p = list->head;
    char line[MAX_LINE_LEN];
    while (p) {
        format_func(p->data, line);
        fprintf(fp, "%s\n", line);
        p = p->next;
    }
    fclose(fp);

    remove(filename);
    if (rename(tmpname, filename) != 0) return -1;
    return 0;
}

int LoadDataFromFile(LinkList* list, const char* filename, void (*parse_func)(char*, void*)) {
    if (!list || !filename || !parse_func) return -1;
    FILE* fp = fopen(filename, "r");
    if (!fp) return -1;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        // 修改：去掉可能的 \r (Windows换行符)
        int len = strlen(line);
        if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';

        // 跳过空行或只有分隔符的行
        int all_sep = 1;
        for (int i = 0; line[i]; i++) {
            if (line[i] != '|' && line[i] != ' ') { all_sep = 0; break; }
        }
        if (strlen(line) == 0 || line[0] == '|' || line[0] == '\0' || all_sep) continue;
        // 跳过以|开头或只有分隔符的空行，修复之前bug产生的空白记录

        void* data = malloc(MAX_DATA_SIZE);
        if (!data) continue;
        memset(data, 0, MAX_DATA_SIZE);
        parse_func(line, data);

        // 检查解析的ID是否有效，ID不为空，且第一个字符必须是前缀字符
        const char* parsed_id = (const char*)data;
        if (strlen(parsed_id) < 4) {
            // ID太短，跳过无效记录，释放内存
            free(data);
            continue;
        }
        InsertNode(list, -1, data, MAX_DATA_SIZE, parsed_id);
        free(data);
    }
    fclose(fp);
    return 0;
}