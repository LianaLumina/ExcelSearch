; Excel 表格关键字搜索工具 V0.3.2 安装脚本（Inno Setup 6）
; ---------------------------------------------------------------------------
; 前置：先跑 tools\deploy.ps1 生成部署目录 ..\build\deploy\（exe + Qt DLL + 插件 + licenses + 空 data）
; 用法："C:\Program Files (x86)\Inno Setup 6\ISCC.exe" setup.iss
; 产物：..\build\installer\ExcelSearchSetup-0.3.2.exe
;
; 设计要点：
;   1) 管理员安装（PrivilegesRequired=admin），但**程序本身以普通权限运行**（asInvoker，见 app_qt.manifest）
;   2) 安装位置可改（DisableDirPage=no）
;   3) 桌面 / 开始菜单 / 任务栏 三项快捷方式均可选
;   4) data 目录**空着自带**，并授予普通用户写权限（Permissions: users-modify）——
;      否则装在 Program Files 下用户"丢不进文件"，与"装完就能用"的目标冲突
;   5) 卸载时**分别询问**是否删除用户配置与 data 里的表格文件，并明确告知是否删干净
;   6) AppId 与 V0.3.0 相同 → 新版覆盖升级旧版，不产生两个并存条目

#define MyAppName "Excel 表格关键字搜索工具"
#define MyAppNameEng "ExcelSearch"
#define MyAppVersion "0.3.2"
#define MyAppPublisher "觉心恋影"
#define MyAppExeName "excel_search.exe"
#define DeployDir "..\build\deploy"

