
/*
 * XDDP-based RT/NRT threads communication demo.
 *
 * Real-time Xenomai threads and regular Linux threads may want to
 * exchange data in a way that does not require the former to leave
 * the real-time domain (i.e. secondary mode). Message pipes - as
 * implemented by the RTDM-based XDDP protocol - are provided for this
 * purpose.
 *
 * On the Linux domain side, pseudo-device files named /dev/rtp<minor>
 * give regular POSIX threads access to non real-time communication
 * endpoints, via the standard character-based I/O interface. On the
 * Xenomai domain side, sockets may be bound to XDDP ports, which act
 * as proxies to send and receive data to/from the associated
 * pseudo-device files. Ports and pseudo-device minor numbers are
 * paired, meaning that e.g. port 7 will proxy the traffic for
 * /dev/rtp7. Therefore, port numbers may range from 0 to
 * CONFIG_XENO_OPT_PIPE_NRDEV - 1.
 *
 * All data sent through a bound/connected XDDP socket via sendto(2) or
 * write(2) will be passed to the peer endpoint in the Linux domain,
 * and made available for reading via the standard read(2) system
 * call. Conversely, all data sent using write(2) through the non
 * real-time endpoint will be conveyed to the real-time socket
 * endpoint, and made available to the recvfrom(2) or read(2) system
 * calls.
 *
 * Both threads can use the bi-directional data path to send and
 * receive datagrams in a FIFO manner, as illustrated by the simple
 * echoing process implemented by this program.
 *
 * realtime_thread1------------------------------>-------+
 *   =>  get socket                                     |
 *   =>  bind socket to port 0                          v
 *   =>  write traffic to NRT domain via sendto()       |
 *   =>  read traffic from NRT domain via recvfrom() <--|--+
 *                                                      |  |
 * regular_thread---------------------------------------+  |
 *   =>  open /dev/rtp0                                 |  ^
 *   =>  read traffic from RT domain via read()         |  |
 *   =>  echo traffic back to RT domain via write()     +--+
 *
 * See Makefile in this directory for build directives.
 *
 * NOTE: XDDP is a replacement for the legacy RT_PIPE interface
 * available from the native skin until Xenomai 3.
 */
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <malloc.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <rtdk.h>
#include <rtdm/rtipc.h>

pthread_t rt1, rt2;

#define XDDP_PORT_LASER 0
#define XDDP_PORT_ODOM  1

static void fail(const char *reason)
{
        perror(reason);
        exit(EXIT_FAILURE);
}

static void *realtime_thread1(void *arg)
{
        struct sockaddr_ipc saddr;
        int ret, s, len;
        struct timespec ts;
        size_t poolsz;
        char buf[128];
        /*
         * Get a datagram socket to bind to the RT endpoint. Each
         * endpoint is represented by a port number within the XDDP
         * protocol namespace.
         */
        s = socket(AF_RTIPC, SOCK_DGRAM, IPCPROTO_XDDP);
        if (s < 0) {
                perror("socket");
                exit(EXIT_FAILURE);
        }
        /*
         * Set a local 16k pool for the RT endpoint. Memory needed to
         * convey datagrams will be pulled from this pool, instead of
         * Xenomai's system pool.
         */
        poolsz = 16384; /* bytes */
        ret = setsockopt(s, SOL_XDDP, XDDP_POOLSZ,
                         &poolsz, sizeof(poolsz));
        if (ret)
                fail("setsockopt");
        /*
         * Bind the socket to the port, to setup a proxy to channel
         * traffic to/from the Linux domain.
         *
         * saddr.sipc_port specifies the port number to use.
         */
        memset(&saddr, 0, sizeof(saddr));
        saddr.sipc_family = AF_RTIPC;
        saddr.sipc_port = XDDP_PORT_LASER;
        ret = bind(s, (struct sockaddr *)&saddr, sizeof(saddr));
        if (ret)
                fail("bind");
        for (;;) {
                ret = recvfrom(s, buf, sizeof(buf), 0, NULL, 0);
                if (ret <= 0)
                        fail("recvfrom");
                rt_printf("   => \"%.*s\" echoed by peer\n", ret, buf);
                
                ret = sendto(s, "ack", len, 0, NULL, 0);
                if (ret != len)
                        fail("sendto");
                rt_printf("%s: sent %d bytes, \"%.*s\"\n",
                          __FUNCTION__, ret, ret, "ack");
                /*
                 * We run in full real-time mode (i.e. primary mode),
                 * so we have to let the system breathe between two
                 * iterations.
                 */
                ts.tv_sec = 0;
                ts.tv_nsec = 500000000; /* 500 ms */
                clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);
        }
        return NULL;
}

