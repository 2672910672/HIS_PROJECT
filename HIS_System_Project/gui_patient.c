#include "his.h"
#include "gui_main.h"
#pragma warning(disable:4312)

/*
 * 患者自助服务模块
 *   患者选择/创建 → PIN验证 → 主菜单（7个功能按钮）
 */

char g_patientId[MAX_ID_LEN]; /* 当前选中患者ID（跨文件访问） */

static void toWide(const char* s, WCHAR* d, int n) { MultiByteToWideChar(CP_ACP,0,s,-1,d,n); d[n-1]=0; }
static void toMulti(const WCHAR* s, char* d, int n) { WideCharToMultiByte(CP_ACP,0,s,-1,d,n,NULL,NULL); d[n-1]=0; }

/* ==================== PIN 验证对话框 ==================== */
LRESULT CALLBACK PinVerifyProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    switch(msg){
    case WM_CREATE:{
        CreateWindowW(L"STATIC",L"请输入6位PIN密码:",WS_CHILD|WS_VISIBLE,20,20,200,18,hWnd,(HMENU)-1,g_hInst,NULL);
        CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_PASSWORD|ES_NUMBER,50,45,180,22,hWnd,(HMENU)IDC_EDIT_1,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"确定",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,50,85,60,28,hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,170,85,60,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont); return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            char pin[16]; GetDlgItemTextA(hWnd,IDC_EDIT_1,pin,sizeof(pin));
            ListNode* n=FindNode(patient_list,g_patientId);
            if(!n){gui_MsgBox(hWnd,"患者不存在！","错误",MB_ICONERROR);return 0;}
            Patient* p=(Patient*)n->data;
            if(strcmp(p->pin,pin)!=0){gui_MsgBox(hWnd,"PIN密码错误！","验证失败",MB_ICONERROR);return 0;}
            g_modalResult = IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== 充值对话框 ==================== */
LRESULT CALLBACK RechargeFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    switch(msg){
    case WM_CREATE:{
        CreateWindowW(L"STATIC",L"充值金额(元):",WS_CHILD|WS_VISIBLE,20,25,120,18,hWnd,(HMENU)-1,g_hInst,NULL);
        CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_NUMBER,105,23,150,22,hWnd,(HMENU)IDC_EDIT_1,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"确定",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,60,70,70,28,hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,160,70,70,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont); return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            char buf[32]; GetDlgItemTextA(hWnd,IDC_EDIT_1,buf,sizeof(buf));
            long long amount = atoll(buf)*100; // 元转分
            if(amount<=0||amount>10000000){gui_MsgBox(hWnd,"请输入1-100000之间的金额(元)！","错误",MB_ICONERROR);return 0;}
            ListNode* n=FindNode(patient_list,g_patientId);
            if(!n){gui_MsgBox(hWnd,"患者不存在！","错误",MB_ICONERROR);return 0;}
            Patient* p=(Patient*)n->data;
            if(p->balance+amount>50000000){gui_MsgBox(hWnd,"余额超过上限50万元！","错误",MB_ICONERROR);return 0;}
            p->balance+=amount;
            savePatientData();
            char msg[128]; snprintf(msg,sizeof(msg),"充值成功！当前余额: %.2f元",p->balance/100.0);
            gui_MsgBox(hWnd,msg,"成功",MB_ICONINFORMATION);
            g_modalResult = IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== 医疗记录查看 ==================== */
LRESULT CALLBACK ViewRecordsProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static HWND s_hLV;
    switch(msg){
    case WM_CREATE:{
        s_hLV=CreateWindowW(WC_LISTVIEW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT,
            10,10,460,310,hWnd,(HMENU)1001,g_hInst,NULL);
        const char* cols[]={"记录ID","类型","费用(元)","详情","时间"};
        int ws[]={70,50,70,180,70};
        gui_InitListView(s_hLV,cols,ws,NULL,5);
        /* 加载该患者的记录 */
        int i=0; ListNode* n=record_list?record_list->head:NULL;
        while(n){
            MedicalRecord* r=(MedicalRecord*)n->data;
            if(r&&strcmp(r->patient_id,g_patientId)==0){
                WCHAR id[32],type[32],cost[32],detail[256],time[64]; char buf[128];
                toWide(r->id,id,32);
                wcscpy_s(type,32,r->type==1?L"挂号":r->type==2?L"诊断":r->type==3?L"检查":r->type==4?L"住院":L"处方");
                snprintf(buf,128,"%.2f",r->cost/100.0); toWide(buf,cost,32);
                toWide(r->detail,detail,256); toWide(r->create_time,time,64);
                LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
                ListView_InsertItem(s_hLV,&lvi);
                ListView_SetItemText(s_hLV,i,1,type); ListView_SetItemText(s_hLV,i,2,cost);
                ListView_SetItemText(s_hLV,i,3,detail); ListView_SetItemText(s_hLV,i,4,time);
                i++;
            }
            n=n->next;
        }
        CreateWindowW(L"BUTTON",L"关闭",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,190,340,80,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont); return 0;
    }
    case WM_NOTIFY:
        if (((NMHDR*)l)->code == NM_CUSTOMDRAW) return gui_HandleListCustomDraw((LPNMLVCUSTOMDRAW)l);
        return 0;
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){DestroyWindow(hWnd);return 0;}
        return 0;
    case WM_CLOSE: DestroyWindow(hWnd); return 0;
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== 患者自助服务主窗口 ==================== */

LRESULT CALLBACK PatientMainWndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static HWND s_hInfo;
    switch(msg){
    case WM_CREATE:{
        /* 读取传入的患者ID */
        CREATESTRUCT* cs=(CREATESTRUCT*)l;
        if(cs->lpCreateParams) HIS_STRNCPY(g_patientId,(const char*)cs->lpCreateParams,MAX_ID_LEN);

        /* 顶部彩色装饰条 */
        CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE, 0, 0, 520, 5, hWnd, (HMENU)1100, g_hInst, NULL);
        /* 顶部信息栏 */
        s_hInfo=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,10,15,480,30,hWnd,(HMENU)-1,g_hInst,NULL);

        /* 7个功能按钮 + 退出 */
        const wchar_t* btns[]={L"1. 普通挂号",L"2. 预约挂号",L"3. 查看/取消挂号",
            L"4. 医疗记录",L"5. 自助充值",L"6. 预约签到",L"0. 退出"};
        int ids[]={1001,1002,1003,1004,1005,1006,1007};
        for(int i=0;i<7;i++){
            int row=i/2, col=i%2;
            CreateWindowW(L"BUTTON",btns[i],WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
                20+col*240,50+row*55,220,42,
                hWnd,(HMENU)ids[i],g_hInst,NULL);
        }
        gui_SetDialogFont(hWnd,g_hFont);
        UpdateInfo:;
        /* 更新信息 */
        ListNode* n=FindNode(patient_list,g_patientId);
        if(n){
            Patient* p=(Patient*)n->data;
            WCHAR wbuf[256];
            swprintf(wbuf,256,L"患者: %hs(%hs)  余额: %.2f元  医保: %.0f%%  状态: %ls",
                p->name,p->id,p->balance/100.0,p->insurance_ratio*100,
                p->register_status==0?L"未挂号":p->register_status==1?L"待就诊":L"已完成");
            SetWindowTextW(s_hInfo,wbuf);
        }
        return 0;
    }

    case WM_COMMAND:
        switch(LOWORD(w)){
        case 1001: { /* 普通挂号 */
            HWND hF=CreateWindowW(L"RegForm",L"普通挂号",WS_CAPTION|WS_SYSMENU,
                CW_USEDEFAULT,CW_USEDEFAULT,310,440,hWnd,NULL,g_hInst,NULL);
            DoModal(hF,310,440); DestroyWindow(hF);
            goto UpdateInfo;
        }
        case 1002: { /* 预约挂号 */
            HWND hF=CreateWindowW(L"ApptForm",L"预约挂号",WS_CAPTION|WS_SYSMENU,
                CW_USEDEFAULT,CW_USEDEFAULT,290,440,hWnd,NULL,g_hInst,NULL);
            DoModal(hF,290,440); DestroyWindow(hF);
            goto UpdateInfo;
        }
        case 1003: { /* 查看/取消挂号 */
            HWND hF=CreateWindowW(L"ViewRegForm",L"挂号信息",WS_CAPTION|WS_SYSMENU,
                CW_USEDEFAULT,CW_USEDEFAULT,300,210,hWnd,NULL,g_hInst,NULL);
            DoModal(hF,300,210); DestroyWindow(hF);
            goto UpdateInfo;
        }
        case 1004: { /* 医疗记录 */
            /* PIN验证 */
            if(patient_list){
                ListNode* n=FindNode(patient_list,g_patientId);
                if(n&&((Patient*)n->data)->pin[0]){
                    HWND hP=CreateWindowW(L"PinVerify",L"PIN验证",WS_CAPTION|WS_SYSMENU,
                        CW_USEDEFAULT,CW_USEDEFAULT,280,160,hWnd,NULL,g_hInst,NULL);
                    int r=DoModal(hP,280,160); DestroyWindow(hP);
                    if(r!=IDOK) break;
                }
            }
            HWND hR=CreateWindowW(L"ViewRecords",L"医疗记录",WS_CAPTION|WS_SYSMENU,
                CW_USEDEFAULT,CW_USEDEFAULT,500,400,hWnd,NULL,g_hInst,NULL);
            DoModal(hR,500,400); DestroyWindow(hR);
            break;
        }
        case 1005: { /* 自助充值 */
            HWND hR=CreateWindowW(L"RechargeForm",L"自助充值",WS_CAPTION|WS_SYSMENU,
                CW_USEDEFAULT,CW_USEDEFAULT,300,140,hWnd,NULL,g_hInst,NULL);
            DoModal(hR,300,140); DestroyWindow(hR);
            /* 刷新信息 */
            goto UpdateInfo;
        }
        case 1006: { /* 预约签到 */
            HWND hF=CreateWindowW(L"CheckInForm",L"预约签到",WS_CAPTION|WS_SYSMENU,
                CW_USEDEFAULT,CW_USEDEFAULT,360,370,hWnd,NULL,g_hInst,NULL);
            DoModal(hF,360,370); DestroyWindow(hF);
            goto UpdateInfo;
        }
        case 1007: /* 退出 */
        case IDCANCEL:
            DestroyWindow(hWnd); return 0;
        }
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)w;
        int id = GetDlgCtrlID((HWND)l);
        if (id == 1100) {
            static HBRUSH hBr = NULL;
            if (!hBr) hBr = CreateSolidBrush(RGB(0, 140, 125));
            return (LRESULT)hBr;
        }
        SetTextColor(hdc, RGB(0, 110, 100));
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)(HBRUSH)GetStockObject(NULL_BRUSH);
    }
    case WM_CLOSE: DestroyWindow(hWnd); return 0;
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== 患者选择窗口 ==================== */

LRESULT CALLBACK PatientSelectWndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static HWND s_hLV;

    switch (msg) {
    case WM_CREATE: {
        gui_SetDialogFont(hWnd, g_hFont);
        /* 顶部彩色装饰条 */
        CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE, 0, 0, 520, 5, hWnd, (HMENU)1100, g_hInst, NULL);
        CreateWindowW(L"STATIC",L"选择已有患者或创建新账号:",WS_CHILD|WS_VISIBLE,15,15,300,15,hWnd,(HMENU)-1,g_hInst,NULL);
        s_hLV=CreateWindowW(WC_LISTVIEW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP
            |LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,15,32,490,290,
            hWnd,(HMENU)IDC_PATIENT_LIST,g_hInst,NULL);

        CreateWindowW(L"BUTTON",L"选择患者",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP,
            150,340,90,28,hWnd,(HMENU)IDC_BTN_PATIENT_SEL,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"创建新患者",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP,
            260,340,90,28,hWnd,(HMENU)IDC_BTN_PATIENT_NEW,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|WS_TABSTOP,
            370,340,90,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);

        const char* cols[]={"患者ID","姓名","年龄","性别","手机号"};
        int ws[]={80,80,50,50,160};
        ListView_SetExtendedListViewStyle(s_hLV, LVS_EX_GRIDLINES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);
        gui_InitListView(s_hLV,cols,ws,NULL,5);

        int i=0; ListNode* node=patient_list?patient_list->head:NULL;
        while(node){
            Patient* p=(Patient*)node->data; if(!p){node=node->next;continue;}
            WCHAR id[32],name[64],age[16],gender[16],phone[32]; char buf[64];
            toWide(p->id,id,32); toWide(p->name,name,64);
            snprintf(buf,64,"%d",p->age); toWide(buf,age,16);
            toWide(p->gender,gender,16); toWide(p->phone,phone,32);
            LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
            ListView_InsertItem(s_hLV,&lvi);
            ListView_SetItemText(s_hLV,i,1,name); ListView_SetItemText(s_hLV,i,2,age);
            ListView_SetItemText(s_hLV,i,3,gender); ListView_SetItemText(s_hLV,i,4,phone);
            i++; node=node->next;
        }
        if(i==0) EnableWindow(GetDlgItem(hWnd,IDC_BTN_PATIENT_SEL),FALSE);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)w;
        int id = GetDlgCtrlID((HWND)l);
        if (id == 1100) {
            static HBRUSH hBr = NULL;
            if (!hBr) hBr = CreateSolidBrush(RGB(0, 140, 125));
            return (LRESULT)hBr;
        }
        SetTextColor(hdc, RGB(0, 110, 100));
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)(HBRUSH)GetStockObject(NULL_BRUSH);
    }
    case WM_NOTIFY:
        if (((NMHDR*)l)->code == NM_CUSTOMDRAW) return gui_HandleListCustomDraw((LPNMLVCUSTOMDRAW)l);
        return 0;
    case WM_COMMAND:
        switch(LOWORD(w)){
        case IDC_BTN_PATIENT_SEL:{
            int sel=ListView_GetNextItem(s_hLV,-1,LVNI_SELECTED);
            if(sel<0){gui_MsgBox(hWnd,"请先在列表中选择一个患者。","提示",MB_ICONINFORMATION);return 0;}
            WCHAR wpid[64]; ListView_GetItemText(s_hLV,sel,0,wpid,64);
            toMulti(wpid,g_patientId,sizeof(g_patientId));

            /* 检查是否需要PIN验证 */
            Patient* p=NULL;
            ListNode* n=FindNode(patient_list,g_patientId);
            if(n) p=(Patient*)n->data;
            if(p&&p->pin[0]){
                HWND hP=CreateWindowW(L"PinVerify",L"PIN验证",WS_CAPTION|WS_SYSMENU,
                    CW_USEDEFAULT,CW_USEDEFAULT,280,160,hWnd,NULL,g_hInst,NULL);
                int r=DoModal(hP,280,160); DestroyWindow(hP);
                if(r!=IDOK) return 0;
            }
            g_modalResult = IDOK;
            DestroyWindow(hWnd); return 0;
        }
        case IDC_BTN_PATIENT_NEW: {
            HWND hForm = CreateWindowW(L"PatientForm",L"添加患者",WS_CAPTION|WS_SYSMENU,
                CW_USEDEFAULT,CW_USEDEFAULT,280,280,hWnd,NULL,g_hInst,NULL);
            int r = DoModal(hForm,280,280);
            DestroyWindow(hForm);
            if(r==IDOK && g_patientId[0]){
                g_modalResult = IDOK;
                DestroyWindow(hWnd);
                return 0;
            }
            return 0;
        }
        case IDCANCEL:
            g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== 普通挂号对话框 ==================== */

LRESULT CALLBACK RegFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static HWND s_hDeptLV, s_hDocLV;
    static char s_selDeptId[MAX_ID_LEN], s_selDocId[MAX_ID_LEN];

    switch(msg){
    case WM_CREATE:{
        CreateWindowW(L"STATIC",L"选择科室:",WS_CHILD|WS_VISIBLE,12,8,200,16,hWnd,(HMENU)-1,g_hInst,NULL);
        s_hDeptLV=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SINGLESEL,
            12,26,270,160,hWnd,(HMENU)1001,g_hInst,NULL);
        const char* dcols[]={"科室ID","科室名称","医生数"};
        int dw[]={70,120,60};
        gui_InitListView(s_hDeptLV,dcols,dw,NULL,3);

        CreateWindowW(L"STATIC",L"选择医生:",WS_CHILD|WS_VISIBLE,12,195,200,16,hWnd,(HMENU)-1,g_hInst,NULL);
        s_hDocLV=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SINGLESEL,
            12,213,270,140,hWnd,(HMENU)1002,g_hInst,NULL);
        const char* doccols[]={"医生ID","姓名","专长"};
        int docw[]={70,60,120};
        gui_InitListView(s_hDocLV,doccols,docw,NULL,3);

        CreateWindowW(L"BUTTON",L"确认挂号",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,50,370,80,28,
            hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,150,370,70,28,
            hWnd,(HMENU)IDCANCEL,g_hInst,NULL);

        gui_SetDialogFont(hWnd,g_hFont);

        /* 加载科室 */
        int i=0; ListNode* n=dept_list?dept_list->head:NULL;
        while(n){
            Department* d=(Department*)n->data; if(!d){n=n->next;continue;}
            WCHAR id[32],name[64],cnt[16]; char buf[64];
            acpToWide(d->id,id,32); acpToWide(d->name,name,64);
            snprintf(buf,64,"%d",d->doctor_count); acpToWide(buf,cnt,16);
            LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
            ListView_InsertItem(s_hDeptLV,&lvi);
            ListView_SetItemText(s_hDeptLV,i,1,name);
            ListView_SetItemText(s_hDeptLV,i,2,cnt);
            i++; n=n->next;
        }
        return 0;
    }

    case WM_NOTIFY:{
        NMHDR* h=(NMHDR*)l;
        /* ListView 斑马纹 */
        if (h->code == NM_CUSTOMDRAW) return gui_HandleListCustomDraw((LPNMLVCUSTOMDRAW)l);
        if(h->hwndFrom==s_hDeptLV && h->code==LVN_ITEMCHANGED){
            int sel=ListView_GetNextItem(s_hDeptLV,-1,LVNI_SELECTED);
            if(sel<0) return 0;
            WCHAR wid[32]; ListView_GetItemText(s_hDeptLV,sel,0,wid,32);
            WideCharToMultiByte(CP_ACP,0,wid,-1,s_selDeptId,sizeof(s_selDeptId),NULL,NULL);

            /* 更新医生列表 */
            ListView_DeleteAllItems(s_hDocLV); s_selDocId[0]=0;
            int j=0; ListNode* dn=doctor_list?doctor_list->head:NULL;
            while(dn){
                Doctor* d=(Doctor*)dn->data;
                if(d && strcmp(d->dept_id,s_selDeptId)==0){
                    WCHAR id[32],name[64],spec[128];
                    acpToWide(d->id,id,32); acpToWide(d->name,name,64); acpToWide(d->specialty,spec,128);
                    LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=j,.iSubItem=0,.pszText=id};
                    ListView_InsertItem(s_hDocLV,&lvi);
                    ListView_SetItemText(s_hDocLV,j,1,name);
                    ListView_SetItemText(s_hDocLV,j,2,spec);
                    j++;
                }
                dn=dn->next;
            }
            return 0;
        }
        if(h->hwndFrom==s_hDocLV && h->code==LVN_ITEMCHANGED){
            int sel=ListView_GetNextItem(s_hDocLV,-1,LVNI_SELECTED);
            if(sel<0) return 0;
            WCHAR wid[32]; ListView_GetItemText(s_hDocLV,sel,0,wid,32);
            WideCharToMultiByte(CP_ACP,0,wid,-1,s_selDocId,sizeof(s_selDocId),NULL,NULL);
            /* 更新标题显示费用 */
            char title[128]; Patient* p=(Patient*)FindNode(patient_list,g_patientId)->data;
            if(p) {
                snprintf(title,sizeof(title),"普通挂号  费用:%.2f元  余额:%.2f元",
                    REGISTRATION_FEE/100.0,p->balance/100.0);
                WCHAR wtitle[128]; acpToWide(title,wtitle,128);
                SetWindowTextW(hWnd,wtitle);
            }
            return 0;
        }
        return 0;
    }

    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult=IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            if(!s_selDocId[0]){gui_MsgBox(hWnd,"请先选择科室和医生！","提示",MB_ICONINFORMATION);return 0;}
            ListNode* pn=FindNode(patient_list,g_patientId);
            if(!pn){gui_MsgBox(hWnd,"患者不存在！","错误",MB_ICONERROR);return 0;}
            Patient* p=(Patient*)pn->data;
            if(p->register_status!=0){
                gui_MsgBox(hWnd,"您已挂号，请先取消当前挂号再重新挂号。","提示",MB_ICONINFORMATION);return 0;
            }
            if(p->balance<REGISTRATION_FEE){
                gui_MsgBox(hWnd,"余额不足，请先充值！","提示",MB_ICONINFORMATION);return 0;
            }

            /* 扣费 */
            p->balance-=REGISTRATION_FEE;
            p->register_status=REG_STATUS_PENDING;
            HIS_STRNCPY(p->doctor_id,s_selDocId,MAX_ID_LEN);
            HIS_STRNCPY(p->dept_id,s_selDeptId,MAX_ID_LEN);
            HisGetSystemTime(p->register_time);

            /* 创建医疗记录 */
            MedicalRecord r; memset(&r,0,sizeof(r));
            generateUniqueID(r.id,ID_PREFIX_RECORD,record_list);
            HIS_STRNCPY(r.patient_id,g_patientId,MAX_ID_LEN);
            HIS_STRNCPY(r.doctor_id,s_selDocId,MAX_ID_LEN);
            r.type=RECORD_REGISTER;
            r.cost=REGISTRATION_FEE;
            HIS_STRNCPY(r.detail,"普通挂号",MAX_DETAIL_LEN);
            HisGetSystemTime(r.create_time);
            InsertNode(record_list,-1,&r,sizeof(MedicalRecord),r.id);

            savePatientData(); saveRecordData();
            gui_MsgBox(hWnd,"挂号成功！","成功",MB_ICONINFORMATION);
            g_modalResult=IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult=IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== 查看/取消挂号对话框 ==================== */

