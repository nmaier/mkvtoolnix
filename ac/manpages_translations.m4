AC_MSG_CHECKING(the manpage translation languages to install)
MANPAGES_TRANSLATIONS_POS="`echo $srcdir/doc/man/po4a/po/*.po`"
MANPAGES_TRANSLATIONS="`echo "$MANPAGES_TRANSLATIONS_POS" | \
  sed -e 's/\S*\/\(\w*\).po/\1/g'`"
AC_MSG_RESULT($MANPAGES_TRANSLATIONS)

MANPAGES_TRANSLATED="`for lang in $MANPAGES_TRANSLATIONS; do \
  echo '$(subst doc/man, doc/man/'$lang', $(MANPAGES))'; done`"

MANPAGES_TRANSLATED_XML_RULE="`for lang in $MANPAGES_TRANSLATIONS; do \
  echo "doc/man/$lang/%.xml: doc/man/%.xml doc/man/po4a/po/$lang.po"
  echo "	@echo '    PO4A ' \$<"
  echo '	$(Q)$(PO4A_TRANSLATE) $(PO4A_TRANSLATE_FLAGS) -m $< -p '"doc/man/po4a/po/$lang.po "'-l $@';done`"


AC_SUBST(MANPAGES_TRANSLATIONS)
AC_SUBST(MANPAGES_TRANSLATIONS_POS)
AC_SUBST(MANPAGES_TRANSLATED)
AC_SUBST(MANPAGES_TRANSLATED_XML_RULE)
