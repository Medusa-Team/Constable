/**
 * @file mlibc.c
 * @short Mini libc for linking with force code objects.
 * (c)1999-2000 by Martin Ockajak
 * $Id: mlibc.c,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include "mlibc.h"


/* Much of this stuff is borrowed from the kernel */

static int errno = 0;

unsigned char _ctype[] = {
    _C, _C, _C, _C, _C, _C, _C, _C,	/* 0-7 */
    _C, _C | _S, _C | _S, _C | _S, _C | _S, _C | _S, _C, _C,	/* 8-15 */
    _C, _C, _C, _C, _C, _C, _C, _C,	/* 16-23 */
    _C, _C, _C, _C, _C, _C, _C, _C,	/* 24-31 */
    _S | _SP, _P, _P, _P, _P, _P, _P, _P,	/* 32-39 */
    _P, _P, _P, _P, _P, _P, _P, _P,	/* 40-47 */
    _D, _D, _D, _D, _D, _D, _D, _D,	/* 48-55 */
    _D, _D, _P, _P, _P, _P, _P, _P,	/* 56-63 */
    _P, _U | _X, _U | _X, _U | _X, _U | _X, _U | _X, _U | _X, _U,	/* 64-71 */
    _U, _U, _U, _U, _U, _U, _U, _U,	/* 72-79 */
    _U, _U, _U, _U, _U, _U, _U, _U,	/* 80-87 */
    _U, _U, _U, _P, _P, _P, _P, _P,	/* 88-95 */
    _P, _L | _X, _L | _X, _L | _X, _L | _X, _L | _X, _L | _X, _L,	/* 96-103 */
    _L, _L, _L, _L, _L, _L, _L, _L,	/* 104-111 */
    _L, _L, _L, _L, _L, _L, _L, _L,	/* 112-119 */
    _L, _L, _L, _P, _P, _P, _P, _C,	/* 120-127 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 128-143 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 144-159 */
    _S | _SP, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P,	/* 160-175 */
    _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P, _P,	/* 176-191 */
    _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U, _U,	/* 192-207 */
    _U, _U, _U, _U, _U, _U, _U, _P, _U, _U, _U, _U, _U, _U, _U, _L,	/* 208-223 */
    _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L, _L,	/* 224-239 */
    _L, _L, _L, _L, _L, _L, _L, _P, _L, _L, _L, _L, _L, _L, _L, _L	/* 240-255 */
};

char print_buf[1024] = { };


#define ATO(name, type) \
    type ato##name(const char *nptr) \
{ \
    type i = 0; \
    \
    while (isdigit(*nptr)) \
    i = 10 * i + *(nptr++) - '0'; \
    return i; \
    }

ATO(i, int) ATO(l, long) ATO(f, double)

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    unsigned long value, minus = 0, result = 0;

    while (isspace(*nptr) || (*nptr == '+') || (*nptr == '-')) {
        if (*nptr == '-')
            minus = !minus;
        nptr++;
    }
    if (!base) {
        base = 10;
        if (*nptr == '0') {
            base = 8;
            nptr++;
            if ((*nptr == 'x') && isxdigit(nptr[1])) {
                nptr++;
                base = 16;
            }
        }
    } else {
        if ((base == 16) && (*nptr == '0') && (nptr[1] == 'x'))
            nptr += 2;
    }
    while (isalnum(*nptr) &&
           (value = isdigit(*nptr) ? *nptr - '0' : toupper(*nptr)
            - 'A' + 10) < base) {
        result = result * base + value;
        nptr++;
    }
    if (endptr)
        *endptr = (char *) nptr;
    return minus ? -result : result;
}

long strtol(const char *nptr, char **endptr, int base)
{
    return (long) strtoul(nptr, endptr, base);
}

static inline int skip_atoi(const char **s)
{
    int i = 0;

    while (isdigit(**s))
        i = i * 10 + *((*s)++) - '0';
    return i;
}