LRESULT CALLBACK ViewRegFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    switch(msg){
    case WM_CREATE:{
        Patient* p=NULL;
        ListNode* pn=FindNode(patient_list,g_patientId);
        if(pn) p=(Patient*)pn->data;

        char info[512]; int pos=0;
        if(!p || p->register_status==0){
            pos+=snprintf(info+pos,sizeof(info)-pos,"当前未挂号。");
        }else{
            pos+=snprintf(info+pos,sizeof(info)-pos,"就诊状态: %s\n",
                p->register_status==1?"待就诊":p->register_status==3?"已完成":"未知");
            pos+=snprintf(info+pos,sizeof(info)-pos,"挂号医生ID: %s\n",p->doctor_id);
            pos+=snprintf(info+pos,sizeof(info)-pos,"科室ID: %s\n",p->dept_id);
            pos+=snprintf(info+pos,sizeof(info)-pos,"挂号时间: %s\n",p->register_time);
        }
        CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,15,15,250,100,hWnd,(HMENU)-1,g_hInst,NULL);
        WCHAR winfo[512]; acpToWide(info,winfo,512);
        SetWindowTextW(GetDlgItem(hWnd,-1),winfo);

        if(p && p->register_status!=0){
            CreateWindowW(L"BUTTON",L"取消挂号",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,60,130,90,28,
                hWnd,(HMENU)1001,g_hInst,NULL);
        }
        CreateWindowW(L"BUTTON",L"关闭",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,170,130,60,28,
            hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont);
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==1001){ /* 取消挂号 */
            if(!gui_MsgBox(hWnd,"确认取消挂号？取消后费用将退还。","确认",MB_YESNO|MB_ICONQUESTION))
                return 0;
            ListNode* pn=FindNode(patient_list,g_patientId);
            if(!pn){gui_MsgBox(hWnd,"患者不存在！","错误",MB_ICONERROR);return 0;}
            Patient* p=(Patient*)pn->data;
            if(p->register_status==0){gui_MsgBox(hWnd,"当前未挂号，无需取消。","提示",MB_ICONINFORMATION);return 0;}

            /* 按实际挂号费退费（查找最近的挂号记录） */
            long long refund=REGISTRATION_FEE;
            ListNode* rn=record_list?record_list->head:NULL;
            while(rn){
                MedicalRecord* r=(MedicalRecord*)rn->data;
                if(r&&strcmp(r->patient_id,g_patientId)==0&&r->type==RECORD_REGISTER&&!r->cancelled)
                    refund=r->cost;
                rn=rn->next;
            }
            p->balance+=refund;
            /* 标记医疗记录为已取消 */
            rn=record_list?record_list->head:NULL;
            while(rn){
                MedicalRecord* r=(MedicalRecord*)rn->data;
                if(r&&strcmp(r->patient_id,g_patientId)==0&&r->type==RECORD_REGISTER&&!r->cancelled)
                    r->cancelled=1;
                rn=rn->next;
            }
            p->register_status=REG_STATUS_NONE;
            p->doctor_id[0]=0;
            p->dept_id[0]=0;
            /* 同步取消关联的预约记录 */
            ListNode* an=appointment_list?appointment_list->head:NULL;
            while(an){
                Appointment* a=(Appointment*)an->data;
                if(a&&strcmp(a->patient_id,g_patientId)==0&&strcmp(a->status,"已预约")==0)
                    HIS_STRNCPY(a->status,"已取消",20);
                an=an->next;
            }
            savePatientData();saveAppointmentData();saveRecordData();
            gui_MsgBox(hWnd,"挂号已取消，费用已退还。","成功",MB_ICONINFORMATION);
            g_modalResult=IDOK;DestroyWindow(hWnd);return 0;
        }
        if(LOWORD(w)==IDCANCEL){g_modalResult=IDCANCEL;DestroyWindow(hWnd);return 0;}
        return 0;
    case WM_CLOSE:{g_modalResult=IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== 预约挂号对话框 ==================== */

LRESULT CALLBACK ApptFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static HWND s_hDeptLV, s_hDocLV, s_hSchedLV;
    static char s_selDeptId[MAX_ID_LEN], s_selDocId[MAX_ID_LEN], s_selSchedId[MAX_ID_LEN];

    switch(msg){
    case WM_CREATE:{
        CreateWindowW(L"STATIC",L"科室:",WS_CHILD|WS_VISIBLE,10,8,200,15,hWnd,(HMENU)-1,g_hInst,NULL);
        s_hDeptLV=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SINGLESEL,
            10,25,250,100,hWnd,(HMENU)1001,g_hInst,NULL);
        gui_InitListView(s_hDeptLV,((const char*[]){"ID","名称","医生数"}),(int[]){50,130,50},NULL,3);

        CreateWindowW(L"STATIC",L"医生:",WS_CHILD|WS_VISIBLE,10,130,200,15,hWnd,(HMENU)-1,g_hInst,NULL);
        s_hDocLV=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SINGLESEL,
            10,147,250,100,hWnd,(HMENU)1002,g_hInst,NULL);
        gui_InitListView(s_hDocLV,((const char*[]){"ID","姓名","专长"}),(int[]){50,60,120},NULL,3);

        CreateWindowW(L"STATIC",L"可选时段:",WS_CHILD|WS_VISIBLE,10,252,200,15,hWnd,(HMENU)-1,g_hInst,NULL);
        s_hSchedLV=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SINGLESEL,
            10,269,250,80,hWnd,(HMENU)1003,g_hInst,NULL);
        gui_InitListView(s_hSchedLV,((const char*[]){"日期","时段","号源"}),(int[]){80,80,70},NULL,3);

        CreateWindowW(L"BUTTON",L"确认预约",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,50,365,80,28,
            hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,150,365,70,28,
            hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont);

        int i=0; ListNode* n=dept_list?dept_list->head:NULL;
        while(n){Department*d=(Department*)n->data;if(!d){n=n->next;continue;}
            WCHAR id[32],nm[64],cnt[16];char buf[64];
            acpToWide(d->id,id,32);acpToWide(d->name,nm,64);
            snprintf(buf,64,"%d",d->doctor_count);acpToWide(buf,cnt,16);
            LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
            ListView_InsertItem(s_hDeptLV,&lvi);ListView_SetItemText(s_hDeptLV,i,1,nm);ListView_SetItemText(s_hDeptLV,i,2,cnt);
            i++;n=n->next;
        }
        return 0;
    }
    case WM_NOTIFY:{
        NMHDR* h=(NMHDR*)l;
        /* ListView 斑马纹 */
        if (h->code == NM_CUSTOMDRAW) return gui_HandleListCustomDraw((LPNMLVCUSTOMDRAW)l);
        if(h->hwndFrom==s_hDeptLV&&h->code==LVN_ITEMCHANGED){
            int sel=ListView_GetNextItem(s_hDeptLV,-1,LVNI_SELECTED);
            if(sel<0)return 0;WCHAR wbuf[32];
            ListView_GetItemText(s_hDeptLV,sel,0,wbuf,32);
            WideCharToMultiByte(CP_ACP,0,wbuf,-1,s_selDeptId,sizeof(s_selDeptId),NULL,NULL);
            ListView_DeleteAllItems(s_hDocLV);s_selDocId[0]=0;
            ListView_DeleteAllItems(s_hSchedLV);s_selSchedId[0]=0;
            int j=0;ListNode* dn=doctor_list?doctor_list->head:NULL;
            while(dn){Doctor*d=(Doctor*)dn->data;
                if(d&&strcmp(d->dept_id,s_selDeptId)==0){
                    WCHAR id[32],nm[64],sp[128];
                    acpToWide(d->id,id,32);acpToWide(d->name,nm,64);acpToWide(d->specialty,sp,128);
                    LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=j,.iSubItem=0,.pszText=id};
                    ListView_InsertItem(s_hDocLV,&lvi);ListView_SetItemText(s_hDocLV,j,1,nm);ListView_SetItemText(s_hDocLV,j,2,sp);
                    j++;
                }dn=dn->next;
            }
            return 0;
        }
        if(h->hwndFrom==s_hDocLV&&h->code==LVN_ITEMCHANGED){
            int sel=ListView_GetNextItem(s_hDocLV,-1,LVNI_SELECTED);
            if(sel<0)return 0;WCHAR wbuf[32];
            ListView_GetItemText(s_hDocLV,sel,0,wbuf,32);
            WideCharToMultiByte(CP_ACP,0,wbuf,-1,s_selDocId,sizeof(s_selDocId),NULL,NULL);
            ListView_DeleteAllItems(s_hSchedLV);s_selSchedId[0]=0;
            char today[16]={0};{time_t t=time(NULL);struct tm*tm=localtime(&t);strftime(today,16,"%Y-%m-%d",tm);}
            int j=0;ListNode* sn=schedule_list?schedule_list->head:NULL;
            while(sn){DoctorSchedule*s=(DoctorSchedule*)sn->data;
                if(s&&strcmp(s->doctor_id,s_selDocId)==0&&s->is_available
                    &&s->current_patients<s->max_patients&&strcmp(s->date,today)>=0){
                    WCHAR date[32],slot[32],num[32];char buf[64];
                    acpToWide(s->date,date,32);acpToWide(s->time_slot,slot,32);
                    snprintf(buf,64,"%d/%d",s->current_patients,s->max_patients);acpToWide(buf,num,32);
                    LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=j,.iSubItem=0,.pszText=date};
                    ListView_InsertItem(s_hSchedLV,&lvi);ListView_SetItemText(s_hSchedLV,j,1,slot);
                    ListView_SetItemText(s_hSchedLV,j,2,num);
                    j++;
                }sn=sn->next;
            }
            return 0;
        }
        if(h->hwndFrom==s_hSchedLV&&h->code==LVN_ITEMCHANGED){
            int sel=ListView_GetNextItem(s_hSchedLV,-1,LVNI_SELECTED);
            if(sel<0)return 0;WCHAR wbuf[32];
            ListView_GetItemText(s_hSchedLV,sel,0,wbuf,32);
            WideCharToMultiByte(CP_ACP,0,wbuf,-1,s_selSchedId,sizeof(s_selSchedId),NULL,NULL);
            char today[16]={0};{time_t t=time(NULL);struct tm*tm=localtime(&t);strftime(today,16,"%Y-%m-%d",tm);}
            int idx=0;ListNode* sn=schedule_list?schedule_list->head:NULL;
            while(sn&&idx<sel){DoctorSchedule*s=(DoctorSchedule*)sn->data;
                if(s&&strcmp(s->doctor_id,s_selDocId)==0&&s->is_available
                    &&s->current_patients<s->max_patients&&strcmp(s->date,today)>=0)idx++;
                sn=sn->next;
            }
            if(sn&&sel>=0){
                DoctorSchedule*s=(DoctorSchedule*)sn->data;
                HIS_STRNCPY(s_selSchedId,s->id,MAX_ID_LEN);
            }
            return 0;
        }
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult=IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            if(!s_selSchedId[0]){gui_MsgBox(hWnd,"请依次选择科室、医生和时段！","提示",MB_ICONINFORMATION);return 0;}
            ListNode* pn=FindNode(patient_list,g_patientId);
            if(!pn){gui_MsgBox(hWnd,"患者不存在！","错误",MB_ICONERROR);return 0;}
            /* 检查是否已有挂号和预约 */
            Patient* p=(Patient*)pn->data;
            if(p->register_status!=0){gui_MsgBox(hWnd,"您已挂号，请先取消！","提示",MB_ICONINFORMATION);return 0;}
            /* 检查排班可用性 */
            ListNode* sn=FindNode(schedule_list,s_selSchedId);
            if(!sn){gui_MsgBox(hWnd,"排班不存在！","错误",MB_ICONERROR);return 0;}
            DoctorSchedule* s=(DoctorSchedule*)sn->data;
            char today[16]={0};{time_t t=time(NULL);struct tm*tm=localtime(&t);strftime(today,16,"%Y-%m-%d",tm);}
            if(strcmp(s->date,today)<0){gui_MsgBox(hWnd,"该排班已过期，请选择其他日期。","提示",MB_ICONINFORMATION);return 0;}
            if(!s->is_available||s->current_patients>=s->max_patients){
                gui_MsgBox(hWnd,"该时段已满！","提示",MB_ICONINFORMATION);return 0;
            }
            if(p->balance<APPOINTMENT_FEE){gui_MsgBox(hWnd,"余额不足，请先充值！","提示",MB_ICONINFORMATION);return 0;}
            /* 扣费 */
            p->balance-=APPOINTMENT_FEE;
            p->register_status=REG_STATUS_PENDING;
            HIS_STRNCPY(p->doctor_id,s_selDocId,MAX_ID_LEN);
            HIS_STRNCPY(p->dept_id,s_selDeptId,MAX_ID_LEN);
            HisGetSystemTime(p->register_time);
            /* 创建预约记录 */
            Appointment a;memset(&a,0,sizeof(a));
            generateUniqueID(a.id,ID_PREFIX_APPOINTMENT,appointment_list);
            HIS_STRNCPY(a.patient_id,g_patientId,MAX_ID_LEN);
            HIS_STRNCPY(a.schedule_id,s_selSchedId,MAX_ID_LEN);
            HIS_STRNCPY(a.status,"已预约",20);
            HisGetSystemTime(a.create_time);
            a.cost=APPOINTMENT_FEE;
            InsertNode(appointment_list,-1,&a,sizeof(Appointment),a.id);
            /* 更新排班号源 */
            s->current_patients++;
            /* 创建挂号医疗记录 */
            MedicalRecord r;memset(&r,0,sizeof(r));
            generateUniqueID(r.id,ID_PREFIX_RECORD,record_list);
            HIS_STRNCPY(r.patient_id,g_patientId,MAX_ID_LEN);
            HIS_STRNCPY(r.doctor_id,s_selDocId,MAX_ID_LEN);
            r.type=RECORD_REGISTER;r.cost=APPOINTMENT_FEE;
            HIS_STRNCPY(r.detail,"预约挂号",MAX_DETAIL_LEN);
            HisGetSystemTime(r.create_time);
            InsertNode(record_list,-1,&r,sizeof(MedicalRecord),r.id);
            savePatientData();saveAppointmentData();saveRecordData();saveScheduleData();
            gui_MsgBox(hWnd,"预约成功！请在就诊日到医院签到。","成功",MB_ICONINFORMATION);
            g_modalResult=IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult=IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== 预约签到对话框 ==================== */

LRESULT CALLBACK CheckInFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static HWND s_hLV;
    static char s_apptIds[256][MAX_ID_LEN];
    static int s_apptCount;

    switch(msg){
    case WM_CREATE:{
        s_hLV=CreateWindowW(WC_LISTVIEWW,L"",WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SINGLESEL,
            15,15,310,260,hWnd,(HMENU)1001,g_hInst,NULL);
        gui_InitListView(s_hLV,((const char*[]){"预约ID","排班ID","状态","时间"}),(int[]){80,80,60,80},NULL,4);
        CreateWindowW(L"BUTTON",L"签到(转为挂号)",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,60,295,110,28,
            hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"关闭",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,190,295,70,28,
            hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont);

        s_apptCount=0;
        int i=0;ListNode* an=appointment_list?appointment_list->head:NULL;
        while(an){Appointment*a=(Appointment*)an->data;
            if(a&&strcmp(a->patient_id,g_patientId)==0&&strcmp(a->status,"已预约")==0){
                HIS_STRNCPY(s_apptIds[s_apptCount],a->id,MAX_ID_LEN);s_apptCount++;
                WCHAR pid[32],sid[32],st[32],ct[64];
                acpToWide(a->id,pid,32);acpToWide(a->schedule_id,sid,32);
                acpToWide(a->status,st,32);acpToWide(a->create_time,ct,64);
                LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=pid};
                ListView_InsertItem(s_hLV,&lvi);
                ListView_SetItemText(s_hLV,i,1,sid);ListView_SetItemText(s_hLV,i,2,st);ListView_SetItemText(s_hLV,i,3,ct);
                i++;
            }an=an->next;
        }
        if(i==0) EnableWindow(GetDlgItem(hWnd,IDOK),FALSE);
        return 0;
    }
    case WM_NOTIFY:
        if (((NMHDR*)l)->code == NM_CUSTOMDRAW) return gui_HandleListCustomDraw((LPNMLVCUSTOMDRAW)l);
        return 0;
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult=IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            int sel=ListView_GetNextItem(s_hLV,-1,LVNI_SELECTED);
            if(sel<0){gui_MsgBox(hWnd,"请选择要签到的预约。","提示",MB_ICONINFORMATION);return 0;}
            char* apptId=s_apptIds[sel];
            ListNode* an=FindNode(appointment_list,apptId);
            if(!an){gui_MsgBox(hWnd,"预约不存在！","错误",MB_ICONERROR);return 0;}
            Appointment* a=(Appointment*)an->data;
            ListNode* sn=FindNode(schedule_list,a->schedule_id);
            if(!sn){gui_MsgBox(hWnd,"排班不存在！","错误",MB_ICONERROR);return 0;}
            DoctorSchedule* s=(DoctorSchedule*)sn->data;
            ListNode* pn=FindNode(patient_list,g_patientId);
            if(!pn){gui_MsgBox(hWnd,"患者不存在！","错误",MB_ICONERROR);return 0;}
            Patient* p=(Patient*)pn->data;

            /* 检查是否已挂号 */
            if(p->register_status!=0){
                if(!gui_MsgBox(hWnd,"您已有挂号记录，签到将覆盖当前状态。继续？","确认",MB_YESNO|MB_ICONQUESTION))
                    return 0;
            }

            /* 标记预约完成 */
            HIS_STRNCPY(a->status,"已完成",20);

            /* 设置患者就诊状态 */
            p->register_status=REG_STATUS_PENDING;
            HIS_STRNCPY(p->doctor_id,s->doctor_id,MAX_ID_LEN);
            HIS_STRNCPY(p->dept_id,s->dept_id,MAX_ID_LEN);
            HisGetSystemTime(p->register_time);

            /* 创建签到医疗记录 */
            MedicalRecord r;memset(&r,0,sizeof(r));
            generateUniqueID(r.id,ID_PREFIX_RECORD,record_list);
            HIS_STRNCPY(r.patient_id,g_patientId,MAX_ID_LEN);
            HIS_STRNCPY(r.doctor_id,s->doctor_id,MAX_ID_LEN);
            r.type=RECORD_REGISTER;
            r.cost=0;
            HIS_STRNCPY(r.detail,"预约签到",MAX_DETAIL_LEN);
            HisGetSystemTime(r.create_time);
            InsertNode(record_list,-1,&r,sizeof(MedicalRecord),r.id);

            saveAppointmentData();savePatientData();saveRecordData();
            gui_MsgBox(hWnd,"签到成功！您现在可以就诊了。","成功",MB_ICONINFORMATION);
            g_modalResult=IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult=IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== 注册 ==================== */

