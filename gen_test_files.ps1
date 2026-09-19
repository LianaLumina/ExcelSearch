$ErrorActionPreference = 'Continue'
$dir = '<repo-v0.3.0>\data'
New-Item -ItemType Directory -Force -Path $dir | Out-Null

# ---------- CSV（手写：BOM + 中文 + 引号/逗号字段，覆盖 csv_reader 边角） ----------
$csvLines = @(
  '姓名,部门,月份,核定工日,备注',
  '张三,运维部,2025-01,22,"含周末加班"',
  '李四,安装部,"安装,调试",20,"跨""班""作业"',
  '王五,检修部,2025-02,25,正常',
  '赵六,数据组,2025-01,18,含"节假日"值守'
)
$csvText = ($csvLines -join "`r`n") + "`r`n"
$utf8Bom = [System.Text.UTF8Encoding]::new($true)
[System.IO.File]::WriteAllText("$dir\回归测试_人员表.csv", $csvText, $utf8Bom)
Write-Output 'csv written'

# ---------- Excel / Word COM 生成（真实文件，可同时被 Office 打开验证） ----------
$excelOk = $false
$wordOk = $false

try {
  $excel = New-Object -ComObject Excel.Application
  $excel.Visible = $false
  $excel.DisplayAlerts = $false
  $wb = $excel.Workbooks.Add()
  $null = $wb.Worksheets.Item(1)
  $s1 = $wb.Sheets.Item(1); $s1.Name = '人员表'
  $rows1 = @(
    @('姓名','部门','工号','工时','工日'),
    @('张三','运维部','YW001',168,21),
    @('李四','安装部','AZ002',190,24),
    @('王五','检修部','JX003',176,22)
  )
  for ($r = 0; $r -lt $rows1.Count; $r++) {
    for ($c = 0; $c -lt $rows1[$r].Count; $c++) { $s1.Cells.Item($r + 1, $c + 1) = $rows1[$r][$c] }
  }
  $s2 = $wb.Sheets.Add([System.Reflection.Missing]::Value, $s1)
  $s2.Name = '工时汇总'
  $s2.Cells.Item(1,1) = '部门'
  $s2.Cells.Item(1,2) = '总工时'
  $s2.Cells.Item(1,3) = '人均工日'
  $s2.Cells.Item(2,1) = '运维部'; $s2.Cells.Item(2,2) = 336; $s2.Cells.Item(2,3) = 21
  $s2.Cells.Item(3,1) = '安装部'; $s2.Cells.Item(3,2) = 190; $s2.Cells.Item(3,3) = 24

  $excel.DisplayAlerts = $false
  $wb.SaveAs("$dir\回归测试_多表.xlsx", 51)     # xlsx
  $wb.SaveAs("$dir\回归测试_旧版.xls", 56)      # xls(BIFF8) -> 测 xls_reader
  $wb.Close($false)
  $excel.Quit()
  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($excel) | Out-Null
  $excelOk = $true
  Write-Output 'excel xlsx+xls written'
} catch {
  Write-Output "excel COM unavailable: $($_.Exception.Message)"
}

try {
  $word = New-Object -ComObject Word.Application
  $word.Visible = $false
  $word.DisplayAlerts = 0
  $doc = $word.Documents.Add()
  $sel = $word.Selection
  $sel.Style = $doc.Styles.Item('标题 1')
  $sel.TypeText('大修工日核定辅助软件 回归测试文档')
  $sel.TypeParagraph()
  $sel.Style = $doc.Styles.Item('正文')
  $sel.TypeText('第一段：包含中文与数字，部门运维部工时 168，工日 21。')
  $sel.TypeParagraph()
  $sel.TypeText('第二段：测试特殊符号 & < > " 引号 与换行内容，供 docx 文本提取检测。')
  $sel.TypeParagraph()
  $sel.TypeText('关键词：搜索命中测试 大修 工日 核定。')
  $doc.SaveAs("$dir\回归测试_文档.docx", 16)   # wdFormatXMLDocument
  $doc.Close($false)
  $word.Quit()
  [System.Runtime.Interopservices.Marshal]::ReleaseComObject($word) | Out-Null
  $wordOk = $true
  Write-Output 'word docx written'
} catch {
  Write-Output "word COM unavailable: $($_.Exception.Message)"
}

