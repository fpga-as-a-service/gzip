################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Get current OS type (i.e. x86_64)
UNAMEM := $(shell uname -m)

# Define QPSDKLIB reading QuickPlaySDK environment variable
# or, if not defined, checking default OS libraries folder (/opt/accelize/quickapps/sdk/debug/x86_64)
ifeq ($(shell echo $$QuickPlaySDK),)
	ifneq "$(wildcard /opt/accelize/quickapps/sdk/debug/$(UNAMEM) )" ""
		QPSDKLIB := -L"/opt/accelize/quickapps/sdk/debug/$(UNAMEM)"
	else
		QPSDKLIB := 
	endif
else
	QPSDKLIB := -L"${QuickPlaySDK}/debug/$(UNAMEM)"
endif

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: gzip_fpga

# Tool invocations
gzip_fpga: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C++ Linker'
	g++ $(QPSDKLIB) -g -o "gzip_fpga" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C++_DEPS)$(C_DEPS)$(CC_DEPS)$(CPP_DEPS)$(EXECUTABLES)$(CXX_DEPS)$(C_UPPER_DEPS) gzip_fpga
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets


