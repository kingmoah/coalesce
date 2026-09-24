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

CFLAGS   = -Wall -O2

ifeq ($(IS_WINDOWS),1)
	LDLIBS  = -lws2_32 -ladvapi32
else
	LDLIBS  =
endif

TARGET   = $(BUILDDIR)/$(PROJECT_NAME)$(EXE_EXT)

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

# --- Rules ---
all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	$(MKDIR) $@

# --- Tests ---
TEST1_TARGET = $(BUILDDIR)/test_phase1$(EXE_EXT)
TEST2_TARGET = $(BUILDDIR)/test_phase2$(EXE_EXT)
TEST3_TARGET = $(BUILDDIR)/test_phase3$(EXE_EXT)
TEST4_TARGET = $(BUILDDIR)/test_phase4$(EXE_EXT)

test: $(TEST1_TARGET) $(TEST2_TARGET) $(TEST3_TARGET) $(TEST4_TARGET)
	./$(TEST1_TARGET)
	./$(TEST2_TARGET)
	./$(TEST3_TARGET)
	./$(TEST4_TARGET)

$(TEST1_TARGET): $(BUILDDIR)/buffer.o tests/test_phase1.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST2_TARGET): $(BUILDDIR)/buffer.o $(BUILDDIR)/rand.o $(BUILDDIR)/sha256.o $(BUILDDIR)/sha512.o $(BUILDDIR)/curve25519.o $(BUILDDIR)/ed25519.o $(BUILDDIR)/aes.o $(BUILDDIR)/kex.o $(BUILDDIR)/base64.o tests/test_phase2.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(BUILDDIR)/buffer.o $(BUILDDIR)/rand.o $(BUILDDIR)/sha256.o $(BUILDDIR)/sha512.o $(BUILDDIR)/curve25519.o $(BUILDDIR)/ed25519.o $(BUILDDIR)/aes.o $(BUILDDIR)/kex.o $(BUILDDIR)/base64.o tests/test_phase2.c $(LDLIBS)

$(TEST3_TARGET): $(filter $(BUILDDIR)/%.o,$(OBJS)) tests/test_phase3.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(filter $(BUILDDIR)/%.o,$(OBJS)) tests/test_phase3.c $(LDLIBS)

$(TEST4_TARGET): $(filter $(BUILDDIR)/%.o,$(OBJS)) tests/test_phase4.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $(filter $(BUILDDIR)/%.o,$(OBJS)) tests/test_phase4.c $(LDLIBS)

# --- Clean ---
clean:
ifeq ($(IS_WINDOWS),1)
	cmd /c "if exist $(BUILDDIR) (del /f /q $(subst /,\,$(BUILDDIR))\*.*)"
else
	$(RM) $(BUILDDIR)/*.o $(TARGET) $(TEST1_TARGET) $(TEST2_TARGET) $(TEST3_TARGET) $(TEST4_TARGET)
endif

.PHONY: all clean test