#define ZEROPAD	1		/* pad with zero */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* show plus */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* left justified */
#define SPECIAL	32		/* 0x */
#define LARGE	64		/* use 'ABCDEF' instead of 'abcdef' */

#define do_div(n,base) ({ \
    int __res; \
    __res = ((unsigned long) n) % (unsigned) base; \
    n = ((unsigned long) n) / (unsigned) base; \
    __res; })

static char *number(char *str, long num, int base, int size, int precision,
                    int type)
{
    char c, sign, tmp[66];
    const char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    int i;

    if (type & LARGE)
        digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (type & LEFT)
        type &= ~ZEROPAD;
    if (base < 2 || base > 36)
        return 0;
    c = (type & ZEROPAD) ? '0' : ' ';
    sign = 0;
    if (type & SIGN) {
        if (num < 0) {
            sign = '-';
            num = -num;
            size--;
        } else if (type & PLUS) {
            sign = '+';
            size--;
        } else if (type & SPACE) {
            sign = ' ';
            size--;
        }
    }
    if (type & SPECIAL) {
        if (base == 16)
            size -= 2;
        else if (base == 8)
            size--;
    }
    i = 0;
    if (num == 0)
        tmp[i++] = '0';
    else
        while (num != 0)
            tmp[i++] = digits[do_div(num, base)];
    if (i > precision)
        precision = i;
    size -= precision;
    if (!(type & (ZEROPAD + LEFT)))
        while (size-- > 0)
            *str++ = ' ';
    if (sign)
        *str++ = sign;
    if (type & SPECIAL) {
        if (base == 8)
            *str++ = '0';
        else if (base == 16) {
            *str++ = '0';
            *str++ = digits[33];
        }
    }
    if (!(type & LEFT))
        while (size-- > 0)
            *str++ = c;
    while (i < precision--)
        *str++ = '0';
    while (i-- > 0)
        *str++ = tmp[i];
    while (size-- > 0)
        *str++ = ' ';
    return str;
}

