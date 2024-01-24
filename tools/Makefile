
TOOLS := \
  ./base_env \
  ./check_rootdir
	
build: $(TOOLS)
# Builds all the tools we have
$(TOOLS):
	@make -C $@ build

.PHONY: build $(TOOLS)
