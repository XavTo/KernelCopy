# utils/Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2

all: otp_test timeotp_test

otp_test: otp_test.c
	$(CC) $(CFLAGS) -o otp_test otp_test.c

timeotp_test: timeotp_test.c
	$(CC) $(CFLAGS) -o timeotp_test timeotp_test.c

clean:
	rm -f otp_test timeotp_test