static int vsprintf(char *buf, const char *fmt, va_list args)
{
    int len;
    unsigned long num;
    int i, base;
    char *str;
    const char *s;

    int flags;		/* flags to number() */

    int field_width;	/* width of output field */
    int precision;		/* min. # of digits for integers; max
                   number of chars for from string */
    int qualifier;		/* 'h', 'l', or 'L' for integer fields */

    for (str = buf; *fmt; ++fmt) {
        if (*fmt != '%') {
            *str++ = *fmt;
            continue;
        }

        /* process flags */
        flags = 0;
repeat:
        ++fmt;		/* this also skips first '%' */
        switch (*fmt) {
        case '-':
            flags |= LEFT;
            goto repeat;
        case '+':
            flags |= PLUS;
            goto repeat;
        case ' ':
            flags |= SPACE;
            goto repeat;
        case '#':
            flags |= SPECIAL;
            goto repeat;
        case '0':
            flags |= ZEROPAD;
            goto repeat;
        }

        /* get field width */
        field_width = -1;
        if (isdigit(*fmt))
            field_width = skip_atoi(&fmt);
        else if (*fmt == '*') {
            ++fmt;
            /* it's the next argument */
            field_width = va_arg(args, int);
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        /* get the precision */
        precision = -1;
        if (*fmt == '.') {
            ++fmt;
            if (isdigit(*fmt))
                precision = skip_atoi(&fmt);
            else if (*fmt == '*') {
                ++fmt;
                /* it's the next argument */
                precision = va_arg(args, int);
            }
            if (precision < 0)
                precision = 0;
        }

        /* get the conversion qualifier */
        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
            qualifier = *fmt;
            ++fmt;
        }

        /* default base */
        base = 10;

        switch (*fmt) {
        case 'c':
            if (!(flags & LEFT))
                while (--field_width > 0)
                    *str++ = ' ';
            *str++ = (unsigned char) va_arg(args, int);
            while (--field_width > 0)
                *str++ = ' ';
            continue;

        case 's':
            s = va_arg(args, char *);
            if (!s)
                s = "<NULL>";

            len = strnlen(s, precision);

            if (!(flags & LEFT))
                while (len < field_width--)
                    *str++ = ' ';
            for (i = 0; i < len; ++i)
                *str++ = *s++;
            while (len < field_width--)
                *str++ = ' ';
            continue;

        case 'p':
            if (field_width == -1) {
                field_width = 2 * sizeof(void *);
                flags |= ZEROPAD;
            }
            str = number(str,
                         (unsigned long) va_arg(args, void *),
                         16, field_width, precision, flags);
            continue;


        case 'n':
            if (qualifier == 'l') {
                long *ip = va_arg(args, long *);
                *ip = (str - buf);
            } else {
                int *ip = va_arg(args, int *);
                *ip = (str - buf);
            }
            continue;

        case '%':
            *str++ = '%';
            continue;

            /* integer number formats - set up the flags and "break" */
        case 'o':
            base = 8;
            break;

        case 'X':
            flags |= LARGE;
        case 'x':
            base = 16;
            break;

        case 'd':
        case 'i':
            flags |= SIGN;
        case 'u':
            break;

        default:
            *str++ = '%';
            if (*fmt)
                *str++ = *fmt;
            else
                --fmt;
            continue;
        }
        if (qualifier == 'l')
            num = va_arg(args, unsigned long);
        else if (qualifier == 'h') {
            num = (unsigned short) va_arg(args, int);
            if (flags & SIGN)
                num = (short) num;
        } else if (flags & SIGN)
            num = va_arg(args, int);
        else
            num = va_arg(args, unsigned int);
        str =
                number(str, num, base, field_width, precision, flags);
    }
    *str = '\0';
    return str - buf;
}

int sprintf(char *str, const char *format, ...)
{
    va_list args;
    int i;

    va_start(args, format);
    i = vsprintf(str, format, args);
    va_end(args);
    return i;
}

void fdprintf(int fd, const char *format, ...)
{
    va_list args;
    int i;

    va_start(args, format);
    i = vsprintf(print_buf, format, args);
    va_end(args);
    write(fd, print_buf, i);
}

#ifndef __NR_setup
#define __NR_setup 0
#endif
_syscall4(void, setup, int, arg1, int, arg2, int, arg3, int, arg4);
_syscall1(void, exit, int, status);
_syscall0(pid_t, fork);
_syscall3(ssize_t, read, int, fd, void *, buf, size_t, count);
_syscall3(ssize_t, write, int, fd, const void *, buf, size_t, count);
_syscall3(int, open, const char *, pathname, int, flags, mode_t, mode);
_syscall1(int, close, int, fd);
_syscall3(pid_t, waitpid, pid_t, pid, int *, status, int, options);
_syscall2(int, creat, const char *, pathname, mode_t, mode);
_syscall2(int, link, const char *, oldpath, const char *, newpath);
_syscall1(int, unlink, const char *, pathname);
_syscall3(int, execve, const char *, filename, char **const, argv,
          char **const, envp);
_syscall1(int, chdir, const char *, path);
_syscall1(time_t, time, time_t *, t);
_syscall3(int, mknod, const char *, pathname, mode_t, mode, dev_t, dev);
_syscall2(int, chmod, const char *, path, mode_t, mode);
_syscall3(int, lchown, const char *, path, uid_t, owner, gid_t, group);
_syscall3(off_t, lseek, int, fildes, off_t, offset, int, whence);
_syscall0(pid_t, getpid);
_syscall5(int, mount, const char *, specialfile, const char *, dir,
          const char *, filesystemtype, unsigned long, rwflag,
          const void *, data);