# ---------- 若 COM 不可用，提供手写最小 xlsx/docx 兜底 ----------
if (-not $excelOk) {
  # 最小 xlsx：workbook + 一张 sheet + 共享字符串（reader 兼容）
  try {
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $wbXml = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>' +
      '<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">' +
      '<sheets><sheet name="Sheet1" sheetId="1" r:id="rId1"/></sheets></workbook>'
    $ssXml = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>' +
      '<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><si><t>运维部</t></si><si><t>安装部</t></si></sst>'
    $shXml = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>' +
      '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>' +
      '<row r="1"><c r="A1" t="inlineStr"><is><t>姓名</t></is></c><c r="B1" t="inlineStr"><is><t>部门</t></is></c></row>' +
      '<row r="2"><c r="A2" t="inlineStr"><is><t>张三</t></is></c><c r="B2" t="s"><v>0</v></c><c r="C2"><v>168</v></c></row>' +
      '<row r="3"><c r="A3" t="inlineStr"><is><t>李四</t></is></c><c r="B3" t="s"><v>1</v></c><c r="C3"><v>190</v></c></row>' +
      '</sheetData></worksheet>'
    $fs = [System.IO.File]::Create("$dir\回归测试_兜底.xlsx")
    $zip = New-Object System.IO.Compression.ZipArchive($fs, [System.IO.Compression.ZipArchiveMode]::Create)
    foreach ($item in @(@('xl/workbook.xml',$wbXml), @('xl/sharedStrings.xml',$ssXml), @('xl/worksheets/sheet1.xml',$shXml))) {
      $entry = $zip.CreateEntry($item[0], [System.IO.Compression.CompressionLevel]::Optimal)
      $sw = [System.IO.StreamWriter]::new($entry.Open(), [System.Text.UTF8Encoding]::new($false))
      $sw.Write($item[1]); $sw.Dispose()
    }
    $zip.Dispose(); $fs.Dispose()
    Write-Output 'fallback xlsx written'
  } catch { Write-Output "fallback xlsx failed: $($_.Exception.Message)" }
}
if (-not $wordOk) {
  # 最小 docx（reader 兼容）
  try {
    $documentXml = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>' +
      '<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body>' +
      '<w:p><w:r><w:t>回归测试文档 标题</w:t></w:r></w:p>' +
      '<w:p><w:r><w:t>第一段 中文与数字 工时168 工日21</w:t></w:r></w:p>' +
      '<w:p><w:r><w:t>关键词 搜索命中测试 大修 工日 核定</w:t></w:r></w:p>' +
      '</w:body></w:document>'
    $fs = [System.IO.File]::Create("$dir\回归测试_兜底.docx")
    $zip = New-Object System.IO.Compression.ZipArchive($fs, [System.IO.Compression.ZipArchiveMode]::Create)
    foreach ($item in @(@('word/document.xml',$documentXml))) {
      $entry = $zip.CreateEntry($item[0], [System.IO.Compression.CompressionLevel]::Optimal)
      $sw = [System.IO.StreamWriter]::new($entry.Open(), [System.Text.UTF8Encoding]::new($false))
      $sw.Write($item[1]); $sw.Dispose()
    }
    $zip.Dispose(); $fs.Dispose()
    Write-Output 'fallback docx written'
  } catch { Write-Output "fallback docx failed: $($_.Exception.Message)" }
}

Write-Output '--- data folder ---'
Get-ChildItem $dir -File | Select-Object Name, Length