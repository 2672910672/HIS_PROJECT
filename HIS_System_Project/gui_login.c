#include "his.h"
#include "gui_main.h"

/* his_main.c 的导出函数 */
extern int verifyAdminPassword(const char* username, const char* password);

/*
 * 登录窗口过程（共用：管理员/医生）
 */
LRESULT CALLBACK LoginWndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static int s_isAdmin = 1;
    static int s_failCount = 0;

    switch (msg) {
    case WM_CREATE: {
        /* 顶部蓝色装饰条 */
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 280, 6, hWnd, (HMENU)1100, g_hInst, NULL);
        CreateWindowW(L"STATIC", L"账号:", WS_CHILD | WS_VISIBLE,
                     30, 25, 40, 15, hWnd, (HMENU)-1, g_hInst, NULL);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                     80, 23, 160, 20, hWnd, (HMENU)IDC_EDIT_USER, g_hInst, NULL);
        CreateWindowW(L"STATIC", L"密码:", WS_CHILD | WS_VISIBLE,
                     30, 58, 40, 15, hWnd, (HMENU)-1, g_hInst, NULL);
        CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
                     80, 56, 160, 20, hWnd, (HMENU)IDC_EDIT_PASS, g_hInst, NULL);
        CreateWindowW(L"BUTTON", L"确定", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                     75, 110, 55, 25, hWnd, (HMENU)IDOK, g_hInst, NULL);
        CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                     145, 110, 55, 25, hWnd, (HMENU)IDCANCEL, g_hInst, NULL);

        gui_SetDialogFont(hWnd, g_hFont);
        s_failCount = 0;

        WCHAR wTitle[64];
        GetWindowTextW(hWnd, wTitle, 64);
        s_isAdmin = (wcsstr(wTitle, L"管理员") != NULL);
        SetFocus(GetDlgItem(hWnd, IDC_EDIT_USER));
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)w;
        int id = GetDlgCtrlID((HWND)l);
        if (id == 1100) {
            static HBRUSH hBr = NULL;
            if (!hBr) hBr = CreateSolidBrush(RGB(25, 110, 180));
            return (LRESULT)hBr;
        }
        SetTextColor(hdc, RGB(40, 85, 130));
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)(HBRUSH)GetStockObject(NULL_BRUSH);
    }

    case WM_COMMAND:
        if (LOWORD(w) == IDCANCEL) {
            g_modalResult = IDCANCEL;
            DestroyWindow(hWnd);
            return 0;
        }
        if (LOWORD(w) == IDOK) {
            if (s_failCount >= 5) {
                gui_MsgBox(hWnd, "登录尝试次数过多，已锁定！", "锁定", MB_ICONERROR);
                g_modalResult = IDCANCEL;
                DestroyWindow(hWnd);
                return 0;
            }

            char username[50], password[50];
            GetDlgItemTextA(hWnd, IDC_EDIT_USER, username, sizeof(username));
            GetDlgItemTextA(hWnd, IDC_EDIT_PASS, password, sizeof(password));

            if (!username[0] || !password[0]) {
                gui_MsgBox(hWnd, "请输入账号和密码！", "提示", MB_ICONINFORMATION);
                return 0;
            }

            if (s_isAdmin) {
                if (verifyAdminPassword(username, password)) {
                    g_modalResult = IDOK;
                    DestroyWindow(hWnd);
                    return 0;
                }
                s_failCount++;
                gui_MsgBox(hWnd, "账号或密码错误！\n(连续5次失败将锁定)", "登录失败", MB_ICONERROR);
                SetDlgItemTextA(hWnd, IDC_EDIT_PASS, "");
                SetFocus(GetDlgItem(hWnd, IDC_EDIT_PASS));
            } else {
                if (!doctor_list || doctor_list->length == 0) {
                    gui_MsgBox(hWnd, "系统中暂无医生数据。", "提示", MB_ICONINFORMATION);
                    return 0;
                }
                g_current_doctor_id[0] = '\0';
                g_current_doctor_name[0] = '\0';

                char obfuscated[MAX_PWD_LEN];
                HIS_STRNCPY(obfuscated, password, MAX_PWD_LEN);
                passwordObfuscate(obfuscated);

                int found = 0, success = 0;
                ListNode* node = doctor_list->head;
                while (node) {
                    Doctor* d = (Doctor*)node->data;
                    if (strcmp(d->account, username) != 0) { node = node->next; continue; }
                    found = 1;
                    if (strcmp(d->password, obfuscated) == 0) success = 1;
                    else {
                        char check[MAX_PWD_LEN];
                        HIS_STRNCPY(check, d->password, MAX_PWD_LEN);
                        passwordObfuscate(check);
                        if (strcmp(check, obfuscated) == 0) {
                            HIS_STRNCPY(d->password, obfuscated, MAX_PWD_LEN);
                            saveDoctorData();
                            success = 1;
                        }
                    }
                    if (success) {
                        HIS_STRNCPY(g_current_doctor_id, d->id, MAX_ID_LEN);
                        HIS_STRNCPY(g_current_doctor_name, d->name, MAX_NAME_LEN);
                        g_modalResult = IDOK;
                        DestroyWindow(hWnd);
                        return 0;
                    }
                    break;
                }
                s_failCount++;
                gui_MsgBox(hWnd, found ? "密码错误！" : "账号不存在！",
                    "登录失败", MB_ICONERROR);
                SetDlgItemTextA(hWnd, IDC_EDIT_PASS, "");
                SetFocus(GetDlgItem(hWnd, IDC_EDIT_PASS));
            }
            return 0;
        }
        return 0;

    case WM_CLOSE:
        g_modalResult = IDCANCEL;
        DestroyWindow(hWnd);
        return 0;
    }
    return DefWindowProc(hWnd, msg, w, l);
}
