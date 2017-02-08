.SILENT:

##################### Porject Directory
PRODIR       = /sambaShare/sample/Unication
PRODIR_OUT   = $(PRODIR)/WorkSpace
PRODIR_CODE  = $(PRODIR)/Project_Code
PRODIR_THIRD = $(PRODIR)/ThirdParty

PRO_TARGET_SERVER = server
PROSERVER_CODE = $(PRODIR_CODE)/kSocketTestServer
PROSERVER_OUT_DIR  = $(PRODIR_OUT)/kSocketDebug

PRO_TARGET_CLIENT = client
PROCLIENT_CODE = $(PRODIR_CODE)/kSocketTestClient
PROCLIENT_OUT_DIR  = $(PRODIR_OUT)/kSocketDebug

##################### Source Code & library
INCPATH     = -I$(PRODIR_CODE)/kSocket

OBJS        = $(PRODIR_CODE)/kSocket/kSocket.o 

CLIENT_OBJS = $(OBJS) \
              $(PROCLIENT_CODE)/main.o

SERVER_OBJS = $(OBJS) \
              $(PROSERVER_CODE)/main.o

LIBPATH = 
LIBS    = -lpthread 

##################### Compiler and options
ASM      = nm 
ASMFLAGS = -f elf -O2 

CC       = arm-linux-gcc
CFLAGS   = -Wall -ansi -pedantic -O2

CXX      = arm-linux-g++
CXXFLAGS = -Wall -O2


############################ command
all: $(PRO_TARGET_CLIENT) $(PRO_TARGET_SERVER)


$(PRO_TARGET_CLIENT): $(CLIENT_OBJS)
	@echo -e "\033[036mCompiling $@\033[0m"
	$(CXX) -o $(PROCLIENT_OUT_DIR)/$@ $(CXXFLAGS) $(CLIENT_OBJS) $(LIBS) $(LIBPATH)

$(PRO_TARGET_SERVER): $(SERVER_OBJS)
	@echo -e "\033[036mCompiling $@\033[0m"
	$(CXX) -o $(PROSERVER_OUT_DIR)/$@  $(CXXFLAGS) $(SERVER_OBJS) $(LIBS) $(LIBPATH)

%.o : %.cpp
	@echo -e "\033[036mCompiling CXX $@\033[0m"
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

%.o : %.c
	@echo -e "\033[036mCompiling CC $@\033[0m"
	$(CC) -c $(CFLAGS) $(INCPATH) -o $@ $<
        
%.o : %.s
	@echo -e "\033[036mCompiling ASM $@\033[0m"
	$(ASM) $(ASMFLAGS) $< 

depend:
	g++ -E -MM $(CODEPATH)*.cpp $(CFLAGS) $(INCPATH) > .depend

clean:
	rm -rf $(PROSERVER_CODE)/*.o
	rm -rf $(PROSERVER_OUT_DIR)/$(PRO_TARGET_SERVER)
	rm -rf $(PROCLIENT_CODE)/*.o
	rm -rf $(PROCLIENT_OUT_DIR)/$(PRO_TARGET_CLIENT)
	rm -rf $(OBJS)
  

