################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../gzip_fpga.cpp 

OBJS += \
./gzip_fpga.o 

CPP_DEPS += \
./gzip_fpga.d 

# Define QPSDKINCLUDE reading QuickPlaySDK environment variable
# or, if not defined, checking default OS headers folder (/opt/accelize/quickapps/sdk/include)
ifeq ($(shell echo $$QuickPlaySDK),)
	ifneq "$(wildcard /opt/accelize/quickapps/sdk/include )" ""
		QPSDKINCLUDEDIR := "/opt/accelize/quickapps/sdk/include"
		
	else
		QPSDKINCLUDEDIR := 
	endif
else
	QPSDKINCLUDEDIR := "${QuickPlaySDK}/include"
endif

QPSDKINCLUDE = $(shell find $(QPSDKINCLUDEDIR) -type d | sed s/^/" -I"/)

# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ $(QPSDKINCLUDE) -std=c++0x -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '




