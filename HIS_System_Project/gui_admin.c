#include "his.h"
#include "gui_main.h"

/* x64: HMENU 是 64 位，控件 ID 是 int，强制转换安全 */
#pragma warning(disable:4312)

/*
 * 管理员主界面
 *   TabControl 切换 6 个标签页，每个标签页一个 ListView
 *   CRUD 按钮根据当前标签页分发到对应处理函数
 */

#define TAB_PATIENT  0
#define TAB_DEPT     1
#define TAB_DOCTOR   2
#define TAB_BED      3
#define TAB_DRUG     4
#define TAB_SCHEDULE 5

static int s_curTab = TAB_PATIENT;
static HWND s_hTab, s_hList[6], s_hBtnAdd, s_hBtnMod, s_hBtnDel, s_hBtnQry, s_hBtnRef;

/* ---- 标签页标题 ---- */
static const wchar_t* s_tabTitles[] = {
    L"患者管理", L"科室管理", L"医生管理",
    L"床位管理", L"药品管理", L"排班管理"
};

/* ---- ListView 列定义 ---- */
static const char* s_colPatient[] = { "ID","姓名","年龄","性别","手机号","身份证","余额(元)","医保比","状态" };
static int s_widPatient[] = { 70,60,40,40,90,120,70,50,60 };
static int s_fmtPatient[] = { LVCFMT_LEFT,LVCFMT_LEFT,LVCFMT_CENTER,LVCFMT_CENTER,LVCFMT_LEFT,LVCFMT_LEFT,LVCFMT_RIGHT,LVCFMT_CENTER,LVCFMT_CENTER };

static const char* s_colDept[] = { "科室ID","科室名称","医生数" };
static int s_widDept[] = { 80,150,80 };

static const char* s_colDoctor[] = { "ID","姓名","科室","专长","账号","每日限额" };
static int s_widDoctor[] = { 70,60,80,180,80,70 };

static const char* s_colBed[] = { "ID","病房类型","所属科室","状态","患者ID","入院时间" };
static int s_widBed[] = { 60,80,80,60,70,120 };

static const char* s_colDrug[] = { "ID","通用名","商品名","单价","库存","预警阈值","科室" };
static int s_widDrug[] = { 60,80,80,60,50,60,80 };

static const char* s_colSchedule[] = { "排班ID","医生ID","科室ID","日期","时间段","号源","可用" };
static int s_widSchedule[] = { 70,70,70,80,80,60,50 };

/* ==================== ListView 刷新函数 ==================== */

static void RefreshPatientList(HWND hLV) {
    ListView_DeleteAllItems(hLV);
    int i = 0;
    ListNode* node = patient_list ? patient_list->head : NULL;
    while (node) {
        Patient* p = (Patient*)node->data; if (!p) { node = node->next; continue; }
        WCHAR id[32], name[64], age[16], gender[16], phone[32], idcard[32];
        WCHAR bal[32], ins[16], status[16];
        char buf[64];
        MultiByteToWideChar(CP_ACP,0,p->id,-1,id,32);
        MultiByteToWideChar(CP_ACP,0,p->name,-1,name,64);
        snprintf(buf,sizeof(buf),"%d",p->age); MultiByteToWideChar(CP_ACP,0,buf,-1,age,16);
        MultiByteToWideChar(CP_ACP,0,p->gender,-1,gender,16);
        MultiByteToWideChar(CP_ACP,0,p->phone,-1,phone,32);
        MultiByteToWideChar(CP_ACP,0,p->id_card,-1,idcard,32);
        snprintf(buf,sizeof(buf),"%.2f",p->balance/100.0); MultiByteToWideChar(CP_ACP,0,buf,-1,bal,32);
        snprintf(buf,sizeof(buf),"%.0f%%",p->insurance_ratio*100); MultiByteToWideChar(CP_ACP,0,buf,-1,ins,16);
        {WCHAR*tmp=p->is_inpatient?L"住院":L"未住院";wcscpy_s(status,16,tmp);}

        LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
        ListView_InsertItem(hLV,&lvi);
        ListView_SetItemText(hLV,i,1,name); ListView_SetItemText(hLV,i,2,age);
        ListView_SetItemText(hLV,i,3,gender); ListView_SetItemText(hLV,i,4,phone);
        ListView_SetItemText(hLV,i,5,idcard); ListView_SetItemText(hLV,i,6,bal);
        ListView_SetItemText(hLV,i,7,ins); ListView_SetItemText(hLV,i,8,status);
        i++; node = node->next;
    }
}

static void RefreshDeptList(HWND hLV) {
    ListView_DeleteAllItems(hLV); int i=0;
    ListNode* node = dept_list ? dept_list->head : NULL;
    while (node) {
        Department* d = (Department*)node->data; if (!d) { node=node->next; continue; }
        WCHAR id[32], name[64], cnt[16]; char buf[64];
        MultiByteToWideChar(CP_ACP,0,d->id,-1,id,32);
        MultiByteToWideChar(CP_ACP,0,d->name,-1,name,64);
        snprintf(buf,sizeof(buf),"%d",d->doctor_count); MultiByteToWideChar(CP_ACP,0,buf,-1,cnt,16);
        LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
        ListView_InsertItem(hLV,&lvi);
        ListView_SetItemText(hLV,i,1,name); ListView_SetItemText(hLV,i,2,cnt);
        i++; node=node->next;
    }
}

static void RefreshDoctorList(HWND hLV) {
    ListView_DeleteAllItems(hLV); int i=0;
    ListNode* node = doctor_list ? doctor_list->head : NULL;
    while (node) {
        Doctor* d = (Doctor*)node->data; if (!d) { node=node->next; continue; }
        WCHAR id[32],name[64],dept[32],spec[128],acct[64],reg[16]; char buf[64];
        MultiByteToWideChar(CP_ACP,0,d->id,-1,id,32);
        MultiByteToWideChar(CP_ACP,0,d->name,-1,name,64);
        MultiByteToWideChar(CP_ACP,0,d->dept_id,-1,dept,32);
        MultiByteToWideChar(CP_ACP,0,d->specialty,-1,spec,128);
        MultiByteToWideChar(CP_ACP,0,d->account,-1,acct,64);
        snprintf(buf,sizeof(buf),"%d",d->max_register); MultiByteToWideChar(CP_ACP,0,buf,-1,reg,16);
        LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
        ListView_InsertItem(hLV,&lvi);
        ListView_SetItemText(hLV,i,1,name); ListView_SetItemText(hLV,i,2,dept);
        ListView_SetItemText(hLV,i,3,spec); ListView_SetItemText(hLV,i,4,acct);
        ListView_SetItemText(hLV,i,5,reg);
        i++; node=node->next;
    }
}

