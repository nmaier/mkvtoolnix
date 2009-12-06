all_with_tools:
	+make -C .. TOOLS=1

APPLICATIONS += src/tools/base64tool$(EXEEXT) src/tools/diracparser$(EXEEXT) src/tools/vc1parser$(EXEEXT) src/tools/ebml_validator$(EXEEXT)
ALL_SOURCES  += $(wildcard src/tools/*.cpp)

vc1parser_SOURCES = src/tools/vc1parser.cpp
vc1parser_OBJECTS = $(patsubst %.cpp,%.o,$(vc1parser_SOURCES))
vc1parser_DEPENDENCIES = $(DEP_COMMON)
vc1parser_LDADD = -lmtxcommon $(MAGIC_LIBS) -lmatroska -lebml \
		-lz $(COMPRESSION_LIBRARIES) -lexpat $(ICONV_LIBS) \
		$(LIBINTL_LIBS) $(LIBRPCRT) $(BOOST_REGEX_LIB)

diracparser_SOURCES = src/tools/diracparser.cpp
diracparser_OBJECTS = $(patsubst %.cpp,%.o,$(diracparser_SOURCES))
diracparser_DEPENDENCIES = $(DEP_COMMON)
diracparser_LDADD = -lmtxcommon $(MAGIC_LIBS) -lmatroska -lebml \
		-lz $(COMPRESSION_LIBRARIES) -lexpat $(ICONV_LIBS) \
		$(LIBINTL_LIBS) $(LIBRPCRT) $(BOOST_REGEX_LIB)

base64tool_SOURCES = src/tools/base64tool.cpp
base64tool_OBJECTS = $(patsubst %.cpp,%.o,$(base64tool_SOURCES))
base64tool_DEPENDENCIES += $(DEP_COMMON)
base64tool_LDADD = -lmtxcommon $(MAGIC_LIBS) -lmatroska -lebml -lexpat $(ICONV_LIBS) \
		$(LIBINTL_LIBS) $(LIBRPCRT) $(BOOST_REGEX_LIB)

ebml_validator_SOURCES = src/tools/ebml_validator.cpp src/tools/element_info.cpp
ebml_validator_OBJECTS = $(patsubst %.cpp,%.o,$(ebml_validator_SOURCES))
ebml_validator_DEPENDENCIES = $(DEP_COMMON)
ebml_validator_LDADD = -lmtxcommon $(MAGIC_LIBS) -lmatroska -lebml \
		-lz $(COMPRESSION_LIBRARIES) -lexpat $(ICONV_LIBS) \
		$(LIBINTL_LIBS) $(LIBRPCRT) $(BOOST_REGEX_LIB)


base64tool: src/tools/base64tool$(EXEEXT)

src/tools/base64tool$(EXEEXT): $(base64tool_OBJECTS) $(base64tool_DEPENDENCIES)
	@echo '    LINK ' $@
	$(Q)$(LINK) -o $@ $(base64tool_OBJECTS) $(base64tool_LDADD)

diracparser: src/tools/diracparser$(EXEEXT)

src/tools/diracparser$(EXEEXT): $(diracparser_OBJECTS) $(diracparser_DEPENDENCIES)
	@echo '    LINK ' $@
	$(Q)$(LINK) -o $@ $(diracparser_OBJECTS) $(diracparser_LDADD)

vc1parser: src/tools/vc1parser$(EXEEXT)

src/tools/vc1parser$(EXEEXT): $(vc1parser_OBJECTS) $(vc1parser_DEPENDENCIES)
	@echo '    LINK ' $@
	$(Q)$(LINK) -o $@ $(vc1parser_OBJECTS) $(vc1parser_LDADD)

ebml_validator: src/tools/ebml_validator$(EXEEXT)

src/tools/ebml_validator$(EXEEXT): $(ebml_validator_OBJECTS) $(ebml_validator_DEPENDENCIES)
	@echo '    LINK ' $@
	$(Q)$(LINK) -o $@ $(ebml_validator_OBJECTS) $(ebml_validator_LDADD)
