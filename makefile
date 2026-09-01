# --- OS detection ---
ifeq ($(OS),Windows_NT)
    IS_WINDOWS := 1
endif

# --- OS-specific commands ---
ifeq ($(IS_WINDOWS),1)
    EXE_EXT := .exe
    RM      := del /f /q
    MKDIR   := mkdir
else
    EXE_EXT :=
    RM      := rm -f
    MKDIR   := mkdir -p
endif

# --- Project config ---
PROJECT_NAME = coalesce
CC       = gcc
SRCDIR   = src
BUILDDIR = build

# --- mbedtls vendor paths ---
MBEDTLS_DIR   = vendor/mbedtls
MBEDTLS_BUILD = $(MBEDTLS_DIR)/build
MBEDTLS_LIB   = $(MBEDTLS_BUILD)/tf-psa-crypto/library/libmbedcrypto.a
MBEDTLS_INC   = -I$(MBEDTLS_DIR)/include \
                -I$(MBEDTLS_DIR)/tf-psa-crypto/include \
                -I$(MBEDTLS_DIR)/tf-psa-crypto/drivers/builtin/include

CFLAGS   = -Wall -O2 $(MBEDTLS_INC)

ifeq ($(IS_WINDOWS),1)
	LDLIBS  = -lws2_32 -lbcrypt
else
	LDLIBS  =
endif

TARGET   = $(BUILDDIR)/$(PROJECT_NAME)$(EXE_EXT)

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

# --- Rules ---
all: $(TARGET)

$(TARGET): $(OBJS) $(MBEDTLS_LIB) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(MBEDTLS_LIB) $(LDLIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	$(MKDIR) $@

# --- Tests ---
TEST1_TARGET = $(BUILDDIR)/test_phase1$(EXE_EXT)
TEST2_TARGET = $(BUILDDIR)/test_phase2$(EXE_EXT)

test: $(TEST1_TARGET) $(TEST2_TARGET)
	./$(TEST1_TARGET)
	./$(TEST2_TARGET)

$(TEST1_TARGET): $(BUILDDIR)/buffer.o tests/test_phase1.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST2_TARGET): $(BUILDDIR)/buffer.o $(BUILDDIR)/sha256.o $(BUILDDIR)/curve25519.o $(BUILDDIR)/aes.o $(BUILDDIR)/kex.o tests/test_phase2.c $(MBEDTLS_LIB) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(BUILDDIR)/buffer.o $(BUILDDIR)/sha256.o $(BUILDDIR)/curve25519.o $(BUILDDIR)/aes.o $(BUILDDIR)/kex.o tests/test_phase2.c $(MBEDTLS_LIB) $(LDLIBS)

# --- Vendor bootstrap (run once after git submodule update --init) ---
vendor-build:
	cmake -S $(MBEDTLS_DIR) -B $(MBEDTLS_BUILD) \
	      -DCMAKE_BUILD_TYPE=Release \
	      -DENABLE_TESTING=OFF \
	      -DENABLE_PROGRAMS=OFF
	cmake --build $(MBEDTLS_BUILD) --target mbedcrypto

# --- Clean ---
clean:
ifeq ($(IS_WINDOWS),1)
	cmd /c "if exist $(BUILDDIR) (del /f /q $(subst /,\,$(BUILDDIR))\*.*)"
else
	$(RM) $(BUILDDIR)/*.o $(TARGET) $(TEST1_TARGET) $(TEST2_TARGET)
endif

.PHONY: all clean test vendor-build