static void RefreshBedList(HWND hLV) {
    ListView_DeleteAllItems(hLV); int i=0;
    ListNode* node = bed_list ? bed_list->head : NULL;
    while (node) {
        Bed* b = (Bed*)node->data; if (!b) { node=node->next; continue; }
        WCHAR id[32],rtype[32],dept[32],st[16],pid[32],atim[64];
        MultiByteToWideChar(CP_ACP,0,b->id,-1,id,32);
        wcscpy_s(rtype,32,b->room_type==1?L"普通":b->room_type==2?L"半私密":L"VIP");
        MultiByteToWideChar(CP_ACP,0,b->dept_id,-1,dept,32);
        wcscpy_s(st,16,b->status==BED_FREE?L"空闲":L"占用");
        MultiByteToWideChar(CP_ACP,0,b->patient_id,-1,pid,32);
        MultiByteToWideChar(CP_ACP,0,b->admit_time,-1,atim,64);
        LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
        ListView_InsertItem(hLV,&lvi);
        ListView_SetItemText(hLV,i,1,rtype); ListView_SetItemText(hLV,i,2,dept);
        ListView_SetItemText(hLV,i,3,st); ListView_SetItemText(hLV,i,4,pid);
        ListView_SetItemText(hLV,i,5,atim);
        i++; node=node->next;
    }
}

static void RefreshDrugList(HWND hLV) {
    ListView_DeleteAllItems(hLV); int i=0;
    ListNode* node = drug_list ? drug_list->head : NULL;
    while (node) {
        Drug* d = (Drug*)node->data; if (!d) { node=node->next; continue; }
        WCHAR id[32],gname[64],tname[64],price[32],stock[16],warn[16],dept[32]; char buf[64];
        MultiByteToWideChar(CP_ACP,0,d->id,-1,id,32);
        MultiByteToWideChar(CP_ACP,0,d->general_name,-1,gname,64);
        MultiByteToWideChar(CP_ACP,0,d->trade_name,-1,tname,64);
        snprintf(buf,sizeof(buf),"%.2f",d->price); MultiByteToWideChar(CP_ACP,0,buf,-1,price,32);
        snprintf(buf,sizeof(buf),"%d",d->stock); MultiByteToWideChar(CP_ACP,0,buf,-1,stock,16);
        snprintf(buf,sizeof(buf),"%d",d->warning_threshold); MultiByteToWideChar(CP_ACP,0,buf,-1,warn,16);
        MultiByteToWideChar(CP_ACP,0,d->dept_id,-1,dept,32);
        LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
        ListView_InsertItem(hLV,&lvi);
        ListView_SetItemText(hLV,i,1,gname); ListView_SetItemText(hLV,i,2,tname);
        ListView_SetItemText(hLV,i,3,price); ListView_SetItemText(hLV,i,4,stock);
        ListView_SetItemText(hLV,i,5,warn); ListView_SetItemText(hLV,i,6,dept);
        i++; node=node->next;
    }
}

static void RefreshScheduleList(HWND hLV) {
    ListView_DeleteAllItems(hLV); int i=0;
    ListNode* node = schedule_list ? schedule_list->head : NULL;
    while (node) {
        DoctorSchedule* s = (DoctorSchedule*)node->data; if (!s) { node=node->next; continue; }
        WCHAR id[32],did[32],dept[32],date[32],slot[32],num[32],avail[16]; char buf[64];
        MultiByteToWideChar(CP_ACP,0,s->id,-1,id,32);
        MultiByteToWideChar(CP_ACP,0,s->doctor_id,-1,did,32);
        MultiByteToWideChar(CP_ACP,0,s->dept_id,-1,dept,32);
        MultiByteToWideChar(CP_ACP,0,s->date,-1,date,32);
        MultiByteToWideChar(CP_ACP,0,s->time_slot,-1,slot,32);
        snprintf(buf,sizeof(buf),"%d/%d",s->current_patients,s->max_patients);
        MultiByteToWideChar(CP_ACP,0,buf,-1,num,32);
        wcscpy_s(avail,16,s->is_available?L"是":L"否");
        LV_ITEMW lvi={.mask=LVIF_TEXT,.iItem=i,.iSubItem=0,.pszText=id};
        ListView_InsertItem(hLV,&lvi);
        ListView_SetItemText(hLV,i,1,did); ListView_SetItemText(hLV,i,2,dept);
        ListView_SetItemText(hLV,i,3,date); ListView_SetItemText(hLV,i,4,slot);
        ListView_SetItemText(hLV,i,5,num); ListView_SetItemText(hLV,i,6,avail);
        i++; node=node->next;
    }
}

static void (*s_refreshFuncs[])(HWND) = {
    RefreshPatientList, RefreshDeptList, RefreshDoctorList,
    RefreshBedList, RefreshDrugList, RefreshScheduleList
};

/* ==================== 表单窗口过程 ==================== */

