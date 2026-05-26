#include "his.h"
#include "gui_main.h"
#include <windows.h>

/* 编码转换：系统默认代码页 (GBK on Chinese Windows) */
void acpToWide(const char* src, WCHAR* dst, int dstLen) {
    if (!src || !dst || dstLen <= 0) return;
    MultiByteToWideChar(CP_ACP, 0, src, -1, dst, dstLen);
    dst[dstLen - 1] = L'\0';
}

/* 编码转换：UTF-8 → WCHAR（用于源码中的字符串字面量） */
void utf8ToWide(const char* src, WCHAR* dst, int dstLen) {
    if (!src || !dst || dstLen <= 0) return;
    MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, dstLen);
    dst[dstLen - 1] = L'\0';
}

/*
 * ListView 斑马纹（交替行背景）
 *   偶数行浅蓝背景，奇数行白色
 *   在 WM_NOTIFY 中调用：
 *     if (h->code == NM_CUSTOMDRAW) return gui_HandleListCustomDraw((LPNMLVCUSTOMDRAW)l);
 */
int gui_HandleListCustomDraw(LPNMLVCUSTOMDRAW lvcd) {
    switch (lvcd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        lvcd->clrTextBk = (lvcd->nmcd.dwItemSpec % 2 == 0) ? RGB(238, 245, 255) : RGB(255, 255, 255);
        return CDRF_NEWFONT;
    default:
        return 0;
    }
}

/* 通用消息框：窄字符串（exe 中为系统代码页）→ MessageBoxW */
int gui_MsgBox(HWND hParent, const char* msg, const char* title, UINT type) {
    WCHAR wmsg[512], wtitle[128];
    MultiByteToWideChar(CP_ACP, 0, msg ? msg : "", -1, wmsg, 512);
    MultiByteToWideChar(CP_ACP, 0, title ? title : "", -1, wtitle, 128);
    return MessageBoxW(hParent, wmsg, wtitle, type);
}

/*
 * 全局字体创建
 */
HFONT gui_CreateDefaultFont(void) {
    LOGFONT lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight         = -MulDiv(10, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72);
    lf.lfWeight         = FW_NORMAL;
    lf.lfCharSet        = GB2312_CHARSET;
    lf.lfQuality        = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"\x5FAE\x8F6F\x96C5\x9ED1");
    return CreateFontIndirect(&lf);
}

/* 子控件字体设置回调 */
static BOOL CALLBACK SetChildFontEnum(HWND hChild, LPARAM lParam) {
    SendMessage(hChild, WM_SETFONT, (WPARAM)lParam, MAKELPARAM(TRUE, 0));
    return TRUE;
}

void gui_SetDialogFont(HWND hDlg, HFONT hFont) {
    EnumChildWindows(hDlg, SetChildFontEnum, (LPARAM)hFont);
}

/*
 * 初始化 ListView 列（Unicode 版本）
 */
