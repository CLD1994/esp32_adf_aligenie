LIBS += aligenie speex ic enc librws

COMPONENT_ADD_LDFLAGS += -L$(COMPONENT_PATH)/lib/ \
						  $(addprefix -l,$(LIBS))

COMPONENT_ADD_INCLUDEDIRS += ../audio_recorder

