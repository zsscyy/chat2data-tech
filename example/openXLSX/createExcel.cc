#include <iostream>
#include <OpenXLSX/OpenXLSX.hpp>

int main(){
    try{
        // 1. 创建Excel文件
        OpenXLSX::XLDocument doc;
        doc.create("studentInfo.xlsx", true);

        // 2. 获取工作簿
        OpenXLSX::XLWorkbook workbook = doc.workbook();
        std::cout<<"worksheet的个数:"<<workbook.sheetCount()<<std::endl;
        
        
        // 3. 获取工作表(Excel文件创建好之后，在工作簿内部包含一个默认的工作表，该工作表的名称sheet1)
        OpenXLSX::XLWorksheet worksheet = workbook.worksheet("Sheet1");
        
        // 4. 修改工作表名称
         std::cout<<"worksheet的名称:"<<worksheet.name()<<std::endl;
        worksheet.setName("1班");
        std::cout<<"worksheet的名称:"<<worksheet.name()<<std::endl;

        // 5. 向工作表中插入数据
        // 添加表头
        worksheet.cell("A1").value() = "姓名";
        worksheet.cell("B1").value() = "学号";
        worksheet.cell("C1").value() = "年龄";
        worksheet.cell("D1").value() = "成绩";

        // 添加数据
        // 学生1
        worksheet.cell("A2").value() = "张三";
        worksheet.cell("B2").value() = "1001";
        worksheet.cell("C2").value() = "18";
        worksheet.cell("D2").value() = "90";

        // 学生2
        worksheet.cell("A3").value() = "李四";
        worksheet.cell("B3").value() = "1002";
        worksheet.cell("C3").value() = "18";
        worksheet.cell("D3").value() = "95";

        // 学生3
        worksheet.cell("A4").value() = "王五";
        worksheet.cell("B4").value() = "1003";
        worksheet.cell("C4").value() = "18";
        worksheet.cell("D4").value() = "85";

        // 再添加一个2班学生的信息--直接克隆1班的worksheet
        workbook.cloneSheet("1班", "2班");
        std::cout<<"worksheet的个数:"<<workbook.worksheetCount()<<std::endl;
        if(workbook.worksheetExists("2班")){
            // 修改最后一名学生信息
            OpenXLSX::XLWorksheet worksheet2 = workbook.worksheet("2班");
            int lastRow = worksheet2.rowCount();
            worksheet2.cell(lastRow, 1).value() = "赵六";
            worksheet2.cell(lastRow, 2).value() = "1004";
            worksheet2.cell(lastRow, 3).value() = "18";
            worksheet2.cell(lastRow, 4).value() = "95";
        }

        // 6. 保存Excel文件
        doc.save();
        // 7. 关闭Excel文件
        doc.close();
    }catch(const OpenXLSX::XLException& e){
        std::cerr<<"Error: "<<e.what()<<std::endl;
        return 1;
    }
    
    return 0;
}