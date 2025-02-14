/* sim_ether.h: OS-dependent network information
  ------------------------------------------------------------------------------

   Copyright (c) 2002-2005, David T. Hittner

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.

  ------------------------------------------------------------------------------

  Modification history:

  17-Nov-11  MP   Added dynamic loading of libpcap on *nix platforms
  30-Oct-11  MP   Added support for vde (Virtual Distributed Ethernet) networking
  18-Apr-11  MP   Fixed race condition with self loopback packets in 
                  multithreaded environments
  09-Dec-10  MP   Added support to determine if network address conflicts exist
  07-Dec-10  MP   Reworked DECnet self detection to the more general approach
                  of loopback self when any Physical Address is being set.
  04-Dec-10  MP   Changed eth_write to do nonblocking writes when 
                  USE_READER_THREAD is defined.
  07-Feb-08  MP   Added eth_show_dev to display ethernet state
  28-Jan-08  MP   Added eth_set_async
  23-Jan-08  MP   Added eth_packet_trace_ex and ethq_destroy
  30-Nov-05  DTH  Added CRC length to packet and more field comments
  04-Feb-04  DTH  Added debugging information
  14-Jan-04  MP   Generalized BSD support issues
  05-Jan-04  DTH  Added eth_mac_scan
  26-Dec-03  DTH  Added ethernet show and queue functions from pdp11_xq
  23-Dec-03  DTH  Added status to packet
  01-Dec-03  DTH  Added reflections, tweaked decnet fix items
  25-Nov-03  DTH  Verified DECNET_FIX, reversed ifdef to mainstream code
  14-Nov-03  DTH  Added #ifdef DECNET_FIX for problematic duplicate detection code
  07-Jun-03  MP   Added WIN32 support for DECNET duplicate address detection.
  05-Jun-03  DTH  Added used to struct eth_packet
  01-Feb-03  MP   Changed some uint8 strings to char* to reflect usage 
  22-Oct-02  DTH  Added all_multicast and promiscuous support
  21-Oct-02  DTH  Corrected copyright again
  16-Oct-02  DTH  Fixed copyright
  08-Oct-02  DTH  Integrated with 2.10-0p4, added variable vector and copyrights
  03-Oct-02  DTH  Beta version of xq/sim_ether released for SIMH 2.09-11
  15-Aug-02  DTH  Started XQ simulation

  ------------------------------------------------------------------------------
*/

#ifndef _SIM_ETHER_H
#define _SIM_ETHER_H

#include "sim_defs.h"

// quick development-time plug
#ifndef DONT_USE_READER_THREAD
//#  define DONT_USE_READER_THREAD
#endif

/* make common BSD code a bit easier to read in this file */
/* OS/X seems to define and compile using one of these BSD types */
#if defined(__NetBSD__) || defined (__OpenBSD__) || defined (__FreeBSD__)
#  define xBSD 1
#endif

#if !defined(__FreeBSD__) && !defined(_WIN32) && !defined(VMS) && !defined(__APPLE__)
#  define USE_SETNONBLOCK 1
#endif

#if (((defined(__sun__) && defined(__i386__)) || defined(__linux) || defined(__APPLE__)) && !defined(DONT_USE_READER_THREAD))
#  define USE_READER_THREAD 1
#endif

#if defined(DONT_USE_READER_THREAD) && defined(USE_READER_THREAD)
#  undef USE_READER_THREAD
#endif

/* make common winpcap code a bit easier to read in this file */
#if defined(_WIN32) || defined(VMS)
#  define PCAP_READ_TIMEOUT -1
#else
#  define PCAP_READ_TIMEOUT  1
#endif

