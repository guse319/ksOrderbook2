CXX := g++
CXXFLAGS_COMMON := -std=c++20 -Wall -Wextra -O2
LDFLAGS_COMMON :=

APPS := subscriber
BUILD_DIR := build

# App: subscriber
subscriber_SRCS := \
	ksSubscriber.cpp \
	core/ksOrderbook.cpp \
	core/ksLogger.cpp \
	core/ksSecretKey.cpp \
	core/ksWebsocket.cpp \
	core/ksUnifiedBook.cpp
subscriber_HDRS := \
	ksSubscriber.h \
	core/ksOrderbook.h \
	core/ksSecretKey.h \
	core/ksWebsocket.h
subscriber_CXXFLAGS := -I. -I/usr/include -I/usr/include/nlohmann
subscriber_LDFLAGS := -lboost_system -lpthread -lssl -lcrypto -lrt

define APP_template
$1_OBJS := $$(patsubst %.cpp,$$(BUILD_DIR)/$1/%.o,$$($1_SRCS))

$1: $$($1_OBJS)
	$$(CXX) $$(CXXFLAGS_COMMON) $$($1_CXXFLAGS) -o $$@ $$^ $$(LDFLAGS_COMMON) $$($1_LDFLAGS)

$$(BUILD_DIR)/$1/%.o: %.cpp $$($1_HDRS)
	@mkdir -p $$(@D)
	$$(CXX) $$(CXXFLAGS_COMMON) $$($1_CXXFLAGS) -c -o $$@ $$<
endef

$(foreach app,$(APPS),$(eval $(call APP_template,$(app))))

all: $(APPS)

clean:
	rm -rf $(BUILD_DIR) $(APPS)

.PHONY: all clean $(APPS)
