#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <grp.h>
#ifdef __linux__
#include <sys/io.h>
#endif
#include "emu.h"
#include "priv.h"
#include "dosemu_config.h"
#include "mapping.h"
#include "utilities.h"
#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

#if 0
#define PRIV_TESTING
#endif

/* Some handy information to have around */
static uid_t uid,euid;
static gid_t gid,egid;
static uid_t cur_uid, cur_euid;
static gid_t cur_gid, cur_egid;

static int skip_priv_setting = 0;

int can_do_root_stuff;
int under_root_login;
int using_sudo;
int current_iopl;

#define PRIVS_ARE_ON (euid == cur_euid)
#define PRIVS_ARE_OFF (uid == cur_euid)
#define PRIVS_WERE_ON(privs) (pop_priv(privs))


static void push_priv(saved_priv_status *privs)
{
  if (!privs || *privs != PRIV_MAGIC) {
    error("Aiiiee... not in-sync saved priv status on push_priv\n");
    leavedos(99);
  }
  *privs = PRIVS_ARE_ON;
#ifdef PRIV_TESTING
  c_printf("PRIV: pushing %d privs_ptr=%p\n", *privs, privs);
#endif
}

static int pop_priv(saved_priv_status *privs)
{
  int ret;
  if (!privs || *privs == PRIV_MAGIC) {
    error("Aiiiee... not in-sync saved priv status on pop_priv\n");
    leavedos(99);
  }
#ifdef PRIV_TESTING
  c_printf("PRIV: poping %d privs_ptr=%p\n", *privs, privs);
#endif
  ret = (int)*privs;
  *privs = PRIV_MAGIC;
  return ret;
}

static int _priv_on(void)
{
  if (PRIVS_ARE_OFF) {  /* make sure the privs need to be changed */
#ifdef PRIV_TESTING
      c_printf("PRIV: on-in %d\n", cur_euid);
#endif
      if (setreuid(uid,euid)) {
         error("Cannot turn privs on!\n");
         return 0;
      }
      cur_uid = uid;
      cur_euid = euid;
      if (setregid(gid,egid)) {
	  error("Cannot turn privs on!\n");
	  return 0;
      }
      cur_gid = gid;
      cur_egid = egid;
  }
#ifdef PRIV_TESTING
  c_printf("PRIV: on-ex %d\n", cur_euid);
#endif
  return 1;
}

static int _priv_off(void)
{
  if (PRIVS_ARE_ON) {  /* make sure the privs need to be changed */
#ifdef PRIV_TESTING
      c_printf("PRIV: off-in %d\n", cur_euid);
#endif
      if (setreuid(euid,uid)) {
	error("Cannot turn privs off!\n");
	return 0;
      }
      cur_uid = euid;
      cur_euid = uid;
      if (setregid(egid,gid)) {
	error("Cannot turn privs off!\n");
	return 0;
      }
      cur_gid = egid;
      cur_egid = gid;
  }
#ifdef PRIV_TESTING
  c_printf("PRIV: off-ex %d\n", cur_euid);
#endif
  return 1;
}

int real_enter_priv_on(saved_priv_status *privs)
{
  if (skip_priv_setting) return 1;
  push_priv(privs);
  return _priv_on();
}

int real_enter_priv_off(saved_priv_status *privs)
{
  if (skip_priv_setting) return 1;
  push_priv(privs);
  return _priv_off();
}

int real_leave_priv_setting(saved_priv_status *privs)
{
  if (skip_priv_setting) return 1;
  if (PRIVS_WERE_ON(privs)) return _priv_on();
  return _priv_off();
}

int priv_iopl(int pl)
{
#ifdef __linux__
  int ret;
  if (PRIVS_ARE_OFF) {
    _priv_on();
    ret = iopl(pl);
    _priv_off();
  }
  else ret = iopl(pl);
#ifdef X86_EMULATOR
  if (config.cpu_vm == CPUVM_EMU) e_priv_iopl(pl);
#endif
  if (ret == 0)
    current_iopl = pl;
  return ret;
#else
  return -1;
#endif
}

