#include "his.h"
#include "gui_main.h"
#pragma warning(disable:4312)

/*
 * 医生工作站
 *   TabControl + 4 标签页：我的患者 / 医疗记录 / 预约信息 / 排班与资料
 */

#define TAB_MY_PATIENTS 0
#define TAB_RECORDS     1
#define TAB_APPOINT     2
#define TAB_PROFILE     3

static int s_curTab = TAB_MY_PATIENTS;
static HWND s_hTab, s_hList[4], s_hBtn1, s_hBtn2, s_hBtn3;
static char s_recPatientId[MAX_ID_LEN]; /* 医疗记录页当前查看的患者ID */

static const wchar_t* s_tabTitles[] = {
    L"我的患者", L"医疗记录", L"预约信息", L"排班与资料"
};

/* ==================== 辅助函数 ==================== */

static void toWide(const char* src, WCHAR* dst, int n) {
    MultiByteToWideChar(CP_ACP,0,src,-1,dst,n); dst[n-1]=0;
}
static void toMulti(const WCHAR* src, char* dst, int n) {
    WideCharToMultiByte(CP_ACP,0,src,-1,dst,n,NULL,NULL); dst[n-1]=0;
}

/* 取当前选中行的第一列文本（UTF-8） */
static int GetSelectedId(HWND hLV, char* out, int cap) {
    int sel = ListView_GetNextItem(hLV,-1,LVNI_SELECTED);
    if (sel<0) return 0;
    WCHAR w[64]; ListView_GetItemText(hLV,sel,0,w,64);
    toMulti(w,out,cap); return 1;
}

/* 取列表中指定行列的值（UTF-8） */
static void GetCellText(HWND hLV, int row, int col, char* out, int cap) {
    WCHAR w[256]; ListView_GetItemText(hLV,row,col,w,256); toMulti(w,out,cap);
}

/* ==================== ListView 刷新函数 ==================== */

/* ---- 我的患者 ---- */
static void RefreshMyPatients(HWND hLV, const char* doctorId) {
    ListView_DeleteAllItems(hLV); int i=0;
    ListNode* n = patient_list ? patient_list->head : NULL;
    while (n) {
        Patient* p = (Patient*)n->data;
        if (p && strcmp(p->doctor_id, doctorId)==0) {
            WCHAR id[32],name[64],age[16],gender[16],phone[32],st[16]; char buf[64];
            toWide(p->id,id,32); toWide(p->name,name,64);
            snprintf(buf,64,"%d",p->age); toWide(buf,age,16);
            toWide(p->gender,gender,16); toWide(p->phone,phone,32);
            wcscpy_s(st,16,p->register_status==0?L"未挂号":p->register_status==1?L"待就诊":L"已完成");
            LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
            ListView_InsertItem(hLV,&lvi);
            ListView_SetItemText(hLV,i,1,name); ListView_SetItemText(hLV,i,2,age);
            ListView_SetItemText(hLV,i,3,gender); ListView_SetItemText(hLV,i,4,phone);
            ListView_SetItemText(hLV,i,5,st); i++;
        }
        n=n->next;
    }
}

/* ---- 医疗记录 ---- */
static void RefreshRecords(HWND hLV, const char* patientId) {
    ListView_DeleteAllItems(hLV);
    if (!patientId || !patientId[0]) return;
    int i=0;
    ListNode* n = record_list ? record_list->head : NULL;
    while (n) {
        MedicalRecord* r = (MedicalRecord*)n->data;
        if (r && strcmp(r->patient_id, patientId)==0) {
            WCHAR id[32],type[32],cost[32],detail[256],time[64],cancel[16]; char buf[128];
            toWide(r->id,id,32);
            wcscpy_s(type,32,r->type==1?L"挂号":r->type==2?L"诊断":r->type==3?L"检查":r->type==4?L"住院":L"处方");
            snprintf(buf,128,"%.2f",r->cost/100.0); toWide(buf,cost,32);
            toWide(r->detail,detail,256); toWide(r->create_time,time,64);
            toWide(r->cancelled?"已取消":"正常",cancel,16);
            LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
            ListView_InsertItem(hLV,&lvi);
            ListView_SetItemText(hLV,i,1,type); ListView_SetItemText(hLV,i,2,cost);
            ListView_SetItemText(hLV,i,3,detail); ListView_SetItemText(hLV,i,4,time);
            ListView_SetItemText(hLV,i,5,cancel); i++;
        }
        n=n->next;
    }
}

