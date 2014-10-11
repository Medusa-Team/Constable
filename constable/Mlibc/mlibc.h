/**
 * @file mlibc.h
 * @short header file for Mini libc
 * (c)1999-2000 by Martin Ockajak
 * $Id: mlibc.h,v 1.2 2002/10/23 10:25:43 marek Exp $
 */

#include <stdarg.h>
#define _LOOSE_KERNEL_NAMES	/* new gcc needs this because stdarg.h now
				   includes features.h which wouldn't allow
				   declaration of some important types in the
				   kernel includes */
#undef __GLIBC__
#include <linux/types.h>
#include <linux/unistd.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <ctype.h>
#include <linux/signal.h>
#include <linux/resource.h>
#include <linux/times.h>
#include <linux/utime.h>
#include <dirent.h>
#include <linux/reboot.h>
#include <linux/kernel.h>
//#include <linux/sys.h>
#include <linux/utsname.h>
#include <linux/timex.h>
//#include <linux/module.h>
#include <linux/uio.h>
#include <linux/sysctl.h>
//#include <linux/nfsd/syscall.h>
#include <linux/prctl.h>
//#include <linux/capability.h>
#include <asm/statfs.h>
#include <asm/vm86.h>
//#include <asm/ipc.h>
//#include <asm/poll.h>
#define __KERNEL__
#include <linux/stat.h>
//#include <asm/string.h>
#undef __KERNEL__

/* Here is stuff we must add */

#define INT_MAX         ((int)(~0U>>1))
#define INT_MIN		(-INT_MAX - 1)
#define UINT_MAX        (~0U)
#define LONG_MAX        ((long)(~0UL>>1))
#define LONG_MIN	(-LONG_MAX - 1L)
#define ULONG_MAX       (~0UL)
#define MININT          INT_MIN
#define MINLONG         LONG_MIN
#define MAXINT          INT_MAX
#define MAXLONG         LONG_MAX
#define utsname new_utsname


typedef __sighandler_t(*sighandler_t) (int);

struct mmap_arg_struct {
	unsigned long addr;
	unsigned long len;
	unsigned long prot;
	unsigned long flags;
	unsigned long fd;
	unsigned long offset;
};


int atoi(const char *nptr);
long atol(const char *nptr);
double atof(const char *nptr);
unsigned long strtoul(const char *nptr, char **endptr, int base);
long strtol(const char *nptr, char **endptr, int base);
int sprintf(char *str, const char *format, ...);
/* This one has only 1024 char buffer - don't overflow it */
void fdprintf(int fd, const char *format, ...);
#define printf(x...) fdprintf(1, x);


void setup(int arg1, int arg2, int arg3, int arg4);
void exit(int status);
pid_t fork(void);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int open(const char *pathname, int flags, mode_t mode);
int close(int fd);
pid_t waitpid(pid_t pid, int *status, int options);
int creat(const char *pathname, mode_t mode);
int link(const char *oldpath, const char *newpath);
int unlink(const char *pathname);
int execve(const char *filename, char **const argv, char **const envp);
int chdir(const char *path);
time_t time(time_t * t);
int mknod(const char *pathname, mode_t mode, dev_t dev);
int chmod(const char *path, mode_t mode);
int lchown(const char *path, uid_t owner, gid_t group);
off_t lseek(int fildes, off_t offset, int whence);
pid_t getpid(void);
int mount(const char *specialfile, const char *dir,
	  const char *filesystemtype, unsigned long rwflag,
	  const void *data);
int umount(const char *dir);
int setuid(uid_t uid);
uid_t getuid(void);
int stime(time_t * t);
int ptrace(int request, int pid, int addr, int data);
unsigned int alarm(unsigned int seconds);
int pause(void);
int utime(const char *filename, struct utimbuf *buf);
int access(const char *pathname, int mode);
int nice(int inc);
int sync(void);
int kill(pid_t pid, int sig);
int rename(const char *oldpath, const char *newpath);
int mkdir(const char *pathname, mode_t mode);
int rmdir(const char *pathname);
int dup(int oldfd);
int pipe(void);
clock_t times(struct tms *buf);
int brk(void *end_data_segment);
int setgid(gid_t gid);
gid_t getgid(void);
sighandler_t signal(int signum, sighandler_t handler);
uid_t geteuid(void);
gid_t getegid(void);
int acct(const char *filename);
int umount2(char *name, int flags);
int ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);
int fcntl(int fd, int cmd, long arg);
int setpgid(pid_t pid, pid_t pgid);
mode_t umask(mode_t mask);
int chroot(const char *path);
int ustat(dev_t dev, struct ustat *ubuf);
int dup2(int oldfd, int newfd);
pid_t getppid(void);
pid_t getpgrp(void);
pid_t setsid(void);
int sigaction(int signum, const struct sigaction *act,
	      struct sigaction *oldact);
int sgetmask(void);
int ssetmask(int newmask);
int setreuid(uid_t ruid, uid_t euid);
int setregid(gid_t rgid, gid_t egid);
int sigsuspend(const sigset_t * mask);
int sigpending(sigset_t * set);
int sethostname(const char *name, size_t len);
int setrlimit(int resource, const struct rlimit *rlim);
int getrlimit(int resource, struct rlimit *rlim);
int getrusage(int who, struct rusage *usage);
int gettimeofday(struct timeval *tv, struct timezone *tz);
int settimeofday(const struct timeval *tv, const struct timezone *tz);
int getgroups(int size);
int setgroups(size_t size, const gid_t * list);
int select(int n, fd_set * readfds, fd_set * writefds, fd_set * exceptfds,
	   struct timeval *timeout);
int symlink(const char *oldpath, const char *newpath);
int readlink(const char *path, char *buf, size_t bufsiz);
int uselib(const char *library);
int swapon(const char *path, int swapflags);
int reboot(int magic, int magic2, int flag, void *arg);
int readdir(unsigned int fd, struct dirent *dirp, unsigned int count);
int mmap(struct mmap_arg_struct *arg);
int munmap(void *start, size_t length);
int truncate(const char *path, size_t length);
int ftruncate(int fd, size_t length);
int fchmod(int fildes, mode_t mode);
int fchown(int fd, uid_t owner, gid_t group);
int getpriority(int which, int who);
int setpriority(int which, int who, int prio);
int statfs(const char *path, struct statfs *buf);
int fstatfs(unsigned int fd, struct statfs *buf);
int ioperm(unsigned long from, unsigned long num, int turn_on);
int getitimer(int which, struct itimerval *value);
int setitimer(int which, const struct itimerval *value,
	      struct itimerval *ovalue);
int socketcall(int call, unsigned long *args);
int syslog(int type, char *bufp, int len);
int stat(const char *file_name, struct stat *buf);
int lstat(const char *file_name, struct stat *buf);
int fstat(int filedes, struct stat *buf);
int iopl(int level);
int vhangup(void);
void idle(void);
int vm86old(struct vm86_struct *info);
pid_t wait4(pid_t pid, int *status, int options, struct rusage *rusage);
int swapoff(const char *path);
int sysinfo(struct sysinfo *info);
//int ipc(unsigned  int  call,  int  first, int second, int third, void *ptr, long fifth);
int fsync(int fd);
int sigreturn(unsigned long __unused);
int clone(void *fn, void *child_stack, int flags, void *arg);
int setdomainname(const char *name, size_t len);
int uname(struct utsname *buf);
int modify_ldt(int func, void *ptr, unsigned long bytecount);
int adjtimex(struct timex *buf);
int mprotect(const void *addr, size_t len, int prot);
int sigprocmask(int how, const sigset_t * set, sigset_t * oldset);
caddr_t create_module(const char *name, size_t size);
int init_module(const char *name, struct module *image);
int delete_module(const char *name);
int get_kernel_syms(struct kernel_sym *table);
int quotactl(int cmd, char *special, int uid, caddr_t addr);
pid_t getpgid(pid_t pid);
int fchdir(int fd);
int bdflush(int func, long data);
int sysfs(int option, unsigned long arg1, unsigned long arg2);
int personality(unsigned long persona);
int setfsuid(uid_t fsuid);
int setfsgid(uid_t fsgid);
int _llseek(unsigned int fd, unsigned long offset_high,
	    unsigned long offset_low, loff_t * result, unsigned int
	    whence);
int getdents(unsigned int fd, struct dirent *dirp, unsigned int count);
int _newselect(int n, fd_set * readfds, fd_set * writefds,
	       fd_set * exceptfds, struct timeval *timeout);
int flock(int fd, int operation);
int msync(const void *start, size_t length, int flags);
int readv(int fd, const struct iovec *vector, int count);
int writev(int fd, const struct iovec *vector, int count);
pid_t getsid(pid_t pid);
int fdatasync(int fd);
int _sysctl(struct __sysctl_args *args);
int mlock(const void *addr, size_t len);
int munlock(const void *addr, size_t len);
int mlockall(int flags);
int munlockall(void);
/* int sched_setparam(pid_t  pid,  const  struct  sched_param
		       *p);
int sched_getparam(pid_t pid, struct sched_param *p);
int sched_setscheduler(pid_t pid, int policy, const struct
		       sched_param *p);
int sched_getscheduler(pid_t pid);
int sched_yield(void);
int sched_get_priority_max(int policy);
int sched_get_priority_min(int policy);
int sched_rr_get_interval(pid_t pid, struct timespec *tp); */
int nanosleep(const struct timespec *req, struct timespec
	      *rem);
void *mremap(void *old_address, size_t old_size, size_t
	     new_size, unsigned long flags);
int setresuid(uid_t ruid, uid_t euid, uid_t suid);
int getresuid(uid_t * ruid, uid_t * euid, uid_t * suid);
int vm86(unsigned long fn, struct vm86plus_struct *v86);
int query_module(const char *name, int which,
		 void *buf, size_t bufsize, size_t * ret);
int poll(struct pollfd *ufds, unsigned int nfds, int timeout);
//void nfsservctl(int   cmd,   struct   nfsctl_arg  *argp,  union nfsctl_res *resp);
int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
int getresgid(gid_t * rgid, gid_t * egid, gid_t * sgid);
int prctl(int option, unsigned long arg2, unsigned long
	  arg3, unsigned long arg4, unsigned long arg5);
int rt_sigreturn(unsigned long __unused);
int rt_sigaction(int sig, const struct sigaction *act,
		 struct sigaction *oact, size_t sigsetsize);
int rt_sigprocmask(int how, const sigset_t * set, sigset_t
		   * oset, size_t sigsetsize);
int rt_sigpending(sigset_t * set, size_t sigsetsize);
int rt_sigtimedwait(const sigset_t * uthese, siginfo_t * uinfo,
		    const struct timespec *uts, size_t sigsetsize);
int rt_sigqueueinfo(int pid, int sig, siginfo_t * uinfo);
int rt_sigsuspend(sigset_t * unewset, size_t sigsetsize);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
int chown(const char *path, uid_t owner, gid_t group);
char *getwd(char *buf, size_t size);
//int capget(cap_user_header_t header, cap_user_data_t dataptr);
//int capset(cap_user_header_t header, const cap_user_data_t data)
int sigaltstack(const stack_t * uss, stack_t * uoss);
int sendfile(int out_fd, int in_fd, off_t * offset, size_t count);
pid_t vfork(void);
#undef __GLIBC__