void gui_InitListView(HWND hLV, const char* columns[], int widths[], int fmt[], int colCount) {
    LV_COLUMNW lvc;
    ZeroMemory(&lvc, sizeof(lvc));
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;

    for (int i = 0; i < colCount; i++) {
        lvc.iSubItem = i;
        WCHAR wTitle[64];
        /* 列标题窄字面量在 exe 中是系统代码页 (GBK) */
        acpToWide(columns[i], wTitle, 64);
        lvc.pszText = wTitle;
        lvc.fmt     = fmt ? fmt[i] : LVCFMT_LEFT;
        lvc.cx      = widths ? widths[i] : 100;

        if (widths && widths[i] == -1) {
            RECT rc;
            GetClientRect(hLV, &rc);
            int used = 0;
            for (int j = 0; j < i; j++) used += widths[j];
            lvc.cx = (rc.right - rc.left - used - 4) > 60
                     ? (rc.right - rc.left - used - 4) : 60;
        }
        ListView_InsertColumn(hLV, i, &lvc);
    }
    /* 全局 ListView 美化：网格线 + 整行高亮 + 双缓冲 */
    ListView_SetExtendedListViewStyle(hLV, LVS_EX_GRIDLINES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
}

int gui_Confirm(HWND hParent, const char* msg) {
    return gui_MsgBox(hParent, msg, "确认", MB_YESNO | MB_ICONQUESTION) == IDYES;
}

/*
 * 简单输入对话框窗口过程
 */
static LRESULT CALLBACK InputDlgProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    switch(msg){
    case WM_CREATE:{
        CREATESTRUCT* cs=(CREATESTRUCT*)l;
        const char* prompt = (const char*)cs->lpCreateParams;
        HWND hStatic=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,15,15,250,18,hWnd,(HMENU)-1,g_hInst,NULL);
        SetWindowTextA(hStatic,prompt);
        CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER,15,40,250,22,hWnd,(HMENU)IDC_EDIT_1,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"确定",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,60,75,70,28,hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,150,75,70,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont); return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            g_modalResult = IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/*
 * 简单输入对话框
 *   显示提示文本 + 编辑框 + 确定/取消
 *   用户输入内容写入 out_buf，返回 1=确定 0=取消
 */
int gui_InputDialog(HWND hParent, const char* title, const char* prompt, char* out_buf, int out_cap) {
    WCHAR wtitle[128]; acpToWide(title,wtitle,128);
    HWND hDlg=CreateWindowW(L"InputDlg",wtitle,WS_CAPTION|WS_SYSMENU,
        CW_USEDEFAULT,CW_USEDEFAULT,300,145,hParent,NULL,g_hInst,(void*)prompt);
    int result=DoModal(hDlg,300,145);
    if(result==IDOK)
        GetDlgItemTextA(hDlg,IDC_EDIT_1,out_buf,out_cap);
    DestroyWindow(hDlg);
    return result==IDOK?1:0;
}

/* 注册输入对话框类（在 WinMain 中调用） */
void gui_RegisterInputDlgClass(HINSTANCE hInst) {
    WNDCLASS wc={.style=CS_HREDRAW|CS_VREDRAW,.hInstance=hInst,.hCursor=LoadCursor(NULL,IDC_ARROW),
                  .hbrBackground=(HBRUSH)(COLOR_BTNFACE+1),.lpszMenuName=NULL};
    wc.lpfnWndProc=InputDlgProc; wc.lpszClassName=L"InputDlg";
    RegisterClass(&wc);
}

// ==================== GUI 版 inputXxx 包装器 ====================
int gui_InputName(HWND hDlg, int ctrlID, char* out, size_t cap) {
    if (!GetDlgItemTextA(hDlg, ctrlID, out, (int)cap)) {
        MessageBoxA(hDlg, "姓名不能为空！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == ' ') out[--len] = '\0';
    if (!ValidateNoPipe(out)) {
        MessageBoxA(hDlg, "姓名不能包含符号'|'", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    return 1;
}

int gui_InputAge(HWND hDlg, int ctrlID, int* out) {
    char buf[16];
    if (!GetDlgItemTextA(hDlg, ctrlID, buf, sizeof(buf)) || strlen(buf) == 0) {
        MessageBoxA(hDlg, "年龄不能为空！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    if (!ValidateNumber(buf)) {
        MessageBoxA(hDlg, "年龄必须为数字！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    int age = atoi(buf);
    if (age < 0 || age > 150) {
        MessageBoxA(hDlg, "年龄必须在 0-150 之间！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    *out = age; return 1;
}

int gui_InputGender(HWND hDlg, int ctrlID, char* out, size_t cap) {
    if (!GetDlgItemTextA(hDlg, ctrlID, out, (int)cap)) {
        MessageBoxA(hDlg, "性别不能为空！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    if (strcmp(out, "男") != 0 && strcmp(out, "女") != 0) {
        MessageBoxA(hDlg, "性别只能输入'男'或'女'！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    return 1;
}

int gui_InputPhone(HWND hDlg, int ctrlID, char* out, size_t cap, const char* exclude_id) {
    if (!GetDlgItemTextA(hDlg, ctrlID, out, (int)cap)) {
        MessageBoxA(hDlg, "手机号不能为空！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    if (!ValidatePhone(out)) {
        MessageBoxA(hDlg, "手机号必须为11位数字且以1开头！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    if (isPhoneUsedByOther(out, exclude_id ? exclude_id : "")) {
        MessageBoxA(hDlg, "该手机号已被其他患者使用！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    return 1;
}

int gui_InputIDCard(HWND hDlg, int ctrlID, char* out, size_t cap, const char* exclude_id, int current_age) {
    if (!GetDlgItemTextA(hDlg, ctrlID, out, (int)cap)) {
        MessageBoxA(hDlg, "身份证号不能为空！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    if (!ValidateIDCard(out)) {
        MessageBoxA(hDlg, "身份证号格式不正确！\n(应为18位, 末位可为X)", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    if (isIDCardUsedByOther(out, exclude_id ? exclude_id : "")) {
        MessageBoxA(hDlg, "该身份证号已被其他患者使用！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    return 1;
}

int gui_InputBalance(HWND hDlg, int ctrlID, long long* out) {
    char buf[32];
    if (!GetDlgItemTextA(hDlg, ctrlID, buf, sizeof(buf)) || strlen(buf) == 0) {
        MessageBoxA(hDlg, "余额不能为空！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    if (!ValidateNumber(buf)) {
        MessageBoxA(hDlg, "余额必须为数字（单位：分）！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    long long val = atoll(buf);
    if (val < 0 || val > 50000000) {
        MessageBoxA(hDlg, "余额范围 0 ~ 500000 元！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    *out = val; return 1;
}

int gui_InputInsuranceRatio(HWND hDlg, int ctrlID, float* out) {
    char buf[16];
    if (!GetDlgItemTextA(hDlg, ctrlID, buf, sizeof(buf)) || strlen(buf) == 0) {
        MessageBoxA(hDlg, "医保比例不能为空！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    float val = (float)atof(buf);
    if (val < 0.0f || val > 1.0f) {
        MessageBoxA(hDlg, "医保比例必须在 0.0 ~ 1.0 之间！", "输入错误", MB_ICONERROR);
        SetFocus(GetDlgItem(hDlg, ctrlID)); return 0; }
    *out = val; return 1;
}
