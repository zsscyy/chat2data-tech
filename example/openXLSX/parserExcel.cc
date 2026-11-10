#include <iostream>
#include <OpenXLSX/OpenXLSX.hpp>
#include <vector>
#include <string>



// 需求：解析studentInof.xlsx文件
//     1. 解析出所有worksheet的名称 及 类型
//     2. 解析2班的worksheet中所有的数据---先解析表头，后解析表中数据行数据
int main(){
    try{
        // 1. 创建文档对象，并打开Excel文件
        OpenXLSX::XLDocument doc;
        doc.open("studentInfo.xlsx");

        // 2. 获取工作簿，通过工作簿获取所有的worksheet名称
        OpenXLSX::XLWorkbook workbook = doc.workbook();
        std::cout<<"worksheet的个数:"<<workbook.sheetCount()<<std::endl;
        
        // 3. 打印worksheet名以及worksheet的类型
        std::vector<std::string> worksheetNames = workbook.worksheetNames();
        for(auto& name : worksheetNames){
            std::cout<<"worksheet的名称:"<<name<<std::endl;
            if(workbook.typeOfSheet(name) == OpenXLSX::XLSheetType::Worksheet){
                std::cout<<"worksheet的类型:"<<"WorkSheetType"<<std::endl;
            }else{
                std::cout<<"worksheet的类型:"<<"其他类型"<<std::endl;
            }
        }

        // 4. 解析2班worksheet中的数据
        OpenXLSX::XLWorksheet worksheet2 = workbook.worksheet("2班");
        std::cout<<"worksheet2的名称:"<<worksheet2.name()<<std::endl;

        // 4.1 先解析表头
        std::cout<<"worksheet2的表头:"<<std::endl;
        for(int i = 1; i <= worksheet2.columnCount(); i++){
            std::cout<<worksheet2.cell(1, i).value().getString()<<"    ";
        }
        std::cout<<std::endl;

        // 4.2 解析表中数据行数据
        std::cout<<"worksheet2的表中数据行数据:"<<std::endl;
        for(int i = 2; i <= worksheet2.rowCount(); i++){
            for(int j = 1; j <= worksheet2.columnCount(); j++){
                std::cout<<worksheet2.cell(i, j).value().getString()<<"    ";
            }
            std::cout<<std::endl;
        }
        
        // 5. 关闭Excel文件
        doc.close();
    }catch(const OpenXLSX::XLException& e){
        std::cout<<"异常信息:"<<e.what()<<std::endl;
        return 1;
    }
    return 0;
}