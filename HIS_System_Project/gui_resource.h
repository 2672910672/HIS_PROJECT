#ifndef GUI_RESOURCE_H
#define GUI_RESOURCE_H

/*
 * 对话框与控件资源 ID 定义
 *   所有对话框 ID 在 100-199 范围
 *   控件 ID 在 1000-1999 范围（按对话框分组）
 */

// ==================== 对话框ID ====================
#define IDD_MAIN                100     // 主窗口（角色选择）
#define IDD_LOGIN_ADMIN         101     // 管理员登录
#define IDD_LOGIN_DOCTOR        102     // 医生登录
#define IDD_ADMIN_MAIN          103     // 管理员主界面
#define IDD_DOCTOR_MAIN         104     // 医生工作站
#define IDD_PATIENT_SELECT      105     // 患者选择/创建
#define IDD_PATIENT_MAIN        106     // 患者自助服务

// ==================== 主窗口控件 ====================
#define IDC_BTN_ADMIN           1001    // 管理员按钮
#define IDC_BTN_DOCTOR          1002    // 医生按钮
#define IDC_BTN_PATIENT         1003    // 患者按钮
#define IDC_BTN_EXIT            1004    // 退出按钮
#define IDC_TITLE               1005    // 标题

// ==================== 登录对话框控件 ====================
#define IDC_EDIT_USER           1010    // 账号输入
#define IDC_EDIT_PASS           1011    // 密码输入

// ==================== 管理员主界面控件 ====================
#define IDC_TAB_ADMIN           1020    // Tab 控件
#define IDC_LIST_PATIENT        1021    // 患者列表
#define IDC_LIST_DEPT           1022    // 科室列表
#define IDC_LIST_DOCTOR         1023    // 医生列表
#define IDC_LIST_BED            1024    // 床位列表
#define IDC_LIST_DRUG           1025    // 药品列表
#define IDC_LIST_SCHEDULE       1026    // 排班列表
#define IDC_BTN_ADD             1027    // 添加按钮
#define IDC_BTN_MODIFY          1028    // 修改按钮
#define IDC_BTN_DELETE          1029    // 删除按钮
#define IDC_BTN_QUERY           1030    // 查询按钮
#define IDC_BTN_REFRESH         1031    // 刷新按钮

// ==================== 模态表单控件 ID（各表单复用以简化）====================
#define IDC_EDIT_1              2001
#define IDC_EDIT_2              2002
#define IDC_EDIT_3              2003
#define IDC_EDIT_4              2004
#define IDC_EDIT_5              2005
#define IDC_EDIT_6              2006
#define IDC_EDIT_7              2007
#define IDC_EDIT_8              2008
#define IDC_COMBO_1             2011
#define IDC_COMBO_2             2012

// ==================== 患者选择控件 ====================
#define IDC_PATIENT_LIST        1030    // 患者列表
#define IDC_BTN_PATIENT_NEW     1031    // 创建新患者
#define IDC_BTN_PATIENT_SEL     1032    // 选择已有患者

// ==================== 自定义消息 ====================
#define WM_REFRESH_DATA         (WM_APP + 100)  // 刷新所有视图

#endif