/* set related values to have correct relationships */
#if defined (USE_READER_THREAD)
#  if defined (USE_SETNONBLOCK)
#    undef USE_SETNONBLOCK
#  endif /* USE_SETNONBLOCK */
#  undef PCAP_READ_TIMEOUT
#  define PCAP_READ_TIMEOUT 15
#  if (!defined (xBSD) && !defined(_WIN32) && !defined(VMS)) || defined (USE_TAP_NETWORK) || defined (USE_VDE_NETWORK)
#    define MUST_DO_SELECT 1
#  endif
#endif /* USE_READER_THREAD */

/* give priority to USE_NETWORK over USE_SHARED */
#if defined(USE_NETWORK) && defined(USE_SHARED)
#  undef USE_SHARED
#endif

/* USE_SHARED only works on Windows or if HAVE_DLOPEN */
#if defined(USE_SHARED) && !defined(_WIN32) && !defined(HAVE_DLOPEN)
#  undef USE_SHARED
#endif

/*
  USE_BPF is defined to let this code leverage the libpcap/OS kernel provided 
  BPF packet filtering.  This generally will enhance performance.  It may not 
  be available in some environments and/or it may not work correctly, so 
  undefining this will still provide working code here.
*/
#define USE_BPF 1

/* structure declarations */

#define ETH_PROMISC            1                        /* promiscuous mode = true */
#define ETH_TIMEOUT           -1                        /* read timeout in milliseconds (immediate) */
#define ETH_FILTER_MAX        20                        /* maximum address filters */
#define ETH_DEV_NAME_MAX     256                        /* maximum device name size */
#define ETH_DEV_DESC_MAX     256                        /* maximum device description size */
#define ETH_MIN_PACKET        60                        /* minimum ethernet packet size */
#define ETH_MAX_PACKET      1514                        /* maximum ethernet packet size */
#define ETH_MAX_JUMBO_FRAME 65536                       /* maximum ethernet jumbo frame size (or Offload Segment Size) */
#define ETH_MAX_DEVICE        20                        /* maximum ethernet devices */
#define ETH_CRC_SIZE           4                        /* ethernet CRC size */
#define ETH_FRAME_SIZE (ETH_MAX_PACKET+ETH_CRC_SIZE)    /* ethernet maximum frame size */
#define ETH_MIN_JUMBO_FRAME ETH_MAX_PACKET              /* Threshold size for Jumbo Frame Processing */

#define LOOPBACK_SELF_FRAME(phy_mac, msg)             \
    (((msg)[12] == 0x90) && ((msg)[13] == 0x00) &&    \
     ((msg)[14] == 0x00) && ((msg)[15] == 0x00) &&    \
     ((msg)[16] == 0x02) && ((msg)[17] == 0x00) &&    \
     ((msg)[24] == 0x01) && ((msg)[25] == 0x00) &&    \
     (memcmp(phy_mac, (msg),    6) == 0) &&           \
     (memcmp(phy_mac, (msg)+6,  6) == 0) &&           \
     (memcmp(phy_mac, (msg)+18, 6) == 0))

#define LOOPBACK_PHYSICAL_RESPONSE(host_phy, phy_mac, msg) \
    (((msg)[12] == 0x90) && ((msg)[13] == 0x00) &&         \
     ((msg)[14] == 0x08) && ((msg)[15] == 0x00) &&         \
     ((msg)[16] == 0x02) && ((msg)[17] == 0x00) &&         \
     ((msg)[24] == 0x01) && ((msg)[25] == 0x00) &&         \
     (memcmp(host_phy, (msg)+18, 6) == 0) &&               \
     (memcmp(host_phy, (msg),    6) == 0) &&               \
     (memcmp(phy_mac,  (msg)+6,  6) == 0))

#define LOOPBACK_PHYSICAL_REFLECTION(host_phy, msg)  \
    (((msg)[12] == 0x90) && ((msg)[13] == 0x00) &&   \
     ((msg)[14] == 0x00) && ((msg)[15] == 0x00) &&   \
     ((msg)[16] == 0x02) && ((msg)[17] == 0x00) &&   \
     ((msg)[24] == 0x01) && ((msg)[25] == 0x00) &&   \
     (memcmp(host_phy, (msg)+6,  6) == 0) &&         \
     (memcmp(host_phy, (msg)+18, 6) == 0))