uid_t get_cur_uid(void)
{
  return cur_uid;
}

uid_t get_cur_euid(void)
{
  return cur_euid;
}

gid_t get_cur_egid(void)
{
  return cur_egid;
}

uid_t get_orig_uid(void)
{
  return uid;
}

uid_t get_orig_euid(void)
{
  return euid;
}

gid_t get_orig_gid(void)
{
  return gid;
}

int priv_drop(void)
{
  if (setreuid(uid,uid) || setregid(gid,gid))
    {
      error("Cannot drop root uid or gid!\n");
      return 0;
    }
  cur_euid = euid = cur_uid = uid;
  cur_egid = egid = cur_gid = gid;
  skip_priv_setting = 1;
  if (uid) can_do_root_stuff = 0;
  return 1;
}

#define MAXGROUPS  20
static gid_t *groups;
static int num_groups = 0;


int is_in_groups(gid_t gid)
{
  int i;
  for (i=0; i<num_groups; i++) {
    if (gid == groups[i]) return 1;
  }
  return 0;
}


void priv_init(void)
{
  uid  = cur_uid  = getuid();
  if (!uid) under_root_login =1;
  euid = cur_euid = geteuid();
  if (!euid) can_do_root_stuff = 1;
  if (!uid) skip_priv_setting = 1;
  gid  = cur_gid  = getgid();
  egid = cur_egid = getegid();

  /* must store the /proc/self/exe symlink contents before dropping
     privs! */
  dosemu_proc_self_exe = readlink_malloc("/proc/self/exe");
  /* For Fedora we must also save a file descriptor to /proc/self/maps */
  dosemu_proc_self_maps_fd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
  if (under_root_login)
  {
    /* check for sudo and set to original user */
    char *s = getenv("SUDO_GID");
    if (s) {
      gid = cur_gid = atoi(s);
      if (gid) {
        setregid(gid, egid);
      }
    }
    s = getenv("SUDO_UID");
    if (s) {
      uid = cur_uid = atoi(s);
      if (uid) {
	pid_t ppid;
	char *path;
	FILE *fp;
	size_t n;
	char *line;

        skip_priv_setting = under_root_login = 0;
	using_sudo = 1;
	s = getenv("SUDO_USER");
	if (s) {
	  initgroups(s, gid);
	  setenv("USER", s, 1);
	}
        setreuid(uid, euid);

	/* retrieve $HOME from sudo's (the parent process') environment */
	ppid = getppid();
	if (asprintf(&path, "/proc/%d/environ", ppid) != -1) {
	  if ((fp = fopen(path, "r"))) {
	    line = NULL;
	    while(getdelim(&line, &n, '\0', fp) != -1) {
	      if(n>5 && memcmp(line, "HOME=", 5) == 0) {
		setenv("HOME", line+5, 1);
	      }
	    }
	    free(line);
	    fclose(fp);
	  }
	  free(path);
	}
      }
    }
  }

  if (!can_do_root_stuff)
    {
      skip_priv_setting = 1;
    }

  if ((num_groups = getgroups(0, NULL)) <= 0) {
    error("priv_init(): getgroups() size returned %d!\n", num_groups);
    goto error_exit;
  }

  if ((groups = malloc(num_groups * sizeof(gid_t))) == NULL) {
    error("priv_init(): malloc() failed!\n");
    goto error_exit;
  }

  if (getgroups(num_groups, groups) == -1) {
    error("priv_init(): getgroups() failed '%s'!\n", strerror(errno));
    free(groups);
    goto error_exit;
  }

  goto done;

error_exit:
  num_groups = 0;
  groups = NULL;

done:
  if (!skip_priv_setting) _priv_off();
}
