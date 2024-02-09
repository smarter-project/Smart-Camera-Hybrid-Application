/** user-space application to capture from Cortex-M

 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/rpmsg.h>
#include <signal.h>
#include <errno.h>

#define RPMSG_DATA_SIZE 256

typedef struct {
  char data[RPMSG_DATA_SIZE];
} msg_data_t;

#define MSG             "Ok, Computer"
#define DEFAULT_LOGFILE         "/tmp/m_console.log"


char *logfile;

msg_data_t data_buf;

/* channel name and address need to match what is running on the Cortex-M */
struct rpmsg_endpoint_info ept_info = {"rpmsg-openamp-demo-channel", 0x2, 0x1e};
int fd_ept;

FILE *logfp = NULL;  /* Used to wrote to a local logfile */
int fd;



void cleanup() {
  /* destroy endpoint */
  if (fd_ept) {
    ioctl(fd_ept, RPMSG_DESTROY_EPT_IOCTL);
    close(fd_ept);
  }
  if (fd) {
    close(fd);
  }
}


void sig_handler(int signo)
{
  if (signo == SIGINT)
    printf("received SIGINT\n");
    if (logfp) {
      printf("Closing logfile\n");
      fclose(logfp);
    }
  cleanup();
  exit(0);
}

void check() {
  if (open("/dev/rpmsg_ctrl0", O_RDWR) < 0) {
    printf("Could not open rpmsg_ctrl0\n");
    if (logfp) {
      printf("Closing logfile\n");
      fclose(logfp);
    }
    exit(0);
  }
}


int main(int argc, char *argv[]) {
  int err;
  if (argc == 1) {
    logfile = DEFAULT_LOGFILE;
  } else if (argc == 2) {
    logfile = argv[1];
  } else {
    printf("Usage: %s logfilename\n", argv[0]);
    exit(-1);
  }

  logfp = fopen(logfile,  "w+");
  if (logfp < 0) {
    int err = errno;
    printf("Error opening logfile %s : %s \n", logfile, strerror(err));
    exit (-1);
  }

  if (signal(SIGINT, sig_handler) == SIG_ERR) {
     printf("\ncan't catch SIGINT\n");
  }

  fd = open("/dev/rpmsg_ctrl0", O_RDWR);
  if (fd < 0) {
    printf("Could not open rpmsg_ctrl0\nMaybe the firmware is not running yet?\n");
    exit(-1);
  }

  /* printf("/dev/rpmsg_ctrl0 opened: %d\n", fd); */

  /* create endpoint interface */
  if (ioctl(fd, RPMSG_CREATE_EPT_IOCTL, &ept_info) < 0) {
        int err = errno;
    printf("Error creating endpoint interface: %s \n", strerror(err));
    exit(-1);
  }

  /* create endpoint */
  fd_ept = open("/dev/rpmsg0", O_RDWR);  /* backend creates endpoint */
  if (fd_ept < 0) {
    int err = errno;
    printf("Error creating endpoint: %s \n", strerror(err));
    exit(-1);
  }

  if (write(fd_ept, &MSG, strlen(MSG)) < 0) {
    int err = errno;
    printf("Error writing to endpoint: %s \n", strerror(err));
    exit(-1);
  }


  /* receive data from remote device */
  printf("Writing Cortex-M console info to %s\n", logfile);
  while(1) {
    check();
    if (read(fd_ept, &data_buf, sizeof(data_buf)) > 0) {
      fprintf(logfp, "%s", data_buf);
      fflush(logfp);
    } else {
      /* The channel is probably closed */
      printf("Closing\n");
      exit(0);
    }
  }

  /* destroy endpoint */
  ioctl(fd_ept, RPMSG_DESTROY_EPT_IOCTL);
  close(fd_ept);
  close(fd);
}