/* ---- 预约信息 ---- */
static void RefreshAppointments(HWND hLV, const char* doctorId) {
    ListView_DeleteAllItems(hLV); int i=0;
    /* 先收集此医生的排班ID列表 */
    char schedIds[256][MAX_ID_LEN]; int schedCnt=0;
    ListNode* sn = schedule_list ? schedule_list->head : NULL;
    while (sn && schedCnt<256) {
        DoctorSchedule* s = (DoctorSchedule*)sn->data;
        if (s && strcmp(s->doctor_id, doctorId)==0)
            HIS_STRNCPY(schedIds[schedCnt++], s->id, MAX_ID_LEN);
        sn=sn->next;
    }
    /* 遍历预约，匹配排班ID */
    ListNode* an = appointment_list ? appointment_list->head : NULL;
    while (an) {
        Appointment* a = (Appointment*)an->data;
        if (!a) { an=an->next; continue; }
        int match=0;
        for (int j=0;j<schedCnt;j++) { if (strcmp(a->schedule_id, schedIds[j])==0) { match=1; break; } }
        if (!match) { an=an->next; continue; }
        WCHAR pid[32],sid[32],st[32],ct[64];
        toWide(a->patient_id,pid,32); toWide(a->schedule_id,sid,32);
        toWide(a->status,st,32); toWide(a->create_time,ct,64);
        LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=pid};
        ListView_InsertItem(hLV,&lvi);
        ListView_SetItemText(hLV,i,1,sid); ListView_SetItemText(hLV,i,2,st);
        ListView_SetItemText(hLV,i,3,ct); i++;
        an=an->next;
    }
}

/* ---- 排班 ---- */
static void RefreshMySchedule(HWND hLV, const char* doctorId) {
    ListView_DeleteAllItems(hLV); int i=0;
    ListNode* n = schedule_list ? schedule_list->head : NULL;
    while (n) {
        DoctorSchedule* s = (DoctorSchedule*)n->data;
        if (s && strcmp(s->doctor_id, doctorId)==0) {
            WCHAR id[32],date[32],slot[32],num[32],avail[16]; char buf[64];
            toWide(s->id,id,32); toWide(s->date,date,32); toWide(s->time_slot,slot,32);
            snprintf(buf,64,"%d/%d",s->current_patients,s->max_patients); toWide(buf,num,32);
            wcscpy_s(avail,16,s->is_available?L"可预约":L"已满");
            LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
            ListView_InsertItem(hLV,&lvi);
            ListView_SetItemText(hLV,i,1,date); ListView_SetItemText(hLV,i,2,slot);
            ListView_SetItemText(hLV,i,3,num); ListView_SetItemText(hLV,i,4,avail); i++;
        }
        n=n->next;
    }
}

/* ---- 个人信息 ---- */
static void ShowProfile(HWND hStatic) {
    ListNode* n = FindNode(doctor_list, g_current_doctor_id);
    if (!n) { SetWindowTextW(hStatic, L"未找到医生信息"); return; }
    Doctor* d = (Doctor*)n->data;
    char buf[1024];
    int pos=0;
    pos+=snprintf(buf+pos,sizeof(buf)-pos,"医生ID: %s\n",d->id);
    pos+=snprintf(buf+pos,sizeof(buf)-pos,"姓名: %s\n",d->name);
    pos+=snprintf(buf+pos,sizeof(buf)-pos,"科室ID: %s\n",d->dept_id);
    pos+=snprintf(buf+pos,sizeof(buf)-pos,"专长: %s\n",d->specialty);
    pos+=snprintf(buf+pos,sizeof(buf)-pos,"账号: %s\n",d->account);
    pos+=snprintf(buf+pos,sizeof(buf)-pos,"每日限额: %d\n",d->max_register);
    pos+=snprintf(buf+pos,sizeof(buf)-pos,"当前已挂号: %d\n",d->current_register);
    WCHAR wbuf[1024]; toWide(buf,wbuf,1024);
    SetWindowTextW(hStatic, wbuf);
}

/* ==================== 表单对话框 ==================== */

