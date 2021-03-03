all: xwebview

.PHONY: xwebview
xwebview:
	$(MAKE) -C server

clean:
	if [ -e server/xwebview ]; then /bin/rm server/xwebview; fi
