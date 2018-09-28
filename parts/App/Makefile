SGX_SDK ?= /path/to/the/sgxsdk
SGX_MODE ?= HW
SGX_ARCH ?= x64
SGX_DEBUG ?= 1

ifeq ($(SGX_SDK),/path/to/the/sgxsdk)
    $(error "SGX_SDK is not set ")
endif

ifeq ($(shell getconf LONG_BIT), 32)
	SGX_ARCH := x86
else ifeq ($(findstring -m32, $(CXXFLAGS)), -m32)
	SGX_ARCH := x86
endif

ifeq ($(SGX_ARCH), x86)
	SGX_COMMON_CFLAGS := -m32
	SGX_LIBRARY_PATH := $(SGX_SDK)/lib
	SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x86/sgx_sign
	SGX_EDGER8R := $(SGX_SDK)/bin/x86/sgx_edger8r
else
	SGX_COMMON_CFLAGS := -m64
	SGX_LIBRARY_PATH := $(SGX_SDK)/lib64
	SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x64/sgx_sign
	SGX_EDGER8R := $(SGX_SDK)/bin/x64/sgx_edger8r
endif

ifeq ($(SGX_DEBUG), 1)
ifeq ($(SGX_PRERELEASE), 1)
$(error Cannot set SGX_DEBUG and SGX_PRERELEASE at the same time!!)
endif
endif

ifeq ($(SGX_DEBUG), 1)
		SGX_COMMON_CFLAGS += -O2 -fdata-sections -ffunction-sections -Wl,--gc-sections
else
		SGX_COMMON_CFLAGS += -O2 -fdata-sections -ffunction-sections -Wl,--gc-sections
endif

######## App Settings ########

ifneq ($(SGX_MODE), HW)
	Urts_Library_Name := sgx_urts_sim
else
	Urts_Library_Name := sgx_urts
endif

# App_Cpp_Files := App/App.cpp $(wildcard App/Edger8rSyntax/*.cpp) $(wildcard App/TrustedLibrary/*.cpp)
App_C_Files := EActors/stack.c EActors/node.c EActors/socket.c EActors/cargo.c EActors/base64.c EActors/factory.c EActors/pack_memcpy.c EActors/pack_ctr.c EActors/pack_gcm.c
App_Cpp_Files := App.cpp sgx_utils/sgx_utils.cpp 
App_Include_Paths := -I. -I../ -I$(SGX_SDK)/include -I./EActors

App_C_Flags := $(SGX_COMMON_CFLAGS) -fPIC -Wno-attributes $(App_Include_Paths) -DUNTRUSTED

# Three configuration modes - Debug, prerelease, release
#   Debug - Macro DEBUG enabled.
#   Prerelease - Macro NDEBUG and EDEBUG enabled.
#   Release - Macro NDEBUG enabled.
ifeq ($(SGX_DEBUG), 1)
		App_C_Flags += -DDEBUG -UNDEBUG -UEDEBUG
else ifeq ($(SGX_PRERELEASE), 1)
		App_C_Flags += -DNDEBUG -DEDEBUG -UDEBUG
else
		App_C_Flags += -DNDEBUG -UEDEBUG -UDEBUG
endif

App_Cpp_Flags := $(App_C_Flags) -std=c++11 
App_Link_Flags := $(SGX_COMMON_CFLAGS) -L$(SGX_LIBRARY_PATH) -l$(Urts_Library_Name) -lpthread -lsgx_tcrypto

ifneq ($(SGX_MODE), HW)
	App_Link_Flags += -lsgx_uae_service_sim
else
	App_Link_Flags += -lsgx_uae_service
endif

App_C_Objects := $(App_C_Files:.c=.o)
App_Cpp_Objects := $(App_Cpp_Files:.cpp=.o)

App_Name := app

ifeq ($(SGX_MODE), HW)
ifneq ($(SGX_DEBUG), 1)
ifneq ($(SGX_PRERELEASE), 1)
Build_Mode = HW_RELEASE
endif
endif
endif


.PHONY: all run

ifeq ($(Build_Mode), HW_RELEASE)
all: $(App_Name) $(Enclave_Name)
	@echo "The project has been built in release hardware mode."
	@echo "Please sign the $(Enclave_Name) first with your signing key before you run the $(App_Name) to launch and access the enclave."
	@echo "To sign the enclave use the command:"
	@echo "   $(SGX_ENCLAVE_SIGNER) sign -key <your key> -enclave $(Enclave_Name) -out <$(Signed_Enclave_Name)> -config $(Enclave_Config_File)"
	@echo "You can also sign the enclave using an external signing tool. See User's Guide for more details."
	@echo "To build the project in simulation mode set SGX_MODE=SIM. To build the project in prerelease mode set SGX_PRERELEASE=1 and SGX_MODE=HW."
else
all: $(App_Name) $(Signed_Enclave_Name)
endif

run: all
ifneq ($(Build_Mode), HW_RELEASE)
	@$(CURDIR)/$(App_Name)
	@echo "RUN  =>  $(App_Name) [$(SGX_MODE)|$(SGX_ARCH), OK]"
endif

######## App Objects ########

Enclave_u.c: $(SGX_EDGER8R) ../Enclave.edl
	@$(SGX_EDGER8R) --untrusted ../Enclave.edl --search-path ../ --search-path $(SGX_SDK)/include
	@echo "GEN  =>  $@"

Enclave_u.o: Enclave_u.c
	@$(CC) $(App_C_Flags) -c $< -o $@
	@echo "CC   <=  $<"

%.o: %.cpp
	$(CXX) $(App_Cpp_Flags) -c $< -o $@
	@echo "CXX  <=  $<"

payload.o: payload.c
	$(CC) $(App_C_Flags) -c  $< -o $@
	@echo "CXX  <=  $<"

%.o: %.c
	$(CC) $(App_C_Flags) -c  $< -o $@
	@echo "CXX  <=  $<"


#what if the same file will generate .o for the app and the enclave folders?
EActors/%.o: EActors/%.cpp
	$(CC) $(App_C_Flags) -c  $< -o $@
	@echo "CC  <=  $<"

$(App_Name): Enclave_u.o $(App_Cpp_Objects) $(App_C_Objects) payload.o
	@$(CXX) $^ -o $@ $(App_Link_Flags)
	mv $(App_Name) ../
	@echo "LINK =>  $@"

.PHONY: clean

clean:
	@rm -f $(App_C_Objects) $(App_Cpp_Objects) Enclave_u.* $(App_Name) payload.o *.cpp.* ../$(App_Name)
