#ifndef __shared_h
#define __shared_h

/* error codes */
#define E_ARGS 1
#define E_DEVICE 2
#define E_MNTPT 3
#define E_POLICY 4
#define E_EXECMOUNT 5
#define E_EXECUMOUNT 5
#define E_UNLOCK 6
#define E_PID 7
#define E_LOCKED 8
/* Something not explicitly allowed from within the system
   configuration file */
#define E_DISALLOWED 9
/* Something failed with loop devices */
#define E_LOSETUP 10
#define E_DISALLOWED 9

#define E_INTERNAL 100


/**
 * Initialize locale, check that we are root, read system
 * configuration file, then drop root privileges.
 */
void shared_init(void);

/**
 * Get real path of device, if possible. The resulting string must be
 * freed.
 */
char *device_realpath(const char *path, int *is_real_path);

#endif /* __shared_h */
