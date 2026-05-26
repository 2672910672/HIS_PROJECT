#ifndef GUI_MAIN_H
#define GUI_MAIN_H

#include <windows.h>
#include <commctrl.h>
#include "gui_resource.h"

/*
 * 全局句柄 — 在 gui_main.c WinMain 中初始化，所有 GUI 模块共用
 */
extern HINSTANCE g_hInst;       // 应用程序实例句柄
extern HWND      g_hMainWnd;    // 主窗口句柄
extern HFONT     g_hFont;       // 全局默认字体（微软雅黑 9pt）

/*
 * 模态循环：创建的程序化窗口调用此函数进入模态
 *   hDlg: 窗口句柄, width/height: 窗口尺寸用于居中
 *   返回用户数据（IDOK / IDCANCEL 等）
 */
int DoModal(HWND hDlg, int width, int height);

/*
 * 全局刷新机制
 *   任何 CRUD 操作成功后调用 gui_RefreshAllViews()
 */
void gui_RefreshAllViews(void);

/* 患者选择窗口选中的患者ID */
extern char g_patientId[MAX_ID_LEN];

/* 模态对话框结果（在窗口销毁前设置，DoModal 在销毁后读取） */
extern int g_modalResult;

/* 通用输入对话框 */
int gui_InputDialog(HWND hParent, const char* title, const char* prompt, char* out_buf, int out_cap);

/* 各模块窗口类注册（在 WinMain 中调用） */
void gui_RegisterAdminClasses(HINSTANCE hInst);
void gui_RegisterDoctorClasses(HINSTANCE hInst);
void gui_RegisterPatientClasses(HINSTANCE hInst);
void gui_RegisterInputDlgClass(HINSTANCE hInst);

/*
 * 创建全局默认字体
 *   微软雅黑 9pt, CLEARTYPE_QUALITY, GB2312_CHARSET
 *   返回字体句柄，进程退出时需 DeleteObject
 */
HFONT gui_CreateDefaultFont(void);

/*
 * 为对话框的所有子控件设置字体
 *   在 WM_INITDIALOG 中调用
 */
void gui_SetDialogFont(HWND hDlg, HFONT hFont);

/*
 * 初始化 ListView 列
 *   支持 LVCFMT_LEFT/CENTER/RIGHT 对齐
 */
void gui_InitListView(HWND hLV, const char* columns[], int widths[], int fmt[], int colCount);

/*
 * 包装器函数（GUI 版 inputXxx，复用已有验证逻辑）
 */
int gui_InputName(HWND hDlg, int ctrlID, char* out, size_t cap);
int gui_InputAge(HWND hDlg, int ctrlID, int* out);
int gui_InputGender(HWND hDlg, int ctrlID, char* out, size_t cap);
int gui_InputPhone(HWND hDlg, int ctrlID, char* out, size_t cap, const char* exclude_id);
int gui_InputIDCard(HWND hDlg, int ctrlID, char* out, size_t cap, const char* exclude_id, int current_age);
int gui_InputBalance(HWND hDlg, int ctrlID, long long* out);
int gui_InputInsuranceRatio(HWND hDlg, int ctrlID, float* out);

/* 编码转换 */
void acpToWide(const char* src, WCHAR* dst, int dstLen);   /* 系统代码页 (GBK) → WCHAR */
void utf8ToWide(const char* src, WCHAR* dst, int dstLen);  /* UTF-8 → WCHAR */

/* 通用消息框：UTF-8 字符串 → MessageBoxW */
int gui_MsgBox(HWND hParent, const char* msg, const char* title, UINT type);

/* 通用确认对话框 (UTF-8 字符串) */
int gui_Confirm(HWND hParent, const char* msg);

/*
 * ListView 斑马纹辅助函数
 *   在 WM_NOTIFY 中调用：if (h->code == NM_CUSTOMDRAW) return gui_HandleListCustomDraw((LPNMLVCUSTOMDRAW)l);
 */
int gui_HandleListCustomDraw(LPNMLVCUSTOMDRAW lvcd);

#endif