/* ---- 患者表单 (添加/修改) ---- */
LRESULT CALLBACK PatientFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static Patient s_editBuf; /* 编辑缓冲 */

    switch (msg) {
    case WM_CREATE: {
        /* 判断模式：CREATESTRUCT.lpCreateParams = NULL 表示添加，非空表示修改 */
        CREATESTRUCT* cs = (CREATESTRUCT*)l;
        int isModify = (cs->lpCreateParams != NULL);
        if (isModify) s_editBuf = *(Patient*)cs->lpCreateParams;

        const wchar_t* labels[] = { L"姓名",L"年龄",L"性别",L"手机号",L"身份证",L"医保比(0~1)",L"余额(分)" };
        for (int i = 0; i < 7; i++) {
            CreateWindowW(L"STATIC",labels[i],WS_CHILD|WS_VISIBLE,20,12+i*28,70,18,
                          hWnd,(HMENU)-1,g_hInst,NULL);
            CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
                          95,10+i*28,150,22,hWnd,(HMENU)(IDC_EDIT_1+i),g_hInst,NULL);
        }
        CreateWindowW(L"BUTTON",isModify?L"保存修改":L"添加",
                      WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,60,215,70,28,
                      hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
                      160,215,70,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont);

        if (isModify) {
            char buf[256];
            snprintf(buf,sizeof(buf),"%s",s_editBuf.name);
            SetDlgItemTextA(hWnd,IDC_EDIT_1,buf);
            snprintf(buf,sizeof(buf),"%d",s_editBuf.age);
            SetDlgItemTextA(hWnd,IDC_EDIT_2,buf);
            SetDlgItemTextA(hWnd,IDC_EDIT_3,s_editBuf.gender);
            SetDlgItemTextA(hWnd,IDC_EDIT_4,s_editBuf.phone);
            SetDlgItemTextA(hWnd,IDC_EDIT_5,s_editBuf.id_card);
            snprintf(buf,sizeof(buf),"%.2f",s_editBuf.insurance_ratio);
            SetDlgItemTextA(hWnd,IDC_EDIT_6,buf);
            snprintf(buf,sizeof(buf),"%lld",s_editBuf.balance);
            SetDlgItemTextA(hWnd,IDC_EDIT_7,buf);
        }
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w)==IDCANCEL) {
            g_modalResult = IDCANCEL;
            DestroyWindow(hWnd); return 0;
        }
        if (LOWORD(w)==IDOK) {
            Patient p; memset(&p,0,sizeof(p));
            if (!gui_InputName(hWnd,IDC_EDIT_1,p.name,sizeof(p.name))) return 0;
            if (!gui_InputAge(hWnd,IDC_EDIT_2,&p.age)) return 0;
            if (!gui_InputGender(hWnd,IDC_EDIT_3,p.gender,sizeof(p.gender))) return 0;
            if (!gui_InputPhone(hWnd,IDC_EDIT_4,p.phone,sizeof(p.phone),
                                s_editBuf.id[0]?s_editBuf.id:NULL)) return 0;
            if (!gui_InputIDCard(hWnd,IDC_EDIT_5,p.id_card,sizeof(p.id_card),
                                 s_editBuf.id[0]?s_editBuf.id:NULL,p.age)) return 0;
            if (!gui_InputInsuranceRatio(hWnd,IDC_EDIT_6,&p.insurance_ratio)) return 0;
            if (!gui_InputBalance(hWnd,IDC_EDIT_7,&p.balance)) return 0;

            /* 是修改还是新增 */
            if (s_editBuf.id[0]) {
                /* 修改：保留原 ID 和其他字段 */
                HIS_STRNCPY(p.id,s_editBuf.id,MAX_ID_LEN);
                p.is_inpatient = s_editBuf.is_inpatient;
                HIS_STRNCPY(p.bed_id,s_editBuf.bed_id,MAX_ID_LEN);
                p.record_count = s_editBuf.record_count;
                HIS_STRNCPY(p.doctor_id,s_editBuf.doctor_id,MAX_ID_LEN);
                HIS_STRNCPY(p.dept_id,s_editBuf.dept_id,MAX_ID_LEN);
                p.register_status = s_editBuf.register_status;
                HIS_STRNCPY(p.register_time,s_editBuf.register_time,MAX_TIME_LEN);
                HIS_STRNCPY(p.pin,s_editBuf.pin,sizeof(p.pin));
                HIS_STRNCPY(p.register_record_id,s_editBuf.register_record_id,MAX_ID_LEN);

                ListNode* old = FindNode(patient_list,s_editBuf.id);
                if (old) memcpy(old->data,&p,sizeof(Patient));
                savePatientData();
                gui_MsgBox(hWnd,"患者信息已修改！","成功",MB_ICONINFORMATION);
            } else {
                if (generateUniqueID(p.id,ID_PREFIX_PATIENT,patient_list)!=0) {
                    gui_MsgBox(hWnd,"无法生成唯一ID！","错误",MB_ICONERROR); return 0;
                }
                p.is_inpatient = PATIENT_OUT;
                InsertNode(patient_list,-1,&p,sizeof(Patient),p.id);
                savePatientData();
                HIS_STRNCPY(g_patientId,p.id,MAX_ID_LEN);
                gui_MsgBox(hWnd,"患者已添加成功！","成功",MB_ICONINFORMATION);
            }
            g_modalResult = IDOK;
            DestroyWindow(hWnd); return 0;
        }
        return 0;
    case WM_CLOSE:
        g_modalResult = IDCANCEL;
        DestroyWindow(hWnd); return 0;
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ---- 科室表单 ---- */
LRESULT CALLBACK DeptFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static Department s_editBuf;
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs=(CREATESTRUCT*)l;
        int isModify=(cs->lpCreateParams!=NULL);
        if(isModify) s_editBuf=*(Department*)cs->lpCreateParams;
        CreateWindowW(L"STATIC",L"科室名称",WS_CHILD|WS_VISIBLE,20,20,70,18,hWnd,(HMENU)-1,g_hInst,NULL);
        CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,100,18,150,22,hWnd,(HMENU)IDC_EDIT_1,g_hInst,NULL);
        CreateWindowW(L"BUTTON",isModify?L"保存修改":L"添加",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,60,70,70,28,hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,160,70,70,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont);
        if(isModify) SetDlgItemTextA(hWnd,IDC_EDIT_1,s_editBuf.name);
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            char name[64];
            if(!GetDlgItemTextA(hWnd,IDC_EDIT_1,name,sizeof(name))||!name[0]){
                gui_MsgBox(hWnd,"科室名称不能为空！","错误",MB_ICONERROR);return 0;
            }
            if(s_editBuf.id[0]){
                Department* d=(Department*)FindNode(dept_list,s_editBuf.id)->data;
                if(d){HIS_STRNCPY(d->name,name,MAX_NAME_LEN);saveDeptData();}
            }else{
                Department d;memset(&d,0,sizeof(d));
                HIS_STRNCPY(d.name,name,MAX_NAME_LEN);
                if(generateUniqueID(d.id,ID_PREFIX_DEPT,dept_list)!=0){
                    gui_MsgBox(hWnd,"无法生成唯一ID！","错误",MB_ICONERROR);return 0;
                }
                InsertNode(dept_list,-1,&d,sizeof(Department),d.id);saveDeptData();
            }
            g_modalResult = IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ---- 医生表单 ---- */
LRESULT CALLBACK DoctorFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static Doctor s_editBuf;
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs=(CREATESTRUCT*)l;
        int isModify=(cs->lpCreateParams!=NULL);
        if(isModify) s_editBuf=*(Doctor*)cs->lpCreateParams;
        const wchar_t* labs[]={L"姓名",L"科室ID",L"专长",L"账号",L"密码",L"每日限额"};
        for(int i=0;i<6;i++){
            CreateWindowW(L"STATIC",labs[i],WS_CHILD|WS_VISIBLE,20,12+i*28,60,18,hWnd,(HMENU)-1,g_hInst,NULL);
            CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,85,10+i*28,160,22,hWnd,(HMENU)(IDC_EDIT_1+i),g_hInst,NULL);
        }
        CreateWindowW(L"BUTTON",isModify?L"保存修改":L"添加",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,60,185,70,28,hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,160,185,70,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont);
        if(isModify){
            char buf[256];
            SetDlgItemTextA(hWnd,IDC_EDIT_1,s_editBuf.name);
            SetDlgItemTextA(hWnd,IDC_EDIT_2,s_editBuf.dept_id);
            SetDlgItemTextA(hWnd,IDC_EDIT_3,s_editBuf.specialty);
            SetDlgItemTextA(hWnd,IDC_EDIT_4,s_editBuf.account);
            SetDlgItemTextA(hWnd,IDC_EDIT_5,""); /* 密码不回显 */
            snprintf(buf,sizeof(buf),"%d",s_editBuf.max_register);
            SetDlgItemTextA(hWnd,IDC_EDIT_6,buf);
        }
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            char name[64],dept[32],spec[128],acct[64],pwd[64],reg[16];
            GetDlgItemTextA(hWnd,IDC_EDIT_1,name,sizeof(name));
            GetDlgItemTextA(hWnd,IDC_EDIT_2,dept,sizeof(dept));
            GetDlgItemTextA(hWnd,IDC_EDIT_3,spec,sizeof(spec));
            GetDlgItemTextA(hWnd,IDC_EDIT_4,acct,sizeof(acct));
            GetDlgItemTextA(hWnd,IDC_EDIT_5,pwd,sizeof(pwd));
            GetDlgItemTextA(hWnd,IDC_EDIT_6,reg,sizeof(reg));
            if(!name[0]||!dept[0]||!acct[0]){gui_MsgBox(hWnd,"姓名/科室ID/账号不能为空！","错误",MB_ICONERROR);return 0;}
            if(!FindNode(dept_list,dept)){gui_MsgBox(hWnd,"科室ID不存在！","错误",MB_ICONERROR);return 0;}

            /* 检查账号唯一性 */
            ListNode* chk=doctor_list?doctor_list->head:NULL;
            while(chk){
                Doctor* d=(Doctor*)chk->data;
                if(d&&strcmp(d->account,acct)==0&&strcmp(d->id,s_editBuf.id)!=0){
                    gui_MsgBox(hWnd,"该账号已被其他医生使用！","错误",MB_ICONERROR);return 0;
                }
                chk=chk->next;
            }

            if(s_editBuf.id[0]){
                Doctor* d=(Doctor*)FindNode(doctor_list,s_editBuf.id)->data;
                if(d){
                    HIS_STRNCPY(d->name,name,MAX_NAME_LEN);
                    HIS_STRNCPY(d->dept_id,dept,MAX_ID_LEN);
                    HIS_STRNCPY(d->specialty,spec,MAX_SPECIALTY_LEN);
                    HIS_STRNCPY(d->account,acct,MAX_NAME_LEN);
                    if(pwd[0]){HIS_STRNCPY(d->password,pwd,MAX_PWD_LEN);passwordObfuscate(d->password);}
                    d->max_register=atoi(reg)>0?atoi(reg):30;
                    saveDoctorData();
                }
            }else{
                Doctor d;memset(&d,0,sizeof(d));
                HIS_STRNCPY(d.name,name,MAX_NAME_LEN);
                HIS_STRNCPY(d.dept_id,dept,MAX_ID_LEN);
                HIS_STRNCPY(d.specialty,spec,MAX_SPECIALTY_LEN);
                HIS_STRNCPY(d.account,acct,MAX_NAME_LEN);
                if(pwd[0]){HIS_STRNCPY(d.password,pwd,MAX_PWD_LEN);passwordObfuscate(d.password);}
                d.max_register=atoi(reg)>0?atoi(reg):30;
                if(generateUniqueID(d.id,ID_PREFIX_DOCTOR,doctor_list)!=0){
                    gui_MsgBox(hWnd,"无法生成唯一ID！","错误",MB_ICONERROR);return 0;
                }
                InsertNode(doctor_list,-1,&d,sizeof(Doctor),d.id);saveDoctorData();
            }
            g_modalResult = IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ---- 床位表单 ---- */
LRESULT CALLBACK BedFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static Bed s_editBuf;
    switch(msg){
    case WM_CREATE:{
        CREATESTRUCT* cs=(CREATESTRUCT*)l; int mod=(cs->lpCreateParams!=NULL);
        if(mod) s_editBuf=*(Bed*)cs->lpCreateParams;
        CreateWindowW(L"STATIC",L"科室ID",WS_CHILD|WS_VISIBLE,20,15,60,18,hWnd,(HMENU)-1,g_hInst,NULL);
        CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER,85,13,150,22,hWnd,(HMENU)IDC_EDIT_1,g_hInst,NULL);
        CreateWindowW(L"STATIC",L"类型(1普通/2半私密/3VIP)",WS_CHILD|WS_VISIBLE,20,48,160,18,hWnd,(HMENU)-1,g_hInst,NULL);
        CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER,160,45,40,22,hWnd,(HMENU)IDC_EDIT_2,g_hInst,NULL);
        CreateWindowW(L"BUTTON",mod?L"保存修改":L"添加",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,60,90,70,28,hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,160,90,70,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont);
        if(mod){SetDlgItemTextA(hWnd,IDC_EDIT_1,s_editBuf.dept_id);char b[8];snprintf(b,8,"%d",s_editBuf.room_type);SetDlgItemTextA(hWnd,IDC_EDIT_2,b);}
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            char dept[32],type[8];GetDlgItemTextA(hWnd,IDC_EDIT_1,dept,sizeof(dept));GetDlgItemTextA(hWnd,IDC_EDIT_2,type,sizeof(type));
            if(!dept[0]||!type[0]){gui_MsgBox(hWnd,"请填写科室ID和类型！","错误",MB_ICONERROR);return 0;}
            if(!FindNode(dept_list,dept)){gui_MsgBox(hWnd,"科室ID不存在！","错误",MB_ICONERROR);return 0;}
            int rt=atoi(type);if(rt<1||rt>3){gui_MsgBox(hWnd,"类型必须是1-3！","错误",MB_ICONERROR);return 0;}
            if(s_editBuf.id[0]){
                Bed* b=(Bed*)FindNode(bed_list,s_editBuf.id)->data;
                if(b){HIS_STRNCPY(b->dept_id,dept,MAX_ID_LEN);b->room_type=rt;saveBedData();}
            }else{
                Bed b;memset(&b,0,sizeof(b));b.room_type=rt;HIS_STRNCPY(b.dept_id,dept,MAX_ID_LEN);
                b.status=BED_FREE;
                if(generateUniqueID(b.id,ID_PREFIX_BED,bed_list)!=0){gui_MsgBox(hWnd,"无法生成ID！","错误",MB_ICONERROR);return 0;}
                InsertNode(bed_list,-1,&b,sizeof(Bed),b.id);saveBedData();
            }
            g_modalResult = IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ---- 药品表单 ---- */
LRESULT CALLBACK DrugFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static Drug s_editBuf;
    switch(msg){
    case WM_CREATE:{
        CREATESTRUCT* cs=(CREATESTRUCT*)l; int mod=(cs->lpCreateParams!=NULL);
        if(mod) s_editBuf=*(Drug*)cs->lpCreateParams;
        const wchar_t* labs[]={L"通用名",L"商品名",L"别名",L"单价",L"库存",L"预警阈值",L"科室ID"};
        for(int i=0;i<7;i++){
            CreateWindowW(L"STATIC",labs[i],WS_CHILD|WS_VISIBLE,20,12+i*28,60,18,hWnd,(HMENU)-1,g_hInst,NULL);
            CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER,85,10+i*28,160,22,hWnd,(HMENU)(IDC_EDIT_1+i),g_hInst,NULL);
        }
        CreateWindowW(L"BUTTON",mod?L"保存修改":L"添加",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,60,215,70,28,hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,160,215,70,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont);
        if(mod){
            char buf[256];
            SetDlgItemTextA(hWnd,IDC_EDIT_1,s_editBuf.general_name);
            SetDlgItemTextA(hWnd,IDC_EDIT_2,s_editBuf.trade_name);
            SetDlgItemTextA(hWnd,IDC_EDIT_3,s_editBuf.alias);
            snprintf(buf,sizeof(buf),"%.2f",s_editBuf.price);SetDlgItemTextA(hWnd,IDC_EDIT_4,buf);
            snprintf(buf,sizeof(buf),"%d",s_editBuf.stock);SetDlgItemTextA(hWnd,IDC_EDIT_5,buf);
            snprintf(buf,sizeof(buf),"%d",s_editBuf.warning_threshold);SetDlgItemTextA(hWnd,IDC_EDIT_6,buf);
            SetDlgItemTextA(hWnd,IDC_EDIT_7,s_editBuf.dept_id);
        }
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            Drug d;memset(&d,0,sizeof(d));
            char buf[256];
            GetDlgItemTextA(hWnd,IDC_EDIT_1,d.general_name,sizeof(d.general_name));
            GetDlgItemTextA(hWnd,IDC_EDIT_2,d.trade_name,sizeof(d.trade_name));
            GetDlgItemTextA(hWnd,IDC_EDIT_3,d.alias,sizeof(d.alias));
            GetDlgItemTextA(hWnd,IDC_EDIT_4,buf,sizeof(buf));d.price=(float)atof(buf);
            GetDlgItemTextA(hWnd,IDC_EDIT_5,buf,sizeof(buf));d.stock=atoi(buf);
            GetDlgItemTextA(hWnd,IDC_EDIT_6,buf,sizeof(buf));d.warning_threshold=atoi(buf);
            GetDlgItemTextA(hWnd,IDC_EDIT_7,d.dept_id,sizeof(d.dept_id));
            if(!d.general_name[0]){gui_MsgBox(hWnd,"通用名不能为空！","错误",MB_ICONERROR);return 0;}
            if(d.price<0){gui_MsgBox(hWnd,"单价不能为负！","错误",MB_ICONERROR);return 0;}
            if(!FindNode(dept_list,d.dept_id)){gui_MsgBox(hWnd,"科室ID不存在！","错误",MB_ICONERROR);return 0;}

            if(s_editBuf.id[0]){
                Drug* p=(Drug*)FindNode(drug_list,s_editBuf.id)->data;
                if(p){memcpy(p,&d,sizeof(Drug));HIS_STRNCPY(p->id,s_editBuf.id,MAX_ID_LEN);saveDrugData();}
            }else{
                if(generateUniqueID(d.id,ID_PREFIX_DRUG,drug_list)!=0){gui_MsgBox(hWnd,"无法生成ID！","错误",MB_ICONERROR);return 0;}
                InsertNode(drug_list,-1,&d,sizeof(Drug),d.id);saveDrugData();
            }
            g_modalResult = IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ---- 排班表单 ---- */
LRESULT CALLBACK ScheduleFormProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    static DoctorSchedule s_editBuf;
    switch(msg){
    case WM_CREATE:{
        CREATESTRUCT* cs=(CREATESTRUCT*)l; int mod=(cs->lpCreateParams!=NULL);
        if(mod) s_editBuf=*(DoctorSchedule*)cs->lpCreateParams;
        const wchar_t* labs[]={L"医生ID",L"科室ID",L"日期",L"时间段",L"总号源"};
        for(int i=0;i<5;i++){
            CreateWindowW(L"STATIC",labs[i],WS_CHILD|WS_VISIBLE,20,12+i*32,60,18,hWnd,(HMENU)-1,g_hInst,NULL);
            CreateWindowW(L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER,85,10+i*32,160,22,hWnd,(HMENU)(IDC_EDIT_1+i),g_hInst,NULL);
        }
        CreateWindowW(L"BUTTON",mod?L"保存修改":L"添加",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,60,180,70,28,hWnd,(HMENU)IDOK,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"取消",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,160,180,70,28,hWnd,(HMENU)IDCANCEL,g_hInst,NULL);
        gui_SetDialogFont(hWnd,g_hFont);
        if(mod){
            SetDlgItemTextA(hWnd,IDC_EDIT_1,s_editBuf.doctor_id);
            SetDlgItemTextA(hWnd,IDC_EDIT_2,s_editBuf.dept_id);
            SetDlgItemTextA(hWnd,IDC_EDIT_3,s_editBuf.date);
            SetDlgItemTextA(hWnd,IDC_EDIT_4,s_editBuf.time_slot);
            char b[16];snprintf(b,16,"%d",s_editBuf.max_patients);SetDlgItemTextA(hWnd,IDC_EDIT_5,b);
        }
        return 0;
    }
    case WM_COMMAND:
        if(LOWORD(w)==IDCANCEL){g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
        if(LOWORD(w)==IDOK){
            DoctorSchedule s;memset(&s,0,sizeof(s));char buf[128];
            GetDlgItemTextA(hWnd,IDC_EDIT_1,s.doctor_id,sizeof(s.doctor_id));
            GetDlgItemTextA(hWnd,IDC_EDIT_2,s.dept_id,sizeof(s.dept_id));
            GetDlgItemTextA(hWnd,IDC_EDIT_3,s.date,sizeof(s.date));
            GetDlgItemTextA(hWnd,IDC_EDIT_4,s.time_slot,sizeof(s.time_slot));
            GetDlgItemTextA(hWnd,IDC_EDIT_5,buf,sizeof(buf));
            if(!s.doctor_id[0]||!s.date[0]){gui_MsgBox(hWnd,"医生ID和日期不能为空！","错误",MB_ICONERROR);return 0;}
            s.max_patients=atoi(buf)>0?atoi(buf):30;
            s.current_patients=s_editBuf.id[0]?s_editBuf.current_patients:0;
            s.is_available=1;
            if(s_editBuf.id[0]){
                DoctorSchedule* p=(DoctorSchedule*)FindNode(schedule_list,s_editBuf.id)->data;
                if(p){memcpy(p,&s,sizeof(DoctorSchedule));HIS_STRNCPY(p->id,s_editBuf.id,MAX_ID_LEN);saveScheduleData();}
            }else{
                if(generateUniqueID(s.id,ID_PREFIX_SCHEDULE,schedule_list)!=0){gui_MsgBox(hWnd,"无法生成ID！","错误",MB_ICONERROR);return 0;}
                InsertNode(schedule_list,-1,&s,sizeof(DoctorSchedule),s.id);saveScheduleData();
            }
            g_modalResult = IDOK;DestroyWindow(hWnd);return 0;
        }
        return 0;
    case WM_CLOSE:{g_modalResult = IDCANCEL;DestroyWindow(hWnd);return 0;}
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== CRUD 按钮处理器 ==================== */

static void OnAdd(HWND hParent) {
    const wchar_t* cls[]={L"PatientForm",L"DeptForm",L"DoctorForm",L"BedForm",L"DrugForm",L"ScheduleForm"};
    const wchar_t* ttl[]={L"添加患者",L"添加科室",L"添加医生",L"添加床位",L"添加药品",L"添加排班"};
    HWND hF=CreateWindowW(cls[s_curTab],ttl[s_curTab],WS_CAPTION|WS_SYSMENU,
                          CW_USEDEFAULT,CW_USEDEFAULT,280,280,hParent,NULL,g_hInst,NULL);
    /* 居中 */
    RECT pr;GetWindowRect(hParent,&pr);
    SetWindowPos(hF,NULL,pr.left+((pr.right-pr.left)-280)/2,pr.top+((pr.bottom-pr.top)-280)/2,0,0,SWP_NOSIZE);
    DoModal(hF,280,280);DestroyWindow(hF);
    if(s_refreshFuncs[s_curTab]) s_refreshFuncs[s_curTab](s_hList[s_curTab]);
}

static void OnModify(HWND hParent) {
    /* 获取选中行 ID */
    int sel=ListView_GetNextItem(s_hList[s_curTab],-1,LVNI_SELECTED);
    if(sel<0){gui_MsgBox(hParent,"请先在列表中选择要修改的条目。","提示",MB_ICONINFORMATION);return;}
    WCHAR wid[32];ListView_GetItemText(s_hList[s_curTab],sel,0,wid,32);
    char id[32]={0};WideCharToMultiByte(CP_ACP,0,wid,-1,id,sizeof(id),NULL,NULL);

    /* 查找数据并传入表单 */
    void* data=NULL;
    if(s_curTab==TAB_PATIENT){ListNode*n=FindNode(patient_list,id);if(n)data=n->data;}
    else if(s_curTab==TAB_DEPT){ListNode*n=FindNode(dept_list,id);if(n)data=n->data;}
    else if(s_curTab==TAB_DOCTOR){ListNode*n=FindNode(doctor_list,id);if(n)data=n->data;}
    else if(s_curTab==TAB_BED){ListNode*n=FindNode(bed_list,id);if(n)data=n->data;}
    else if(s_curTab==TAB_DRUG){ListNode*n=FindNode(drug_list,id);if(n)data=n->data;}
    else if(s_curTab==TAB_SCHEDULE){ListNode*n=FindNode(schedule_list,id);if(n)data=n->data;}
    if(!data){gui_MsgBox(hParent,"未找到该数据。","错误",MB_ICONERROR);return;}

    const wchar_t* cls[]={L"PatientForm",L"DeptForm",L"DoctorForm",L"BedForm",L"DrugForm",L"ScheduleForm"};
    const wchar_t* ttl[]={L"修改患者",L"修改科室",L"修改医生",L"修改床位",L"修改药品",L"修改排班"};
    HWND hF=CreateWindowW(cls[s_curTab],ttl[s_curTab],WS_CAPTION|WS_SYSMENU,
                          CW_USEDEFAULT,CW_USEDEFAULT,280,280,hParent,NULL,g_hInst,(LPVOID)data);
    RECT pr;GetWindowRect(hParent,&pr);
    SetWindowPos(hF,NULL,pr.left+((pr.right-pr.left)-280)/2,pr.top+((pr.bottom-pr.top)-280)/2,0,0,SWP_NOSIZE);
    DoModal(hF,280,280);DestroyWindow(hF);
    if(s_refreshFuncs[s_curTab]) s_refreshFuncs[s_curTab](s_hList[s_curTab]);
}

static void OnDelete(HWND hParent) {
    int sel=ListView_GetNextItem(s_hList[s_curTab],-1,LVNI_SELECTED);
    if(sel<0){gui_MsgBox(hParent,"请先在列表中选择要删除的条目。","提示",MB_ICONINFORMATION);return;}
    if(!gui_Confirm(hParent,"确认删除选中条目？")) return;
    WCHAR wid[32];ListView_GetItemText(s_hList[s_curTab],sel,0,wid,32);
    char id[32]={0};WideCharToMultiByte(CP_ACP,0,wid,-1,id,sizeof(id),NULL,NULL);

    if(s_curTab==TAB_PATIENT){
        /* 检查是否有医疗记录 */
        ListNode* rn=record_list?record_list->head:NULL;
        while(rn){MedicalRecord*r=(MedicalRecord*)rn->data;if(r&&strcmp(r->patient_id,id)==0){gui_MsgBox(hParent,"该患者有医疗记录，无法删除！","错误",MB_ICONERROR);return;}rn=rn->next;}
        if(DeleteNode(patient_list,id)==0) savePatientData();
    }else if(s_curTab==TAB_DEPT){
        Department* d=(Department*)FindNode(dept_list,id)->data;
        if(d&&d->doctor_count>0){gui_MsgBox(hParent,"该科室有医生，无法删除！","错误",MB_ICONERROR);return;}
        if(DeleteNode(dept_list,id)==0) saveDeptData();
    }else if(s_curTab==TAB_DOCTOR){
        if(DeleteNode(doctor_list,id)==0) saveDoctorData();
    }else if(s_curTab==TAB_BED){
        if(DeleteNode(bed_list,id)==0) saveBedData();
    }else if(s_curTab==TAB_DRUG){
        if(DeleteNode(drug_list,id)==0) saveDrugData();
    }else if(s_curTab==TAB_SCHEDULE){
        if(DeleteNode(schedule_list,id)==0) saveScheduleData();
    }
    if(s_refreshFuncs[s_curTab]) s_refreshFuncs[s_curTab](s_hList[s_curTab]);
}

static void OnQuery(HWND hParent) {
    int sel=ListView_GetNextItem(s_hList[s_curTab],-1,LVNI_SELECTED);
    if(sel<0){gui_MsgBox(hParent,"请在列表中选择要查看的条目。","提示",MB_ICONINFORMATION);return;}
    /* 读取所有列的值并拼接显示 */
    WCHAR buf[1024]={0};
    int nCols=ListView_GetItemCount(s_hList[s_curTab]);
    (void)nCols; /* 用于获取列数 */
    HWND hHeader=ListView_GetHeader(s_hList[s_curTab]);
    int colCount=Header_GetItemCount(hHeader);
    for(int c=0;c<colCount;c++){
        WCHAR colName[64],cell[256];
        LVCOLUMNW lvc={.mask=LVCF_TEXT,.pszText=colName,.cchTextMax=64};
        ListView_GetColumn(s_hList[s_curTab],c,&lvc);
        ListView_GetItemText(s_hList[s_curTab],sel,c,cell,256);
        WCHAR line[320];wsprintfW(line,L"%s: %s\n",colName,cell);
        wcscat(buf,line);
    }
    MessageBoxW(hParent,buf,L"详细信息",MB_OK|MB_ICONINFORMATION);
}

/* ==================== 管理员窗口过程 ==================== */

LRESULT CALLBACK AdminWndProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE: {
        /* TabControl */
        s_hTab=CreateWindowW(WC_TABCONTROL,NULL,WS_CHILD|WS_VISIBLE|TCS_MULTILINE,
                             0,0,700,460,hWnd,(HMENU)IDC_TAB_ADMIN,g_hInst,NULL);

        /* 插入标签页 */
        TCITEMW tc={.mask=TCIF_TEXT};
        for(int i=0;i<6;i++){tc.pszText=(LPWSTR)s_tabTitles[i];TabCtrl_InsertItem(s_hTab,i,&tc);}

        /* 创建 6 个 ListView */
        DWORD lvStyle=WS_CHILD|WS_VISIBLE|WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS;
        s_hList[0]=CreateWindowW(WC_LISTVIEW,L"",lvStyle,10,30,680,380,hWnd,(HMENU)IDC_LIST_PATIENT,g_hInst,NULL);
        s_hList[1]=CreateWindowW(WC_LISTVIEW,L"",lvStyle,10,30,680,380,hWnd,(HMENU)IDC_LIST_DEPT,g_hInst,NULL);
        s_hList[2]=CreateWindowW(WC_LISTVIEW,L"",lvStyle,10,30,680,380,hWnd,(HMENU)IDC_LIST_DOCTOR,g_hInst,NULL);
        s_hList[3]=CreateWindowW(WC_LISTVIEW,L"",lvStyle,10,30,680,380,hWnd,(HMENU)IDC_LIST_BED,g_hInst,NULL);
        s_hList[4]=CreateWindowW(WC_LISTVIEW,L"",lvStyle,10,30,680,380,hWnd,(HMENU)IDC_LIST_DRUG,g_hInst,NULL);
        s_hList[5]=CreateWindowW(WC_LISTVIEW,L"",lvStyle,10,30,680,380,hWnd,(HMENU)IDC_LIST_SCHEDULE,g_hInst,NULL);

        /* 美化按钮：扁平样式 + 宽按钮 + 图标符号 */
        DWORD btnStyle = WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON|BS_FLAT;
        s_hBtnAdd=CreateWindowW(L"BUTTON",L"+ 新增",btnStyle,10,420,70,28,hWnd,(HMENU)IDC_BTN_ADD,g_hInst,NULL);
        s_hBtnMod=CreateWindowW(L"BUTTON",L"✐ 修改",btnStyle,88,420,70,28,hWnd,(HMENU)IDC_BTN_MODIFY,g_hInst,NULL);
        s_hBtnDel=CreateWindowW(L"BUTTON",L"✖ 删除",btnStyle,166,420,70,28,hWnd,(HMENU)IDC_BTN_DELETE,g_hInst,NULL);
        s_hBtnQry=CreateWindowW(L"BUTTON",L"○ 详情",btnStyle,244,420,70,28,hWnd,(HMENU)IDC_BTN_QUERY,g_hInst,NULL);
        s_hBtnRef=CreateWindowW(L"BUTTON",L"↻ 刷新",btnStyle,322,420,70,28,hWnd,(HMENU)IDC_BTN_REFRESH,g_hInst,NULL);
        CreateWindowW(L"BUTTON",L"☰ 统计",btnStyle,400,420,70,28,hWnd,(HMENU)1050,g_hInst,NULL);

        gui_SetDialogFont(hWnd,g_hFont);

        /* ListView 扩展样式：网格线 + 整行选择 + 双缓冲 */
        DWORD exStyle = LVS_EX_GRIDLINES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER;
        for(int i=0;i<6;i++) ListView_SetExtendedListViewStyle(s_hList[i], exStyle);

        /* 初始化 ListView 列 */
        gui_InitListView(s_hList[0],s_colPatient,s_widPatient,s_fmtPatient,9);
        gui_InitListView(s_hList[1],s_colDept,s_widDept,NULL,3);
        gui_InitListView(s_hList[2],s_colDoctor,s_widDoctor,NULL,6);
        gui_InitListView(s_hList[3],s_colBed,s_widBed,NULL,6);
        gui_InitListView(s_hList[4],s_colDrug,s_widDrug,NULL,7);
        gui_InitListView(s_hList[5],s_colSchedule,s_widSchedule,NULL,7);

        s_refreshFuncs[TAB_PATIENT](s_hList[TAB_PATIENT]);
        s_refreshFuncs[TAB_DEPT](s_hList[TAB_DEPT]);
        s_refreshFuncs[TAB_DOCTOR](s_hList[TAB_DOCTOR]);
        s_refreshFuncs[TAB_BED](s_hList[TAB_BED]);
        s_refreshFuncs[TAB_DRUG](s_hList[TAB_DRUG]);
        s_refreshFuncs[TAB_SCHEDULE](s_hList[TAB_SCHEDULE]);

        /* 默认只显示第一个标签页 */
        for(int i=1;i<6;i++) ShowWindow(s_hList[i],SW_HIDE);
        return 0;
    }

    case WM_SIZE: {
        int ww=LOWORD(l),wh=HIWORD(l);
        if(!s_hTab) break;
        SetWindowPos(s_hTab,NULL,0,0,ww,wh-50,SWP_NOZORDER);
        for(int i=0;i<6;i++){
            SetWindowPos(s_hList[i],NULL,10,30,ww-20,wh-100,SWP_NOZORDER);
        }
        SetWindowPos(s_hBtnAdd,NULL,10,wh-45,70,28,SWP_NOZORDER);
        SetWindowPos(s_hBtnMod,NULL,88,wh-45,70,28,SWP_NOZORDER);
        SetWindowPos(s_hBtnDel,NULL,166,wh-45,70,28,SWP_NOZORDER);
        SetWindowPos(s_hBtnQry,NULL,244,wh-45,70,28,SWP_NOZORDER);
        SetWindowPos(s_hBtnRef,NULL,322,wh-45,70,28,SWP_NOZORDER);
        /* Stats button position */
        {HWND c=GetDlgItem(hWnd,1050);if(c)SetWindowPos(c,NULL,400,wh-45,70,28,SWP_NOZORDER);}
        return 0;
    }

    case WM_NOTIFY: {
        NMHDR* h=(NMHDR*)l;
        /* ListView 斑马纹 */
        if (h->code == NM_CUSTOMDRAW) return gui_HandleListCustomDraw((LPNMLVCUSTOMDRAW)l);
        if(h->idFrom==IDC_TAB_ADMIN && h->code==TCN_SELCHANGE) {
            int sel=TabCtrl_GetCurSel(s_hTab);
            if(sel!=s_curTab){
                ShowWindow(s_hList[s_curTab],SW_HIDE);
                s_curTab=sel;
                ShowWindow(s_hList[s_curTab],SW_SHOW);
                if(s_refreshFuncs[s_curTab]) s_refreshFuncs[s_curTab](s_hList[s_curTab]);
            }
        }
        return 0;
    }

    case WM_COMMAND:
        switch(LOWORD(w)){
        case IDC_BTN_ADD: OnAdd(hWnd); return 0;
        case IDC_BTN_MODIFY: OnModify(hWnd); return 0;
        case IDC_BTN_DELETE: OnDelete(hWnd); return 0;
        case IDC_BTN_QUERY: OnQuery(hWnd); return 0;
        case IDC_BTN_REFRESH: if(s_refreshFuncs[s_curTab]) s_refreshFuncs[s_curTab](s_hList[s_curTab]); return 0;
        case 1050: { /* 统计 */
            HWND hDS = CreateWindowW(L"StatsDashboard", L"HIS 系统统计仪表板",
                WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                500, 300, hWnd, NULL, g_hInst, NULL);
            DoModal(hDS, 480, 290); DestroyWindow(hDS);
            return 0;
        }
        }
        return 0;

    case WM_REFRESH_DATA:
        if(s_refreshFuncs[s_curTab]) s_refreshFuncs[s_curTab](s_hList[s_curTab]);
        return 0;

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)w;
        SetTextColor(hdc, RGB(40, 70, 110));
        SetBkMode(hdc, TRANSPARENT);
        static HBRUSH hBr = NULL;
        if(!hBr) hBr = CreateSolidBrush(RGB(242, 244, 249));
        return (LRESULT)hBr;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)l;
        mmi->ptMinTrackSize.x = 720;
        mmi->ptMinTrackSize.y = 520;
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    }
    return DefWindowProc(hWnd,msg,w,l);
}