/* ---- 新增诊断/处方记录 ---- */
LRESULT CALLBACK RecordFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static int s_recordType = RECORD_DIAGNOSIS;
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs=(CREATESTRUCT*)l;
        if(cs->lpCreateParams) s_recordType = *(int*)cs->lpCreateParams;
        const wchar_t* title = s_recordType==RECORD_DIAGNOSIS ? L"新增诊断记录" : L"新增处方记录";
        SetWindowTextW(hWnd, (LPWSTR)title);
        const wchar_t* labs[]={L"患者ID",L"费用(分)",L"详情"};
        for(int i=0;i<3;i++){CreateWindowW(L"STATIC",labs[i],WS_CHILD|WS_VISIBLE,15,15+i*35,60,18,hWnd,(HMENU)-1,g_hInst,NULL);}
        CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER,80,13,170,22,hWnd,(HMENU)IDC_EDIT_1,g_hInst,NULL);
        CreateWindowW(L"EDIT",L"0",WS_CHILD|WS_VISIBLE|WS_BORDER,80,48,170,22,hWnd,(HMENU)IDC_EDIT_2,g_hInst,NULL);
        CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL,80,80,170,60,hWnd,(HMENU)IDC_EDIT_3,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"确定",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,50,155,70,28,hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,160,155,70,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont);
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            char pid[MAX_ID_LEN],cost[32],detail[MAX_DETAIL_LEN];
            GetDlgItemTextA(hWnd,IDC_EDIT_1,pid,sizeof(pid));
            GetDlgItemTextA(hWnd,IDC_EDIT_2,cost,sizeof(cost));
            GetDlgItemTextA(hWnd,IDC_EDIT_3,detail,sizeof(detail));
            if(!pid[0]){gui_MsgBox(hWnd,"请输入患者ID！","错误",MB_ICONERROR);return 0;}
            ListNode* pn=FindNode(patient_list,pid);
            if(!pn){gui_MsgBox(hWnd,"患者ID不存在！","错误",MB_ICONERROR);return 0;}
            Patient* p=(Patient*)pn->data;
            if(strcmp(p->doctor_id,g_current_doctor_id)!=0){
                gui_MsgBox(hWnd,"该患者未挂您的号，无法添加记录。","错误",MB_ICONERROR);return 0;
            }
            MedicalRecord r; memset(&r,0,sizeof(r));
            generateUniqueID(r.id,ID_PREFIX_RECORD,record_list);
            HIS_STRNCPY(r.patient_id,pid,MAX_ID_LEN);
            HIS_STRNCPY(r.doctor_id,g_current_doctor_id,MAX_ID_LEN);
            r.type=s_recordType;
            r.cost=atoll(cost);
            HIS_STRNCPY(r.detail,detail,MAX_DETAIL_LEN);
            HisGetSystemTime(r.create_time);
            if(InsertNode(record_list,-1,&r,sizeof(MedicalRecord),r.id)!=0){
                gui_MsgBox(hWnd,"保存失败！","错误",MB_ICONERROR);return 0;
            }
            saveRecordData();
            g_modalResult = IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ---- 修改就诊状态 ---- */
