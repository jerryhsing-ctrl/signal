CXX ?= g++
# 基礎編譯參數
CXXFLAGS = -std=c++20 -Wall -pthread -I./helper -I./QuoteSv -I./StrategySv -I./Order -I/usr/local/include -I.
CXXFLAGS += -DCFG_FILE=\"./cfg/HWStratSv.cfg\"

# 基礎連結參數
LDFLAGS = -pthread
SDKROOT := $(shell xcrun --show-sdk-path 2>/dev/null)
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
	CXX := clang++
	CXXFLAGS += -stdlib=libc++ -isystem $(SDKROOT)/usr/include/c++/v1 -isysroot $(SDKROOT)
	LDFLAGS += -stdlib=libc++ -isysroot $(SDKROOT)
endif

# 依賴檔設定
DEPFLAGS = -MMD -MP

# ASan (AddressSanitizer) 專用參數
# -fno-omit-frame-pointer 對於顯示準確的堆疊很重要
DEBUG_FLAGS = -fsanitize=address -g -O0 -fno-omit-frame-pointer

# 路徑設定
BUILDDIR = build
EXEC_ROOT = ../../exec
TARGET = $(BUILDDIR)/HWStratSv

# 搜尋原始碼
SRCDIRS = . QuoteSv StrategySv helper Order
SRCS := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.cpp))
OBJS := $(patsubst %.cpp,$(BUILDDIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

.PHONY: all run asan debug clean kill

all: $(TARGET)

# 連結規則
$(TARGET): $(OBJS)
	@echo "🔗 連結 $@"
	@mkdir -p $(dir $@)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)
	@mkdir -p $(EXEC_ROOT)/sv
	@mkdir -p $(EXEC_ROOT)/log
	@cp -f $@ $(EXEC_ROOT)/sv/signal

# 編譯規則
$(BUILDDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "🔨 編譯 $<"
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# 一般執行 (不含 ASan)
run: $(TARGET)
	cd $(EXEC_ROOT) && time ./sv/signal

# ==========================================
# 🔥 核心功能：ASan 記憶體檢測模式
# ==========================================
# 使用 Target-specific variables 來注入旗標
asan: CXXFLAGS += $(DEBUG_FLAGS)
asan: LDFLAGS += $(DEBUG_FLAGS)
asan: clean $(TARGET)
	@echo "🚑 [ASan 模式] 正在啟動記憶體偵測..."
	@echo "請等待程式崩潰並觀察紅色錯誤訊息..."
	cd $(EXEC_ROOT) && ./sv/signal

# GDB 除錯模式 (不含 ASan，避免干擾)
debug: CXXFLAGS += -g -O0
debug: clean $(TARGET)
	cd $(EXEC_ROOT) && gdb ./sv/signal

clean:
	rm -rf $(BUILDDIR)

kill:
	pkill -f signal

-include $(DEPS)