struct eth_packet {
  uint8   msg[ETH_FRAME_SIZE];                          /* ethernet frame (message) */
  int     len;                                          /* packet length without CRC */
  int     used;                                         /* bytes processed (used in packet chaining) */
  int     status;                                       /* transmit/receive status */
  int     crc_len;                                      /* packet length with CRC */
};

struct eth_item {
  int                 type;                             /* receive (0=setup, 1=loopback, 2=normal) */
  struct eth_packet   packet;
};

struct eth_queue {
  int                 max;
  int                 count;
  int                 head;
  int                 tail;
  int                 loss;
  int                 high;
  struct eth_item*    item;
};

struct eth_list {
  char    name[ETH_DEV_NAME_MAX];
  char    desc[ETH_DEV_DESC_MAX];
};

typedef int ETH_BOOL;
typedef unsigned char ETH_MAC[6];
typedef unsigned char ETH_MULTIHASH[8];
typedef struct eth_packet  ETH_PACK;
typedef void (*ETH_PCALLBACK)(int status);
typedef struct eth_list ETH_LIST;
typedef struct eth_queue ETH_QUE;
typedef struct eth_item ETH_ITEM;

#if defined (USE_READER_THREAD)
struct eth_write_request
{
    struct eth_write_request* next;
    ETH_PACK packet;
};
#endif

struct eth_device {
  char*         name;                                   /* name of ethernet device */
  void*         handle;                                 /* handle of implementation-specific device */
  int           fd_handle;                              /* fd to kernel device (where needed) */
  int           eth_api;                                /* Designator for which API is being used to move packets */
#define ETH_API_PCAP 0                                  /* Pcap API in use */
#define ETH_API_TAP  1                                  /* tun/tap API in use */
#define ETH_API_VDE  2                                  /* VDE API in use */
  ETH_PCALLBACK read_callback;                          /* read callback function */
  ETH_PCALLBACK write_callback;                         /* write callback function */
  ETH_PACK*     read_packet;                            /* read packet */
  ETH_MAC       filter_address[ETH_FILTER_MAX];         /* filtering addresses */
  int           addr_count;                             /* count of filtering addresses */
  ETH_BOOL      promiscuous;                            /* promiscuous mode flag */
  ETH_BOOL      all_multicast;                          /* receive all multicast messages */
  ETH_BOOL      hash_filter;                            /* filter using AUTODIN II multicast hash */
  ETH_MULTIHASH hash;                                   /* AUTODIN II multicast hash */
  int32         loopback_self_sent;                     /* loopback packets sent but not seen */
  int32         loopback_self_sent_total;               /* total loopback packets sent */
  int32         loopback_self_rcvd_total;               /* total loopback packets seen */
  ETH_MAC       physical_addr;                          /* physical address of interface */
  int32         have_host_nic_phy_addr;                 /* flag indicating that the host_nic_phy_hw_addr is valid */
  ETH_MAC       host_nic_phy_hw_addr;                   /* MAC address of the attached NIC */
  uint32        jumbo_fragmented;                       /* Giant IPv4 Frames Fragmented */
  uint32        jumbo_dropped;                          /* Giant Frames Dropped */
  uint32        jumbo_truncated;                        /* Giant Frames too big for capture buffer - Dropped */
  DEVICE*       dptr;                                   /* device ethernet is attached to */
  uint32        dbit;                                   /* debugging bit */
  int           reflections;                            /* packet reflections on interface */
  int           need_crc;                               /* device needs CRC (Cyclic Redundancy Check) */
#if defined (USE_READER_THREAD)
  t_bool        asynch_io;                              /* Asynchronous Interrupt scheduling enabled */
  int32         asynch_io_latency;
  ETH_QUE       read_queue;
  smp_thread_t  reader_thread;
  t_bool        reader_thread_created;
  smp_thread_t  writer_thread;
  t_bool        writer_thread_created;
  smp_lock*     lock;
  smp_lock*     writer_lock;
  smp_lock*     self_lock;
  smp_event*    writer_cond;
  struct eth_write_request* write_requests;
  int write_queue_peak;
  struct eth_write_request* write_buffers;
  t_stat write_status;
#endif
};

typedef struct eth_device  ETH_DEV;

/* prototype declarations*/

t_stat eth_open   (ETH_DEV* dev, char* name,            /* open ethernet interface */
                   DEVICE* dptr, uint32 dbit);
t_stat eth_close  (ETH_DEV* dev);                       /* close ethernet interface */
t_stat eth_write  (ETH_DEV* dev, ETH_PACK* packet,      /* write sychronous packet; */
                   ETH_PCALLBACK routine);              /*  callback when done */
int eth_read      (ETH_DEV* dev, ETH_PACK* packet,      /* read single packet; */
                   ETH_PCALLBACK routine);              /*  callback when done*/
t_stat eth_filter (ETH_DEV* dev, int addr_count,        /* set filter on incoming packets */
                   ETH_MAC* const addresses,
                   ETH_BOOL all_multicast,
                   ETH_BOOL promiscuous);
t_stat eth_filter_hash (ETH_DEV* dev, int addr_count,   /* set filter on incoming packets with AUTODIN II based hash */
                        ETH_MAC* const addresses,
                        ETH_BOOL all_multicast,
                        ETH_BOOL promiscuous,
                        ETH_MULTIHASH* const hash);
t_stat eth_check_address_conflict (ETH_DEV* dev, 
                                   ETH_MAC* const address);
int eth_devices   (int max, ETH_LIST* dev);             /* get ethernet devices on host */
void eth_setcrc   (ETH_DEV* dev, int need_crc);         /* enable/disable CRC mode */
t_stat eth_set_async (ETH_DEV* dev, int latency);       /* set read behavior to be async */
t_stat eth_clr_async (ETH_DEV* dev);                    /* set read behavior to be not async */
uint32 eth_crc32(uint32 crc, const void* vbuf, size_t len); /* Compute Ethernet Autodin II CRC for buffer */

void eth_packet_trace (ETH_DEV* dev, const uint8 *msg, int len, char* txt); /* trace ethernet packet header+crc */
void eth_packet_trace_ex (ETH_DEV* dev, const uint8 *msg, int len, const char* txt, int detail, uint32 reason); /* trace ethernet packet */
t_stat eth_show  (SMP_FILE* st, UNIT* uptr,             /* show ethernet devices */
                  int32 val, void* desc);
void eth_show_dev (SMP_FILE*st, ETH_DEV* dev);          /* show ethernet device state */

void eth_mac_fmt      (ETH_MAC* add, char* buffer);     /* format ethernet mac address */
t_stat eth_mac_scan (ETH_MAC* mac, char* strmac);       /* scan string for mac, put in mac */

t_stat ethq_init (ETH_QUE* que, int max);               /* initialize FIFO queue */
void ethq_clear  (ETH_QUE* que);                        /* clear FIFO queue */
void ethq_remove (ETH_QUE* que);                        /* remove item from FIFO queue */
void ethq_insert (ETH_QUE* que, int32 type,             /* insert item into FIFO queue */
                  ETH_PACK* packet, int32 status);
void ethq_insert_data(ETH_QUE* que, int32 type,         /* insert item into FIFO queue */
                  const uint8 *data, int used, int len, 
                  int crc_len, const uint8 *crc_data, int32 status);
t_stat ethq_destroy(ETH_QUE* que);                      /* release FIFO queue */


#endif                                                  /* _SIM_ETHER_H */
