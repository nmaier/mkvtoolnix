all_with_tools:
	make -C .. TOOLS=1

APPLICATIONS += src/tools/vc1parser$(EXEEXT) src/tools/base64tool$(EXEEXT)
ALL_SOURCES  += $(wildcard src/tools/*.cpp)
MANPAGES     +=  doc/base64tool.1

vc1parser_SOURCES = src/tools/vc1parser.cpp
vc1parser_OBJECTS = $(patsubst %.cpp,%.o,$(vc1parser_SOURCES))
vc1parser_DEPENDENCIES = $(DEP_COMMON)
vc1parser_LDADD = -lmtxcommon $(MAGIC_LIBS) -lmatroska -lebml \
		-lz $(COMPRESSION_LIBRARIES) -lexpat $(ICONV_LIBS) \
		$(LIBINTL_LIBS) $(LIBRPCRT)

base64tool_SOURCES = src/tools/base64tool.cpp
base64tool_OBJECTS = $(patsubst %.cpp,%.o,$(base64tool_SOURCES))
base64tool_DEPENDENCIES += $(DEP_COMMON)
base64tool_LDADD = -lmtxcommon $(MAGIC_LIBS) -lexpat $(ICONV_LIBS) $(LIBINTL_LIBS) $(LIBRPCRT)


vc1parser: src/tools/vc1parser$(EXEEXT)

src/tools/vc1parser$(EXEEXT): $(vc1parser_OBJECTS) $(vc1parser_DEPENDENCIES)
	@echo '    LINK ' $@
	$(Q)$(LINK) -o $@ $(vc1parser_OBJECTS) $(vc1parser_LDADD)

base64tool: src/tools/base64tool$(EXEEXT)

src/tools/base64tool$(EXEEXT): $(base64tool_OBJECTS) $(base64tool_DEPENDENCIES)
	@echo '    LINK ' $@
	$(Q)$(LINK) -o $@ $(base64tool_OBJECTS) $(base64tool_LDADD)