[Setup]
AppId={{8E4C7A21-3B5D-4F1A-9C62-A1B2C3D4E5F6}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} 安装程序
DefaultDirName={autopf}\{#MyAppNameEng}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=no
; 安装时必须阅读并接受 GPL-3.0
LicenseFile=..\LICENSE
PrivilegesRequired=admin
OutputDir=..\build\installer
OutputBaseFilename=ExcelSearchSetup-{#MyAppVersion}
SetupIconFile=app_setup_small.ico
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
; 装/卸载时若程序正在运行，提示关闭（单实例闸门用命名管道，Inno 靠进程名识别）
; 任务栏快捷方式必须写用户固定目录（per-user），管理员安装模式下 Inno 会提示该警告，已知并接受
UsedUserAreasWarning=no
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon";   Description: "创建桌面快捷方式";                 GroupDescription: "附加图标："; Flags: unchecked
Name: "startmenuicon"; Description: "创建开始菜单快捷方式";             GroupDescription: "附加图标："
Name: "taskbaricon";   Description: "创建任务栏快捷方式"; GroupDescription: "附加图标："; Flags: unchecked

[Dirs]
; 数据目录：空着自带 + 授予普通用户修改权限（方案①）
Name: "{app}\data"; Permissions: users-modify

[Files]
; 整个部署产物（exe + Qt DLL + 插件 + licenses + MANUAL.md）——由 tools\deploy.ps1 生成
Source: "{#DeployDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; 说明：MANUAL.md（使用说明书正文）随部署产物一并安装到 {app}\，程序优先读取它、缺失时回退到内嵌副本。

[Icons]
Name: "{group}\{#MyAppName}";        Filename: "{app}\{#MyAppExeName}"; Tasks: startmenuicon
Name: "{autodesktop}\{#MyAppName}";  Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
; 任务栏：写入"用户固定"目录（Win10 生效；Win11 通常需用户手动固定一次）
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: taskbaricon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动 {#MyAppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 只清理可能的空目录；**data 目录与用户配置一律不在此处删除**，改由 [Code] 段询问后处理
Type: dirifempty; Name: "{app}\licenses\msys2"
Type: dirifempty; Name: "{app}\licenses"
Type: dirifempty; Name: "{app}\platforms"
Type: dirifempty; Name: "{app}"

[Code]
// ======================= 卸载向导（多页，仿安装向导布局） =======================
// 为什么自建：Inno 的卸载器**不支持自定义向导页**（CreateCustomPage 仅安装器可用；
//   6.7.3 更新日志只提到 WizardSizePercent/WizardBackColor 已支持 Uninstall，从未提供卸载器页面 API）。
// 因此这里自绘一个两页向导：
//   第 1 页「卸载选项」—— 三个复选框（全部删除 / 删 data / 删用户配置），勾「全部」联动置灰；
//   第 2 页「确认卸载」—— 列出将删除 / 将保留的内容；
//   底部按钮 [上一步] [下一步…/卸载] [取消]，与安装向导的按钮布局一致；
//   每页有粗体标题 + 灰色说明（对齐向导页的标题层级）。
// 注意：Inno 的 Pascal 注释用 //，不要用 { }（会被当成参数名报错，实测踩过）；
//   也不要给变量取名 List（与脚本引擎冲突，实测踩过）。
// 静默卸载（/VERYSILENT）不弹任何界面，且一律保留用户数据（安全默认）。

var
  DeleteAll: Boolean;
  DeleteUserData: Boolean;
  DeleteUserConfig: Boolean;
  WizPage: Integer;
  WizConfirmed: Boolean;   // ModalResult 在 Inno 的 TForm 上只读，故用标志位
  WizForm: TForm;
  P1Title: TNewStaticText;
  P1Desc: TNewStaticText;
  WizList: TNewCheckListBox;
  P1Path: TNewStaticText;
  P2Title: TNewStaticText;
  P2Desc: TNewStaticText;
  P2Body: TNewStaticText;
  BtnBack: TNewButton;
  BtnNext: TNewButton;
  BtnCancel: TNewButton;

procedure WizSyncButtons();
begin
  BtnBack.Enabled := (WizPage > 1);
  if WizPage = 1 then
    BtnNext.Caption := '下一步…'
  else
    BtnNext.Caption := '卸载';
end;

procedure WizShowPage(P: Integer);
begin
  WizPage := P;
  P1Title.Visible := (P = 1);  P1Desc.Visible := (P = 1);
  WizList.Visible := (P = 1);  P1Path.Visible := (P = 1);
  P2Title.Visible := (P = 2);  P2Desc.Visible := (P = 2);  P2Body.Visible := (P = 2);
  WizSyncButtons();
end;

// 「删除全部数据」联动：勾上 → 子项强制勾选并置灰不可改；取消 → 子项恢复可改并取消
procedure WizAllClick(Sender: TObject);
begin
  if WizList.Checked[0] then
  begin
    WizList.Checked[1] := True;   WizList.ItemEnabled[1] := False;
    WizList.Checked[2] := True;   WizList.ItemEnabled[2] := False;
  end
  else
  begin
    WizList.Checked[1] := False;  WizList.ItemEnabled[1] := True;
    WizList.Checked[2] := False;  WizList.ItemEnabled[2] := True;
  end;
end;

procedure WizBackClick(Sender: TObject);
begin
  WizShowPage(1);
end;

procedure WizNextClick(Sender: TObject);
var
  S: String;
begin
  if WizPage = 1 then
  begin
    S := '请确认以下内容：' + #13#10#13#10;
    S := S + '[将删除] 程序文件：' + ExpandConstant('{app}') + #13#10;
    if WizList.Checked[1] then
      S := S + '[将删除] 数据目录内的文件：' + ExpandConstant('{app}\data') + #13#10
    else
      S := S + '[将保留] 数据目录内的文件：' + ExpandConstant('{app}\data') + #13#10;
    if WizList.Checked[2] then
      S := S + '[将删除] 用户配置与索引缓存：' + ExpandConstant('{userappdata}\ExcelSearch')
    else
      S := S + '[将保留] 用户配置与索引缓存：' + ExpandConstant('{userappdata}\ExcelSearch');
    P2Body.Caption := S;
    WizShowPage(2);
  end
  else
  begin
    WizConfirmed := True;
    WizForm.Close;   // 关闭向导 -> 开始卸载
  end;   // 关闭向导 → 开始卸载
end;

procedure WizCancelClick(Sender: TObject);
begin
  WizConfirmed := False;
  WizForm.Close;
end;

function ShowUninstallOptions(): Boolean;
begin
  Result := False;
  WizConfirmed := False;
  WizForm := TForm.Create(nil);
  try
    WizForm.Caption := '卸载 {#MyAppName}';
    WizForm.BorderStyle := bsDialog;
    WizForm.Position := poScreenCenter;
    WizForm.ClientWidth := 580;
    WizForm.ClientHeight := 340;
    WizForm.ShowHint := False;
    WizForm.Font.Name := 'Microsoft YaHei UI';
    WizForm.Font.Size := 9;
    WizForm.Color := clWhite;   // 内容区白底，贴近现代向导观感

    // ---- 第 1 页：卸载选项 ----
    P1Title := TNewStaticText.Create(WizForm);
    P1Title.Parent := WizForm;
    P1Title.Left := 24; P1Title.Top := 20; P1Title.Width := 540;
    P1Title.Caption := '卸载选项';
    P1Title.Font.Name := WizForm.Font.Name;
    P1Title.Font.Size := 14;
    P1Title.Font.Style := [fsBold];

    P1Desc := TNewStaticText.Create(WizForm);
    P1Desc.Parent := WizForm;
    P1Desc.Left := 26; P1Desc.Top := 54; P1Desc.Width := 530;
    P1Desc.Caption := '请选择要一并删除的内容。未勾选的内容会保留在本机，卸载后仍可找回。';
    P1Desc.Font.Name := WizForm.Font.Name;
    P1Desc.Font.Color := clGrayText;

    // 与安装向导「任务」页同款控件；勾选但置灰是其原生能力
    WizList := TNewCheckListBox.Create(WizForm);
    WizList.Parent := WizForm;
    WizList.Left := 26; WizList.Top := 82; WizList.Width := 528; WizList.Height := 160;
    WizList.Font.Name := WizForm.Font.Name;
    // AddCheckBox(ACaption, ASubItem, ALevel, AChecked, AEnabled,
    //             AHasInternalChildren, ACheckWhenParentChecked, AObject)
    WizList.AddCheckBox('删除全部数据（下面两项一并删除）', '', 0, False, True, True, True, nil);
    WizList.AddCheckBox('删除 data 文件夹内的数据（你自己的表格文件）', '', 1, False, True, False, False, nil);
    WizList.AddCheckBox('删除用户配置与索引缓存（主题 / 屏蔽 / 标记 / 搜索历史）', '', 1, False, True, False, False, nil);
    WizList.OnClickCheck := @WizAllClick;

    P1Path := TNewStaticText.Create(WizForm);
    P1Path.Parent := WizForm;
    P1Path.Left := 26; P1Path.Top := 250; P1Path.Width := 530;
    P1Path.AutoSize := False; P1Path.Height := 34; P1Path.WordWrap := True;
    P1Path.Caption := '数据目录：' + ExpandConstant('{app}\data') + #13#10 +
                      '用户配置：' + ExpandConstant('{userappdata}\ExcelSearch');
    P1Path.Font.Name := WizForm.Font.Name;
    P1Path.Font.Color := clGrayText;

    // ---- 第 2 页：确认 ----
    P2Title := TNewStaticText.Create(WizForm);
    P2Title.Parent := WizForm;
    P2Title.Left := 24; P2Title.Top := 20; P2Title.Width := 540;
    P2Title.Caption := '确认卸载';
    P2Title.Font.Name := WizForm.Font.Name;
    P2Title.Font.Size := 14;
    P2Title.Font.Style := [fsBold];

    P2Desc := TNewStaticText.Create(WizForm);
    P2Desc.Parent := WizForm;
    P2Desc.Left := 26; P2Desc.Top := 54; P2Desc.Width := 530;
    P2Desc.Caption := '点「卸载」开始卸载，点「上一步」返回修改选项。';
    P2Desc.Font.Name := WizForm.Font.Name;
    P2Desc.Font.Color := clGrayText;

    P2Body := TNewStaticText.Create(WizForm);
    P2Body.Parent := WizForm;
    P2Body.Left := 28; P2Body.Top := 88; P2Body.Width := 526;
    P2Body.AutoSize := False; P2Body.Height := 200; P2Body.WordWrap := True;
    P2Body.Font.Name := WizForm.Font.Name;

    // ---- 底部按钮（与安装向导一致：左空、右侧依次 上一步 / 下一步 / 取消）----
    BtnBack := TNewButton.Create(WizForm);
    BtnBack.Parent := WizForm;
    BtnBack.Caption := '上一步';
    BtnBack.Left := WizForm.ClientWidth - 306; BtnBack.Top := WizForm.ClientHeight - 46;
    BtnBack.Width := 88; BtnBack.Height := 26;
    BtnBack.OnClick := @WizBackClick;

    BtnNext := TNewButton.Create(WizForm);
    BtnNext.Parent := WizForm;
    BtnNext.Caption := '下一步…';
    BtnNext.Left := WizForm.ClientWidth - 212; BtnNext.Top := WizForm.ClientHeight - 46;
    BtnNext.Width := 88; BtnNext.Height := 26;
    BtnNext.Default := True;
    BtnNext.OnClick := @WizNextClick;

    BtnCancel := TNewButton.Create(WizForm);
    BtnCancel.Parent := WizForm;
    BtnCancel.Caption := '取消';
    BtnCancel.Left := WizForm.ClientWidth - 118; BtnCancel.Top := WizForm.ClientHeight - 46;
    BtnCancel.Width := 88; BtnCancel.Height := 26;
    BtnCancel.Cancel := True;
    BtnCancel.OnClick := @WizCancelClick;

    WizShowPage(1);
    WizForm.ShowModal;
    if WizConfirmed then
    begin
      DeleteAll := WizList.Checked[0];
      DeleteUserData := WizList.Checked[1];
      DeleteUserConfig := WizList.Checked[2];
      Result := True;
    end;
  finally
    WizForm.Free;
  end;
end;

function InitializeUninstall(): Boolean;
var
  ResultCode: Integer;
begin
  Result := True;
  DeleteAll := False;
  DeleteUserData := False;
  DeleteUserConfig := False;

  // ★卸载前必须先结束正在运行的程序：
  //   程序本身与其加载的 Qt DLL / 平台插件会被系统锁住，卸载器删不掉，
  //   结果是「卸载成功但留下 79MB 残骸」（实测踩过）。本程序设置即时落盘、无未保存状态，
  //   且全机单实例，故按映像名结束最多影响一个进程，安全。
  Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM excel_search.exe', '', SW_HIDE,
       ewWaitUntilTerminated, ResultCode);
  Sleep(600);   // 等系统释放文件句柄

  // 静默卸载：不弹界面，也绝不删用户数据
  if UninstallSilent then
  begin
    Result := True;
    Exit;
  end;
  // 用户点「取消」/ 关窗 → 中止卸载
  Result := ShowUninstallOptions();
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Msg: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    // 「删除全部数据」已保证另两项为真，这里无需再分叉
    if DeleteUserData then
      DelTree(ExpandConstant('{app}\data'), True, True, True);
    if DeleteUserConfig then
      DelTree(ExpandConstant('{userappdata}\ExcelSearch'), True, True, True);

    if UninstallSilent then Exit;   // 静默卸载不弹结果框

    Msg := '卸载完成。' + #13#10#13#10;
    if DeleteUserData then
      Msg := Msg + '[已删除] 数据目录：' + ExpandConstant('{app}\data')
    else
      Msg := Msg + '[已保留] 数据目录：' + ExpandConstant('{app}\data');
    Msg := Msg + #13#10;
    if DeleteUserConfig then
      Msg := Msg + '[已删除] 用户配置：' + ExpandConstant('{userappdata}\ExcelSearch')
    else
      Msg := Msg + '[已保留] 用户配置：' + ExpandConstant('{userappdata}\ExcelSearch');
    Msg := Msg + #13#10#13#10 + '以上未标注「已删除」的内容仍保留在本机。';

    MsgBox(Msg, mbInformation, MB_OK);
  end;
end;

// ======================= 安装结束时把卸载器改名为 uninstall.exe =======================
// ⚠️ 重写 [Code] 段时切勿再漏掉本段（曾漏过一次，导致卸载器又变回 unins000.exe）。
// 背景：Inno 官方不支持自定义卸载器文件名（实测 6.7.3 无该指令）。
// 做法：把 unins000.exe 与 unins000.dat **一起**改名（卸载器按自身文件名推导 .dat，必须同改），
//       再同步注册表卸载入口；任一步失败都回滚，保证「卸载器能正常用」优先于「名字好看」。
// 说明：必须同时挂 ssPostInstall 与 ssDone —— **静默安装下 ssDone 不会触发**（曾因此漏改）。
procedure RenameUninstaller();
var
  FindRec: TFindRec;
  AppDir, OldExe, OldDat, NewExe, NewDat, RegKey, AppId: String;
begin
  AppDir := ExpandConstant('{app}');
  // SetupSetting("AppId") 返回未反转义的 {{GUID}，而注册表真实键是 {GUID}_is1，故去掉重复花括号
  AppId := '{#SetupSetting("AppId")}';
  if (Length(AppId) > 1) and (AppId[1] = '{') and (AppId[2] = '{') then
    AppId := Copy(AppId, 2, Length(AppId) - 1);
  RegKey := 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\' + AppId + '_is1';

  OldExe := '';
  if FindFirst(AppDir + '\unins*.exe', FindRec) then
  begin
    try
      OldExe := AppDir + '\' + FindRec.Name;
    finally
      FindClose(FindRec);
    end;
  end;
  if OldExe = '' then Exit;
  OldDat := Copy(OldExe, 1, Length(OldExe) - 4) + '.dat';
  if not FileExists(OldDat) then Exit;

  NewExe := AppDir + '\uninstall.exe';
  NewDat := AppDir + '\uninstall.dat';
  if FileExists(NewExe) then Exit;          // 已改过

  if not RenameFile(OldExe, NewExe) then Exit;
  if not RenameFile(OldDat, NewDat) then
  begin
    RenameFile(NewExe, OldExe);             // 回滚
    Exit;
  end;

  RegWriteStringValue(HKLM, RegKey, 'UninstallString', '"' + NewExe + '"');
  RegWriteStringValue(HKLM, RegKey, 'QuietUninstallString', '"' + NewExe + '" /SILENT');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) or (CurStep = ssDone) then
    RenameUninstaller();
end;