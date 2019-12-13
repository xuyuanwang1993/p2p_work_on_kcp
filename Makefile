#Makefile

TARGET_APP = p2p_server
TARGET_APP2 = p2p_client
TARGET_APP3 = new_model_test
OUT_DIR = out
APP_DIR = examples
APP_SUFIX =.cpp
OBJS_PATH = $(OUT_DIR)/objs
ifndef CROSS_COMPILE
TIPS=You can run \"make\" command with the given CROSS_COMPILE like \"make -f Makefile.cross CROSS_COMPILE=/test/bin/\" \
default \"CROSS_COMPILE=/usr/bin/\"
else
TIPS=CROSS_COMPILE :$(CROSS_COMPILE)
endif
CROSS_COMPILE ?= /usr/bin/
CXX   = $(CROSS_COMPILE)g++
CC    = $(CROSS_COMPILE)gcc
STRIP = $(CROSS_COMPILE)strip

LD_FLAGS  = -lrt -lpthread -ldl -lm

CXX_FLAGS = -std=c++11

WORK_DIR = .\
        ./Net\
        ./p2p_punch_model \
        ./Tools\
	./Tests

SRC_FILE += $(patsubst %,%/*.c,$(WORK_DIR))
SRC_FILE += $(patsubst %,%/*.h,$(WORK_DIR))
SRC_FILE += $(patsubst %,%/*.cpp,$(WORK_DIR))

CPP_SOURCE =$(filter %.c %.cpp,$(wildcard $(SRC_FILE)))
INC = $(patsubst %,-I%,$(WORK_DIR))
ALL_OBJECTS = $(addprefix $(OBJS_PATH)/,$(addsuffix .o,$(basename $(notdir $(CPP_SOURCE)))))

all: prepare BUILD_DIR $(TARGET_APP) $(TARGET_APP2) $(TARGET_APP3)

.PHONY : prepare
prepare:
	@echo -e "\033[33m$(TIPS)\033[0m"
	@echo CPP_SOURCE :$(CPP_SOURCE)
	@echo INC :$(INC)
	@echo ALL_OBJECTS :$(ALL_OBJECTS)

BUILD_DIR:
	@-mkdir -p $(OUT_DIR)
	@-mkdir -p $(OBJS_PATH)
	@echo -e "\033[32mIt's creating object files,please wait some seconds!\033[0m"
$(TARGET_APP) : $(ALL_OBJECTS)
	$(CXX) $^ $(APP_DIR)/$@$(APP_SUFIX) $(INC) -o $(OUT_DIR)/$@ $(CFLAGS) $(LD_FLAGS) $(CXX_FLAGS)
$(TARGET_APP2) : $(ALL_OBJECTS)
	$(CXX) $^ $(APP_DIR)/$@$(APP_SUFIX) $(INC) -o $(OUT_DIR)/$@ $(CFLAGS) $(LD_FLAGS) $(CXX_FLAGS)
$(TARGET_APP3) : $(ALL_OBJECTS)
	$(CXX) $^ $(APP_DIR)/$@$(APP_SUFIX) $(INC) -o $(OUT_DIR)/$@ $(CFLAGS) $(LD_FLAGS) $(CXX_FLAGS)
$(OBJS_PATH)/%.o : ./Net/%.cpp
	@$(CXX) -c $< -o $@ $(CXX_FLAGS) $(INC)
$(OBJS_PATH)/%.o : ./p2p_punch_model/%.cpp
	@$(CXX) -c $< -o  $@  $(CXX_FLAGS) $(INC)
$(OBJS_PATH)/%.o : ./Tools/%.cpp
	@$(CXX) -c $< -o  $@  $(CXX_FLAGS) $(INC)
$(OBJS_PATH)/%.o : ./Tests/%.cpp
	@$(CXX) -c $< -o  $@  $(CXX_FLAGS) $(INC)
$(OBJS_PATH)/%.o : ./%.cpp
	@$(CXX) -c $< -o $@ $(CXX_FLAGS) $(INC)

$(OBJS_PATH)/%.o : ./Net/%.c
	@$(CXX) -c  $< -o  $@  $(CXX_FLAGS) $(INC)
$(OBJS_PATH)/%.o : ./p2p_punch_model/%.c
	@$(CXX) -c  $< -o  $@  $(CXX_FLAGS) $(INC)
$(OBJS_PATH)/%.o : ./Tools/%.c
	@$(CXX) -c  $< -o  $@  $(CXX_FLAGS) $(INC)
$(OBJS_PATH)/%.o : ./%.c
	@$(CXX) -c  $< -o  $@  $(CXX_FLAGS) $(INC)

install :
	@echo -e "\033[31mERROR:install is't supported yet!\033[0m"

clean :
	-rm -rf $(OUT_DIR)