/* ==================== 统计仪表板窗口 ==================== */

#define IDC_PROGRESS_BED    1101
#define IDC_PROGRESS_DRUG   1102

LRESULT CALLBACK StatsDashboardProc(HWND hWnd, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE: {
        /* 计算统计数据 */
        int patientCount = 0, doctorCount = 0, deptCount = 0;
        int bedTotal = 0, bedOccupied = 0, drugWarnings = 0, recordCount = 0;
        ListNode* n;
        n = patient_list ? patient_list->head : NULL; while (n) { patientCount++; n = n->next; }
        n = doctor_list ? doctor_list->head : NULL; while (n) { doctorCount++; n = n->next; }
        n = dept_list ? dept_list->head : NULL; while (n) { deptCount++; n = n->next; }
        n = bed_list ? bed_list->head : NULL; while (n) { Bed* b = (Bed*)n->data; if (b) { bedTotal++; if (b->status == BED_OCCUPIED) bedOccupied++; } n = n->next; }
        n = drug_list ? drug_list->head : NULL; while (n) { Drug* d = (Drug*)n->data; if (d && d->stock < d->warning_threshold) drugWarnings++; n = n->next; }
        int drugTotal = 0; n = drug_list ? drug_list->head : NULL; while (n) { drugTotal++; n = n->next; }
        n = record_list ? record_list->head : NULL; while (n) { recordCount++; n = n->next; }

        /* 标题 */
        CreateWindowW(L"STATIC", L"HIS 系统统计仪表板", WS_CHILD | WS_VISIBLE | SS_CENTER,
            10, 8, 380, 28, hWnd, (HMENU)-1, g_hInst, NULL);

        /* 床位数统计 + 进度条 */
        CreateWindowW(L"STATIC", L"床位占用率", WS_CHILD | WS_VISIBLE,
            20, 50, 120, 18, hWnd, (HMENU)-1, g_hInst, NULL);
        HWND hPB = CreateWindowW(PROGRESS_CLASS, L"", WS_CHILD | WS_VISIBLE,
            150, 48, 200, 22, hWnd, (HMENU)IDC_PROGRESS_BED, g_hInst, NULL);
        int bedPct = bedTotal > 0 ? (bedOccupied * 100 / bedTotal) : 0;
        SendMessage(hPB, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessage(hPB, PBM_SETPOS, bedPct, 0);
        { char buf[64]; snprintf(buf, 64, "%d / %d (%.0f%%)", bedOccupied, bedTotal, bedTotal > 0 ? bedOccupied * 100.0 / bedTotal : 0.0);
        WCHAR wb[64]; acpToWide(buf, wb, 64); CreateWindowW(L"STATIC", wb, WS_CHILD | WS_VISIBLE, 360, 50, 100, 18, hWnd, (HMENU)-1, g_hInst, NULL); }

        /* 药品预警统计 + 进度条 */
        CreateWindowW(L"STATIC", L"药品预警", WS_CHILD | WS_VISIBLE,
            20, 82, 120, 18, hWnd, (HMENU)-1, g_hInst, NULL);
        HWND hPB2 = CreateWindowW(PROGRESS_CLASS, L"", WS_CHILD | WS_VISIBLE,
            150, 80, 200, 22, hWnd, (HMENU)IDC_PROGRESS_DRUG, g_hInst, NULL);
        SendMessage(hPB2, PBM_SETRANGE, 0, MAKELPARAM(0, drugTotal > 0 ? drugTotal : 1));
        SendMessage(hPB2, PBM_SETPOS, drugWarnings, 0);
        { char buf[64]; snprintf(buf, 64, "%d 种库存不足", drugWarnings);
        WCHAR wb[64]; acpToWide(buf, wb, 64); CreateWindowW(L"STATIC", wb, WS_CHILD | WS_VISIBLE, 360, 82, 120, 18, hWnd, (HMENU)-1, g_hInst, NULL); }

        /* 统计数据文本行 */
        int y = 120;
        char lines[6][128];
        snprintf(lines[0], 128, "患者总数: %d 人", patientCount);
        snprintf(lines[1], 128, "医生总数: %d 人", doctorCount);
        snprintf(lines[2], 128, "科室总数: %d 个", deptCount);
        snprintf(lines[3], 128, "医疗记录: %d 条", recordCount);
        for (int i = 0; i < 4; i++) {
            WCHAR wb[128]; acpToWide(lines[i], wb, 128);
            CreateWindowW(L"STATIC", wb, WS_CHILD | WS_VISIBLE, 30, y + i * 24, 200, 20, hWnd, (HMENU)-1, g_hInst, NULL);
        }

        CreateWindowW(L"BUTTON", L"关闭", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            170, 225, 70, 28, hWnd, (HMENU)IDCANCEL, g_hInst, NULL);

        gui_SetDialogFont(hWnd, g_hFont);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w) == IDCANCEL) { DestroyWindow(hWnd); return 0; }
        return 0;
    case WM_CLOSE: DestroyWindow(hWnd); return 0;
    }
    return DefWindowProc(hWnd, msg, w, l);
}

