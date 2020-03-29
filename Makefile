TARGET=wlroots-screen-record

SRCDIR=src/
OBJDIR=obj/

CC=gcc
CFLAGS=-c -O2 -Wall -Wno-unused-function -Wshadow
LDFLAGS=-lavformat -lavcodec -lavutil -lm -lswresample -lswscale -lrt -lwayland-client -lwlroots

SRCS=$(wildcard $(SRCDIR)*.c)
OBJS=$(addprefix $(OBJDIR),$(notdir $(SRCS:.c=.o)))

.PHONY: all
all: $(OBJDIR) $(TARGET) venv

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OBJDIR)%.o: $(SRCDIR)%.c
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -rf $(TARGET) $(OBJDIR) .venv

.PHONY: venv
venv:
	python3 -m venv .venv
	sh -c '. .venv/bin/activate; pip install -r requirements.txt'

.PHONY: run
run: all
	sh -c '. .venv/bin/activate; python ./dlna-screencast.py'