static void *realtime_thread2(void *arg)
{
        struct sockaddr_ipc saddr;
        int ret, s, len;
        struct timespec ts;
        size_t poolsz;
        char buf[128];
        /*
         * Get a datagram socket to bind to the RT endpoint. Each
         * endpoint is represented by a port number within the XDDP
         * protocol namespace.
         */
        s = socket(AF_RTIPC, SOCK_DGRAM, IPCPROTO_XDDP);
        if (s < 0) {
                perror("socket");
                exit(EXIT_FAILURE);
        }
        /*
         * Set a local 16k pool for the RT endpoint. Memory needed to
         * convey datagrams will be pulled from this pool, instead of
         * Xenomai's system pool.
         */
        poolsz = 16384; /* bytes */
        ret = setsockopt(s, SOL_XDDP, XDDP_POOLSZ,
                         &poolsz, sizeof(poolsz));
        if (ret)
                fail("setsockopt");
        /*
         * Bind the socket to the port, to setup a proxy to channel
         * traffic to/from the Linux domain.
         *
         * saddr.sipc_port specifies the port number to use.
         */
        memset(&saddr, 0, sizeof(saddr));
        saddr.sipc_family = AF_RTIPC;
        saddr.sipc_port = XDDP_PORT_ODOM;
        ret = bind(s, (struct sockaddr *)&saddr, sizeof(saddr));
        if (ret)
                fail("bind");
        for (;;) {
                ret = recvfrom(s, buf, sizeof(buf), 0, NULL, 0);
                if (ret <= 0)
                        fail("recvfrom");
                rt_printf("   => \"%.*s\" echoed by peer\n", ret, buf);
                
                ret = sendto(s, "ack", len, 0, NULL, 0);
                if (ret != len)
                        fail("sendto");
                rt_printf("%s: sent %d bytes, \"%.*s\"\n",
                          __FUNCTION__, ret, ret, "ack");
                /*
                 * We run in full real-time mode (i.e. primary mode),
                 * so we have to let the system breathe between two
                 * iterations.
                 */
                ts.tv_sec = 0;
                ts.tv_nsec = 500000000; /* 500 ms */
                clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);
        }
        return NULL;
}

static void cleanup_upon_sig(int sig)
{
        pthread_cancel(rt);
        pthread_cancel(nrt);
        signal(sig, SIG_DFL);
        pthread_join(rt, NULL);
        pthread_join(nrt, NULL);
}
int main(int argc, char **argv)
{
        struct sched_param rtparam = { .sched_priority = 42 };
        pthread_attr_t rtattr;
        sigset_t mask, oldmask;
        mlockall(MCL_CURRENT | MCL_FUTURE);
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        signal(SIGINT, cleanup_upon_sig);
        sigaddset(&mask, SIGTERM);
        signal(SIGTERM, cleanup_upon_sig);
        sigaddset(&mask, SIGHUP);
        signal(SIGHUP, cleanup_upon_sig);
        pthread_sigmask(SIG_BLOCK, &mask, &oldmask);
        /*
         * This is a real-time compatible printf() package from
         * Xenomai's RT Development Kit (RTDK), that does NOT cause
         * any transition to secondary (i.e. non real-time) mode when
         * writing output.
         */
        rt_print_auto_init(1);
        pthread_attr_init(&rtattr);
        pthread_attr_setdetachstate(&rtattr, PTHREAD_CREATE_JOINABLE);
        pthread_attr_setinheritsched(&rtattr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&rtattr, SCHED_FIFO);
        pthread_attr_setschedparam(&rtattr, &rtparam);
        errno = pthread_create(&rt1, &rtattr, &realtime_thread1, NULL);
        if (errno)
                fail("pthread_create rt1");
        errno = pthread_create(&rt2, &rtattr, &realtime_thread2, NULL);
        if (errno)
                fail("pthread_create rt2");
        sigsuspend(&oldmask);
        return 0;
}