_syscall1(int, umount, const char *, dir);
_syscall1(int, setuid, uid_t, uid);
_syscall0(uid_t, getuid);
_syscall1(int, stime, time_t *, t);
_syscall2(int, utime, const char *, filename, struct utimbuf *, buf);
_syscall4(int, ptrace, int, request, int, pid, int, addr, int, data);
_syscall1(unsigned int, alarm, unsigned int, seconds);
_syscall0(int, pause);
_syscall2(int, access, const char *, pathname, int, mode);
_syscall1(int, nice, int, inc);
_syscall0(int, sync);
_syscall2(int, kill, pid_t, pid, int, sig);
_syscall2(int, rename, const char *, oldpath, const char *, newpath);
_syscall2(int, mkdir, const char *, pathname, mode_t, mode);
_syscall1(int, rmdir, const char *, pathname);
_syscall1(int, dup, int, oldfd);
_syscall0(int, pipe);
_syscall1(clock_t, times, struct tms *, buf);
_syscall1(int, brk, void *, end_data_segment);
_syscall1(int, setgid, gid_t, gid);
_syscall0(gid_t, getgid);
_syscall2(sighandler_t, signal, int, signum, sighandler_t, handler);
_syscall0(uid_t, geteuid);
_syscall0(gid_t, getegid);
_syscall1(int, acct, const char *, filename);
_syscall2(int, umount2, char *, name, int, flags);
_syscall3(int, ioctl, unsigned int, fd, unsigned int, cmd, unsigned long,
          arg);
_syscall3(int, fcntl, int, fd, int, cmd, long, arg);
_syscall2(int, setpgid, pid_t, pid, pid_t, pgid);
_syscall1(mode_t, umask, mode_t, mask);
_syscall1(int, chroot, const char *, path);
_syscall2(int, ustat, dev_t, dev, struct ustat *, ubuf);
_syscall2(int, dup2, int, oldfd, int, newfd);
_syscall0(pid_t, getppid);
_syscall0(pid_t, getpgrp);
_syscall0(pid_t, setsid);
_syscall3(int, sigaction, int, signum, const struct sigaction *, act,
          struct sigaction *, oldact);
_syscall0(int, sgetmask);
_syscall1(int, ssetmask, int, newmask);
_syscall2(int, setreuid, uid_t, ruid, uid_t, euid);
_syscall2(int, setregid, gid_t, rgid, gid_t, egid);
_syscall1(int, sigsuspend, const sigset_t *, mask);
_syscall1(int, sigpending, sigset_t *, set);
_syscall2(int, sethostname, const char *, name, size_t, len);
_syscall2(int, setrlimit, int, resource, const struct rlimit *, rlim);
_syscall2(int, getrlimit, int, resource, struct rlimit *, rlim);
_syscall2(int, getrusage, int, who, struct rusage *, usage);
_syscall2(int, gettimeofday, struct timeval *, tv, struct timezone *, tz);
_syscall2(int, settimeofday, const struct timeval *, tv,
          const struct timezone *, tz);
_syscall1(int, getgroups, int, size);
_syscall2(int, setgroups, size_t, size, const gid_t *, list);
_syscall5(int, select, int, n, fd_set *, readfds, fd_set *, writefds,
          fd_set *, exceptfds, struct timeval *, timeout);
_syscall2(int, symlink, const char *, oldpath, const char *, newpath);
_syscall3(int, readlink, const char *, path, char *, buf, size_t, bufsiz);
_syscall1(int, uselib, const char *, library);
_syscall2(int, swapon, const char *, path, int, swapflags);
_syscall4(int, reboot, int, magic, int, magic2, int, flag, void *, arg);
_syscall3(int, readdir, unsigned int, fd, struct dirent *, dirp,
          unsigned int, count);