LRESULT CALLBACK StatusFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    switch(msg){
    case WM_CREATE:{
        CreateWindowW(L"STATIC",L"患者ID",WS_CHILD|WS_VISIBLE,15,20,60,18,hWnd,(HMENU)-1,g_hInst,NULL);
        CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER,80,18,160,22,hWnd,(HMENU)IDC_EDIT_1,g_hInst,NULL);
        CreateWindowW(L"STATIC",L"新状态(1=待就诊 3=已完成)",WS_CHILD|WS_VISIBLE,15,55,180,18,hWnd,(HMENU)-1,g_hInst,NULL);
        CreateWindowW(L"EDIT",L"3",WS_CHILD|WS_VISIBLE|WS_BORDER,180,53,40,22,hWnd,(HMENU)IDC_EDIT_2,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"确定",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,50,100,70,28,hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,160,100,70,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont); return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            char pid[MAX_ID_LEN],st[8];
            GetDlgItemTextA(hWnd,IDC_EDIT_1,pid,sizeof(pid));
            GetDlgItemTextA(hWnd,IDC_EDIT_2,st,sizeof(st));
            if(!pid[0]){gui_MsgBox(hWnd,"请输入患者ID！","错误",MB_ICONERROR);return 0;}
            ListNode* pn=FindNode(patient_list,pid);
            if(!pn){gui_MsgBox(hWnd,"患者ID不存在！","错误",MB_ICONERROR);return 0;}
            Patient* p=(Patient*)pn->data;
            int newSt=atoi(st);
            if(newSt!=1&&newSt!=3){gui_MsgBox(hWnd,"状态只能是1(待就诊)或3(已完成)！","错误",MB_ICONERROR);return 0;}
            p->register_status=newSt;
            savePatientData();
            g_modalResult = IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ---- 修改密码 ---- */
LRESULT CALLBACK DoctorPwdFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    switch(msg){
    case WM_CREATE:{
        const wchar_t* labs[]={L"当前密码",L"新密码",L"确认新密码"};
        for(int i=0;i<3;i++){CreateWindowW(L"STATIC",labs[i],WS_CHILD|WS_VISIBLE,20,20+i*35,70,18,hWnd,(HMENU)-1,g_hInst,NULL);
            CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_PASSWORD,95,18+i*35,160,22,hWnd,(HMENU)(IDC_EDIT_1+i),g_hInst,NULL);}
        CreateWindowW(L"BUTTON",L"确定",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,60,120,70,28,hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,160,120,70,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont); return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            char old[32],new1[32],new2[32];
            GetDlgItemTextA(hWnd,IDC_EDIT_1,old,sizeof(old));
            GetDlgItemTextA(hWnd,IDC_EDIT_2,new1,sizeof(new1));
            GetDlgItemTextA(hWnd,IDC_EDIT_3,new2,sizeof(new2));
            if(!old[0]||!new1[0]){gui_MsgBox(hWnd,"密码不能为空！","错误",MB_ICONERROR);return 0;}
            if(strcmp(new1,new2)!=0){gui_MsgBox(hWnd,"两次密码不一致！","错误",MB_ICONERROR);return 0;}
            char obfuscated_old[32]; HIS_STRNCPY(obfuscated_old,old,32); passwordObfuscate(obfuscated_old);
            ListNode* n=FindNode(doctor_list,g_current_doctor_id);
            if(!n){gui_MsgBox(hWnd,"未找到医生信息！","错误",MB_ICONERROR);return 0;}
            Doctor* d=(Doctor*)n->data;
            if(strcmp(obfuscated_old,d->password)!=0){gui_MsgBox(hWnd,"当前密码错误！","错误",MB_ICONERROR);return 0;}
            HIS_STRNCPY(d->password,new1,MAX_PWD_LEN); passwordObfuscate(d->password);
            saveDoctorData();
            gui_MsgBox(hWnd,"密码已修改！","成功",MB_ICONINFORMATION);
            g_modalResult = IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== 按钮处理器 ==================== */

static void OnViewRecords(HWND hParent) {
    char pid[MAX_ID_LEN];
    if(!gui_InputDialog(hParent,"查看医疗记录","请输入患者ID:",pid,MAX_ID_LEN)) return;
    HIS_STRNCPY(s_recPatientId,pid,MAX_ID_LEN);
    RefreshRecords(s_hList[TAB_RECORDS],s_recPatientId);
    TabCtrl_SetCurSel(s_hTab,TAB_RECORDS);
    ShowWindow(s_hList[s_curTab],SW_HIDE); s_curTab=TAB_RECORDS;
    ShowWindow(s_hList[TAB_RECORDS],SW_SHOW);
}

static void OnAddRecord(HWND hParent, int recordType) {
    HWND hF=CreateWindowW(L"RecordForm",L"新增记录",WS_CAPTION|WS_SYSMENU,
        CW_USEDEFAULT,CW_USEDEFAULT,280,230,hParent,NULL,g_hInst,(LPVOID)&recordType);
    DoModal(hF,280,230); DestroyWindow(hF);
    if(s_recPatientId[0]) RefreshRecords(s_hList[TAB_RECORDS],s_recPatientId);
}

static void OnChangeStatus(HWND hParent) {
    HWND hF=CreateWindowW(L"StatusForm",L"修改就诊状态",WS_CAPTION|WS_SYSMENU,
        CW_USEDEFAULT,CW_USEDEFAULT,280,180,hParent,NULL,g_hInst,NULL);
    DoModal(hF,280,180); DestroyWindow(hF);
    RefreshMyPatients(s_hList[TAB_MY_PATIENTS],g_current_doctor_id);
}

/* ==================== 医生窗口过程 ==================== */

LRESULT CALLBACK DoctorWndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    switch(msg){
    case WM_CREATE:{
        /* TabControl */
        s_hTab=CreateWindowW(WC_TABCONTROL,NULL,WS_CHILD|WS_VISIBLE|TCS_MULTILINE,0,0,700,460,hWnd,(HMENU)IDC_TAB_ADMIN,g_hInst,NULL);
        TCITEMW tc={.mask=TCIF_TEXT};
        for(int i=0;i<4;i++){tc.pszText=(LPWSTR)s_tabTitles[i];TabCtrl_InsertItem(s_hTab,i,&tc);}

        DWORD lvStyle=WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS;
        /* Tab0: 我的患者 */
        const char* pcols[]={"患者ID","姓名","年龄","性别","手机号","状态"};
        int pw[]={70,60,40,40,90,60};
        s_hList[0]=CreateWindowW(WC_LISTVIEW,L"",lvStyle,10,30,680,370,hWnd,(HMENU)1001,g_hInst,NULL);
        gui_InitListView(s_hList[0],pcols,pw,NULL,6);

        /* Tab1: 医疗记录 */
        const char* rcols[]={"记录ID","类型","费用(元)","详情","创建时间","状态"};
        int rw[]={70,50,70,200,100,60};
        s_hList[1]=CreateWindowW(WC_LISTVIEW,L"",lvStyle,10,30,680,340,hWnd,(HMENU)1002,g_hInst,NULL);
        gui_InitListView(s_hList[1],rcols,rw,NULL,6);
        s_hBtn1=CreateWindowW(L"BUTTON",L"查看患者记录",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,10,380,100,28,hWnd,(HMENU)2001,g_hInst,NULL);
        s_hBtn2=CreateWindowW(L"BUTTON",L"新增诊断",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,115,380,80,28,hWnd,(HMENU)2002,g_hInst,NULL);
        s_hBtn3=CreateWindowW(L"BUTTON",L"新增处方",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,200,380,80,28,hWnd,(HMENU)2003,g_hInst,NULL);

        /* Tab2: 预约信息 */
        const char* acols[]={"患者ID","排班ID","状态","创建时间"};
        int aw[]={80,80,60,120};
        s_hList[2]=CreateWindowW(WC_LISTVIEW,L"",lvStyle,10,30,680,370,hWnd,(HMENU)1003,g_hInst,NULL);
        gui_InitListView(s_hList[2],acols,aw,NULL,4);

        /* Tab3: 排班与资料 */
        const char* scols[]={"排班ID","日期","时间段","号源","状态"};
        int sw[]={70,80,80,80,60};
        s_hList[3]=CreateWindowW(WC_LISTVIEW,L"",lvStyle|WS_CLIPSIBLINGS,10,160,680,240,hWnd,(HMENU)1004,g_hInst,NULL);
        gui_InitListView(s_hList[3],scols,sw,NULL,5);

        gui_SetDialogFont(hWnd,g_hFont);

        /* ListView 扩展样式 */
        DWORD lvEx = LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER;
        for(int i=0;i<4;i++) ListView_SetExtendedListViewStyle(s_hList[i], lvEx);

        /* 初始数据加载 */
        RefreshMyPatients(s_hList[0],g_current_doctor_id);
        RefreshAppointments(s_hList[2],g_current_doctor_id);
        RefreshMySchedule(s_hList[3],g_current_doctor_id);

        for(int i=1;i<4;i++) ShowWindow(s_hList[i],SW_HIDE);
        return 0;
    }

    case WM_SIZE:{
        int ww=LOWORD(l),wh=HIWORD(l);
        if(!s_hTab) break;
        SetWindowPos(s_hTab,NULL,0,0,ww,wh-30,SWP_NOZORDER);
        for(int i=0;i<4;i++) SetWindowPos(s_hList[i],NULL,10,30,ww-20,wh-100,SWP_NOZORDER);
        SetWindowPos(s_hBtn1,NULL,10,wh-65,100,28,SWP_NOZORDER);
        SetWindowPos(s_hBtn2,NULL,115,wh-65,80,28,SWP_NOZORDER);
        SetWindowPos(s_hBtn3,NULL,200,wh-65,80,28,SWP_NOZORDER);
        return 0;
    }

    case WM_NOTIFY:{
        NMHDR* h=(NMHDR*)l;
        /* ListView 斑马纹 */
        if (h->code == NM_CUSTOMDRAW) return gui_HandleListCustomDraw((LPNMLVCUSTOMDRAW)l);
        /* 双击患者自动跳转到医疗记录 */
        if(h->idFrom==1001&&h->code==NM_DBLCLK){
            char pid[MAX_ID_LEN];
            if(GetSelectedId(s_hList[0],pid,sizeof(pid))){
                HIS_STRNCPY(s_recPatientId,pid,MAX_ID_LEN);
                RefreshRecords(s_hList[TAB_RECORDS],s_recPatientId);
                TabCtrl_SetCurSel(s_hTab,TAB_RECORDS);
                ShowWindow(s_hList[s_curTab],SW_HIDE); s_curTab=TAB_RECORDS;
                ShowWindow(s_hList[TAB_RECORDS],SW_SHOW);
                ShowWindow(s_hBtn1,SW_SHOW);ShowWindow(s_hBtn2,SW_SHOW);ShowWindow(s_hBtn3,SW_SHOW);
            }
            return 0;
        }
        if(h->idFrom==IDC_TAB_ADMIN&&h->code==TCN_SELCHANGE){
            int sel=TabCtrl_GetCurSel(s_hTab);
            if(sel!=s_curTab){
                ShowWindow(s_hList[s_curTab],SW_HIDE); s_curTab=sel;
                ShowWindow(s_hList[s_curTab],SW_SHOW);
                if(s_curTab==TAB_MY_PATIENTS) RefreshMyPatients(s_hList[0],g_current_doctor_id);
                if(s_curTab==TAB_APPOINT) RefreshAppointments(s_hList[2],g_current_doctor_id);
                if(s_curTab==TAB_PROFILE){RefreshMySchedule(s_hList[3],g_current_doctor_id);}
                /* 医疗记录tab进入时自动刷新 */
                if(s_curTab==TAB_RECORDS&&s_recPatientId[0]) RefreshRecords(s_hList[1],s_recPatientId);
                /* Tab3 上方的个人资料区用静态文本 */
            }
            /* 按钮可见性 */
            ShowWindow(s_hBtn1,s_curTab==TAB_RECORDS?SW_SHOW:SW_HIDE);
            ShowWindow(s_hBtn2,s_curTab==TAB_RECORDS?SW_SHOW:SW_HIDE);
            ShowWindow(s_hBtn3,s_curTab==TAB_RECORDS?SW_SHOW:SW_HIDE);
        }
        return 0;
    }

    case WM_COMMAND:
        switch(LOWORD(w)){
        case 2001: OnViewRecords(hWnd); return 0;  /* 查看患者记录 */
        case 2002: OnAddRecord(hWnd,RECORD_DIAGNOSIS); return 0; /* 新增诊断 */
        case 2003: OnAddRecord(hWnd,RECORD_PRESCR); return 0;    /* 新增处方 */
        case IDC_BTN_ADD:{
            /* 修改就诊状态（在Tab0也提供一个入口） */
            OnChangeStatus(hWnd); return 0;
        }
        case IDC_BTN_MODIFY:{
            /* 修改密码（在Tab3提供入口） */
            HWND hF=CreateWindowW(L"DoctorPwdForm",L"修改密码",WS_CAPTION|WS_SYSMENU,
                CW_USEDEFAULT,CW_USEDEFAULT,280,190,hWnd,NULL,g_hInst,NULL);
            DoModal(hF,280,190); DestroyWindow(hF);
            return 0;
        }
        }
        return 0;

    case WM_REFRESH_DATA:
        if(s_curTab==TAB_MY_PATIENTS) RefreshMyPatients(s_hList[0],g_current_doctor_id);
        if(s_curTab==TAB_RECORDS&&s_recPatientId[0]) RefreshRecords(s_hList[1],s_recPatientId);
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)l;
        mmi->ptMinTrackSize.x = 720;
        mmi->ptMinTrackSize.y = 520;
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)w;
        SetTextColor(hdc, RGB(30, 100, 70));
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)(HBRUSH)GetStockObject(NULL_BRUSH);
    }
    case WM_CLOSE: DestroyWindow(hWnd); return 0;
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== 注册 ==================== */

void gui_RegisterDoctorClasses(HINSTANCE hInst) {
    WNDCLASS wc={.style=CS_HREDRAW|CS_VREDRAW,.hInstance=hInst,.hCursor=LoadCursor(NULL,IDC_ARROW),
                  .hbrBackground=(HBRUSH)(COLOR_WINDOW+1),.lpszMenuName=NULL};
    wc.lpfnWndProc=DoctorWndProc; wc.lpszClassName=L"HIS_DoctorWnd"; RegisterClass(&wc);
    wc.lpfnWndProc=RecordFormProc; wc.lpszClassName=L"RecordForm"; RegisterClass(&wc);
    wc.lpfnWndProc=StatusFormProc; wc.lpszClassName=L"StatusForm"; RegisterClass(&wc);
    wc.lpfnWndProc=DoctorPwdFormProc; wc.lpszClassName=L"DoctorPwdForm"; RegisterClass(&wc);
}
