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
	LDLIBS  = -lws2_32
else
	LDLIBS  = 
endif

TARGET   = $(BUILDDIR)/$(PROJECT_NAME)$(EXE_EXT)

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

# --- Rules ---
all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)  

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	$(MKDIR) $@

clean:
ifeq ($(IS_WINDOWS),1)
	cmd /c "if exist $(BUILDDIR) (del /f /q $(subst /,\,$(BUILDDIR))\*.*)"
else
	$(RM) $(BUILDDIR)/*.o $(TARGET)
endif

.PHONY: all clean   