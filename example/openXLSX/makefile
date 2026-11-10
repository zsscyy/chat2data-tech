all: createExcel parserExcel
createExcel: createExcel.cc
	g++ -o createExcel createExcel.cc -lOpenXLSX
parserExcel: parserExcel.cc
	g++ -o parserExcel parserExcel.cc -lOpenXLSX
clear:
	rm -f createExcel parserExcel