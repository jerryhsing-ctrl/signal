# ==========================================
#  GCC 13 專用 Makefile (for udpserver)
# ==========================================

# 1. 定義你的 GCC 13 安裝路徑
GCC_HOME = /home/hwuser/GCC-13

# 2. 指定使用新版編譯器
CXX = g++

# 3. 編譯參數 (C++11)
CXXFLAGS = -std=c++11

# 4. 連結參數 (關鍵！告訴程式執行時去哪裡找 libstdc++.so)
LDFLAGS = -Wl,-rpath,$(GCC_HOME)/lib64

# 5. 檔案名稱
TARGET = udpserver
SRC    = udpserver.cpp IniReader.cpp
DATA   = ./udp.1119

# ------------------------------------------

# 預設目標：只編譯
all: $(TARGET)

# 編譯規則
$(TARGET): $(SRC)
	@echo "🔨 正在使用 GCC 13 編譯..."
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC)
	@echo "✅ 編譯完成！"

# 執行目標 (make run)
run: $(TARGET)
	@echo "🚀 正在執行..."
	./$(TARGET) $(DATA)

# 清理目標 (make clean)
clean:
	@echo "🧹 清理舊檔案..."
	rm -f $(TARGET)