void gui_RegisterPatientClasses(HINSTANCE hInst) {
    WNDCLASS wc={.style=CS_HREDRAW|CS_VREDRAW,.hInstance=hInst,.hCursor=LoadCursor(NULL,IDC_ARROW),
                  .hbrBackground=(HBRUSH)(COLOR_WINDOW+1),.lpszMenuName=NULL};
    wc.lpfnWndProc=PatientSelectWndProc; wc.lpszClassName=L"HIS_PatientSelectWnd"; RegisterClass(&wc);
    wc.lpfnWndProc=PatientMainWndProc; wc.lpszClassName=L"HIS_PatientMainWnd"; RegisterClass(&wc);
    wc.lpfnWndProc=PinVerifyProc; wc.lpszClassName=L"PinVerify"; RegisterClass(&wc);
    wc.lpfnWndProc=RechargeFormProc; wc.lpszClassName=L"RechargeForm"; RegisterClass(&wc);
    wc.lpfnWndProc=ViewRecordsProc; wc.lpszClassName=L"ViewRecords"; RegisterClass(&wc);
    wc.lpfnWndProc=RegFormProc; wc.lpszClassName=L"RegForm"; RegisterClass(&wc);
    wc.lpfnWndProc=ViewRegFormProc; wc.lpszClassName=L"ViewRegForm"; RegisterClass(&wc);
    wc.lpfnWndProc=ApptFormProc; wc.lpszClassName=L"ApptForm"; RegisterClass(&wc);
    wc.lpfnWndProc=CheckInFormProc; wc.lpszClassName=L"CheckInForm"; RegisterClass(&wc);
}
