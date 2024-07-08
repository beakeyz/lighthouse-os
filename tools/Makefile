
TOOLS := \
  ./base_env \
	
build: $(TOOLS)
# Builds all the tools we have
$(TOOLS):
	@make -C $@ build

.PHONY: build $(TOOLS)