/* ==================== 注册 ==================== */

void gui_RegisterAdminClasses(HINSTANCE hInst) {
    WNDCLASS wc={.style=CS_HREDRAW|CS_VREDRAW,.hInstance=hInst,.hCursor=LoadCursor(NULL,IDC_ARROW),
                  .hbrBackground=(HBRUSH)(COLOR_WINDOW+1),.lpszMenuName=NULL};

    wc.lpfnWndProc=AdminWndProc; wc.lpszClassName=L"HIS_AdminWnd"; RegisterClass(&wc);
    wc.lpfnWndProc=PatientFormProc; wc.lpszClassName=L"PatientForm"; RegisterClass(&wc);
    wc.lpfnWndProc=DeptFormProc; wc.lpszClassName=L"DeptForm"; RegisterClass(&wc);
    wc.lpfnWndProc=DoctorFormProc; wc.lpszClassName=L"DoctorForm"; RegisterClass(&wc);
    wc.lpfnWndProc=BedFormProc; wc.lpszClassName=L"BedForm"; RegisterClass(&wc);
    wc.lpfnWndProc=DrugFormProc; wc.lpszClassName=L"DrugForm"; RegisterClass(&wc);
    wc.lpfnWndProc=ScheduleFormProc; wc.lpszClassName=L"ScheduleForm"; RegisterClass(&wc);
    wc.lpfnWndProc=StatsDashboardProc; wc.lpszClassName=L"StatsDashboard"; RegisterClass(&wc);
}
