nobase_fluxionpy_PYTHON = \
	jsongraph/objects/__init__.py \
	jsongraph/objects/edge.py \
	jsongraph/objects/graph.py \
	jsongraph/objects/multigraph.py \
	jsongraph/objects/node.py

clean-local:
	-rm -f *.pyc */*.pyc *.pyo */*.pyo
	-rm -rf __pycache__ */__pycache__

install-data-hook:
	$(AM_V_at)echo Linking fluxion py in non-standard location... && \
	  $(INSTALL) -d -m 0755 "$(DESTDIR)$(fluxionpylinkdir)" && \
	  target=$(fluxionpydir) && \
	  f=$${target##*/} && \
	  cd "$(DESTDIR)$(fluxionpylinkdir)" && \
	  rm -f $$f && \
	  $(LN_S) $$target .

uninstall-local:
	$(AM_V_at)target=$(fluxionpydir) && f=$${target##*/} && \
	  echo "Removing $(fluxionpylinkdir)/$$f" && \
	  rm -rf $(fluxionpylinkdir)/$$f