_syscall1(int, mmap, struct mmap_arg_struct *, arg);
_syscall2(int, munmap, void *, start, size_t, length);
_syscall2(int, truncate, const char *, path, size_t, length);
_syscall2(int, ftruncate, int, fd, size_t, length);
_syscall2(int, fchmod, int, fildes, mode_t, mode);
_syscall3(int, fchown, int, fd, uid_t, owner, gid_t, group);
_syscall2(int, getpriority, int, which, int, who);
_syscall3(int, setpriority, int, which, int, who, int, prio);
_syscall2(int, statfs, const char *, path, struct statfs *, buf);
_syscall2(int, fstatfs, unsigned int, fd, struct statfs *, buf);
_syscall3(int, ioperm, unsigned long, from, unsigned long, num, int,
          turn_on);
_syscall2(int, socketcall, int, call, unsigned long *, args);
_syscall3(int, syslog, int, type, char *, bufp, int, len);
_syscall2(int, getitimer, int, which, struct itimerval *, value);
_syscall3(int, setitimer, int, which, const struct itimerval *, value,
          struct itimerval *, ovalue);
_syscall2(int, stat, const char *, file_name, struct stat *, buf);
_syscall2(int, lstat, const char *, file_name, struct stat *, buf);
_syscall2(int, fstat, int, filedes, struct stat *, buf);
_syscall1(int, iopl, int, level);
_syscall0(int, vhangup);
_syscall0(void, idle);
_syscall1(int, vm86old, struct vm86_struct *, info);
_syscall4(pid_t, wait4, pid_t, pid, int *, status, int, options,
          struct rusage *, rusage);
_syscall1(int, swapoff, const char *, path);
_syscall1(int, sysinfo, struct sysinfo *, info);
//_syscall5(int,  ipc, unsigned  int,  call,  int,  first, int, second, int, third, void *, ptr, long, fifth);
_syscall1(int, fsync, int, fd);
_syscall1(int, sigreturn, unsigned long, __unused);
_syscall4(int, clone, void *, fn, void *, child_stack, int, flags, void *,
          arg);
_syscall2(int, setdomainname, const char *, name, size_t, len);
_syscall1(int, uname, struct utsname *, buf);
_syscall3(int, modify_ldt, int, func, void *, ptr, unsigned long,
          bytecount);
_syscall1(int, adjtimex, struct timex *, buf);
_syscall3(int, mprotect, const void *, addr, size_t, len, int, prot);
_syscall3(int, sigprocmask, int, how, const sigset_t *, set, sigset_t *,
          oldset);
_syscall2(caddr_t, create_module, const char *, name, size_t, size);
_syscall2(int, init_module, const char *, name, struct module *, image);
_syscall1(int, delete_module, const char *, name);
_syscall1(int, get_kernel_syms, struct kernel_sym *, table);
_syscall4(int, quotactl, int, cmd, char *, special, int, uid, caddr_t,
          addr);
_syscall1(pid_t, getpgid, pid_t, pid);
_syscall1(int, fchdir, int, fd);
_syscall2(int, bdflush, int, func, long, data);
_syscall3(int, sysfs, int, option, unsigned long, arg1, unsigned long,
          arg2);
_syscall1(int, personality, unsigned long, persona);
_syscall1(int, setfsuid, uid_t, fsuid);
_syscall1(int, setfsgid, uid_t, fsgid);
_syscall5(int, _llseek, unsigned int, fd, unsigned long, offset_high,
          unsigned long, offset_low, loff_t *, result, unsigned int
          , whence);
_syscall3(int, getdents, unsigned int, fd, struct dirent *, dirp,
          unsigned int, count);
_syscall5(int, _newselect, int, n, fd_set *, readfds, fd_set *, writefds,
          fd_set *, exceptfds, struct timeval *, timeout);
_syscall2(int, flock, int, fd, int, operation);
_syscall3(int, msync, const void *, start, size_t, length, int, flags);
_syscall3(int, readv, int, fd, const struct iovec *, vector, int, count);
_syscall3(int, writev, int, fd, const struct iovec *, vector, int
          , count);
