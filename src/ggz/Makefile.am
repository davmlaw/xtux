SUBDIRS=easysock server client

EXTRA_DIST= xtux.dsc xtux.dsc xtuxclient.room


install-data-local:
	@$(NORMAL_INSTALL)
	$(GGZ_CONFIG) -D --install --fromfile=$(srcdir)/xtuxclient.dsc --force

uninstall:
	@$(NORMAL_UNINSTALL)
	-$(GGZ_CONFIG) --remove --fromfile=$(srcdir)/xtuxclient.dsc