_syscall1(pid_t, getsid, pid_t, pid);
_syscall1(int, fdatasync, int, fd);
_syscall1(int, _sysctl, struct __sysctl_args *, args);
_syscall2(int, mlock, const void *, addr, size_t, len);
_syscall2(int, munlock, const void *, addr, size_t, len);
_syscall1(int, mlockall, int, flags);
_syscall0(int, munlockall);
/* _syscall2(int, sched_setparam, pid_t,  pid,  const  struct  sched_param
                   *, p);
_syscall2(int, sched_getparam, pid_t, pid, struct sched_param *, p);
_syscall3(int, sched_setscheduler, pid_t, pid, int, policy, const struct
                   sched_param *, p);
_syscall1(int, sched_getscheduler, pid_t, pid);
_syscall0(int, sched_yield);
_syscall1(int, sched_get_priority_max, int, policy);
_syscall1(int, sched_get_priority_min, int, policy);
_syscall2(int, sched_rr_get_interval, pid_t, pid, struct timespec *, tp); */
_syscall2(int, nanosleep, const struct timespec *, req, struct timespec
          *, rem);
_syscall4(void *, mremap, void *, old_address, size_t, old_size, size_t,
          new_size, unsigned long, flags);
_syscall3(int, setresuid, uid_t, ruid, uid_t, euid, uid_t, suid);
_syscall3(int, getresuid, uid_t *, ruid, uid_t *, euid, uid_t *, suid);
_syscall2(int, vm86, unsigned long, fn, struct vm86plus_struct *, v86);
_syscall5(int, query_module, const char *, name, int, which,
          void *, buf, size_t, bufsize, size_t *, ret);
_syscall3(int, poll, struct pollfd *, ufds, unsigned int, nfds, int,
          timeout);
//_syscall3(void, nfsservctl, int,   cmd,   struct   nfsctl_arg  *, argp, union nfsctl_res *, resp);
_syscall3(int, setresgid, gid_t, rgid, gid_t, egid, gid_t, sgid);
_syscall3(int, getresgid, gid_t *, rgid, gid_t *, egid, gid_t *, sgid);
_syscall5(int, prctl, int, option, unsigned long, arg2, unsigned long,
          arg3, unsigned long, arg4, unsigned long, arg5);
_syscall1(int, rt_sigreturn, unsigned long, __unused);
_syscall4(int, rt_sigaction, int, sig, const struct sigaction *, act,
          struct sigaction *, oact, size_t, sigsetsize);
_syscall4(int, rt_sigprocmask, int, how, const sigset_t *, set, sigset_t
          *, oset, size_t, sigsetsize);
_syscall2(int, rt_sigpending, sigset_t *, set, size_t, sigsetsize);
_syscall4(int, rt_sigtimedwait, const sigset_t *, uthese, siginfo_t *,
          uinfo, const struct timespec *, uts, size_t, sigsetsize);
_syscall3(int, rt_sigqueueinfo, int, pid, int, sig, siginfo_t *, uinfo);
_syscall2(int, rt_sigsuspend, sigset_t *, unewset, size_t, sigsetsize);
_syscall4(ssize_t, pread, int, fd, void *, buf, size_t, count, off_t,
          offset);
_syscall4(ssize_t, pwrite, int, fd, const void *, buf, size_t, count,
          off_t, offset);
_syscall3(int, chown, const char *, path, uid_t, owner, gid_t, group);
_syscall2(char *, getcwd, char *, buf, size_t, size);
//_syscall2(int, capget, cap_user_header_t, header, cap_user_data_t, dataptr);  
//_syscall2(int, capset, cap_user_header_t, header, const cap_user_data_t, data);
_syscall2(int, sigaltstack, const stack_t *, uss, stack_t *, uoss);
_syscall4(int, sendfile, int, out_fd, int, in_fd, off_t *, offset, size_t,
          count);
_syscall0(pid_t, vfork);
